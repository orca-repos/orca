// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "icore.hpp"
#include "windowsupport.hpp"
#include "dialogs/settingsdialog.hpp"

#include <app/app_version.hpp>
#include <extensionsystem/pluginmanager.hpp>

#include <utils/qtcassert.hpp>
#include <utils/algorithm.hpp>

#include <QApplication>
#include <QDebug>
#include <QStandardPaths>

/*!
    \namespace Core
    \inmodule Orca
    \brief The Core namespace contains all classes that make up the Core plugin
    which constitute the basic functionality of \QC.
*/

/*!
    \enum Core::FindFlag
    This enum holds the find flags.

    \value FindBackward
           Searches backwards.
    \value FindCaseSensitively
           Considers case when searching.
    \value FindWholeWords
           Finds only whole words.
    \value FindRegularExpression
           Uses a regular epression as a search term.
    \value FindPreserveCase
           Preserves the case when replacing search terms.
*/

/*!
    \enum Core::ICore::ContextPriority

    This enum defines the priority of additional contexts.

    \value High
           Additional contexts that have higher priority than contexts from
           Core::IContext instances.
    \value Low
           Additional contexts that have lower priority than contexts from
           Core::IContext instances.

    \sa Core::ICore::updateAdditionalContexts()
*/

/*!
    \enum Core::SaveSettingsReason
    \internal
*/

/*!
    \namespace Core::Internal
    \internal
*/

/*!
    \class Core::ICore
    \inheaderfile coreplugin/icore.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The ICore class allows access to the different parts that make up
    the basic functionality of \QC.

    You should never create a subclass of this interface. The one and only
    instance is created by the Core plugin. You can access this instance
    from your plugin through instance().
*/

/*!
    \fn void Core::ICore::coreAboutToOpen()

    Indicates that all plugins have been loaded and the main window is about to
    be shown.
*/

/*!
    \fn void Core::ICore::coreOpened()
    Indicates that all plugins have been loaded and the main window is shown.
*/

/*!
    \fn void Core::ICore::saveSettingsRequested(Core::ICore::SaveSettingsReason reason)
    Signals that the user has requested that the global settings
    should be saved to disk for a \a reason.

    At the moment that happens when the application is closed, and on \uicontrol{Save All}.
*/

/*!
    \fn void Core::ICore::coreAboutToClose()
    Enables plugins to perform some pre-end-of-life actions.

    The application is guaranteed to shut down after this signal is emitted.
    It is there as an addition to the usual plugin lifecycle functions, namely
    \c IPlugin::aboutToShutdown(), just for convenience.
*/

/*!
    \fn void Core::ICore::contextAboutToChange(const QList<Core::IContext *> &context)
    Indicates that a new \a context will shortly become the current context
    (meaning that its widget got focus).
*/

/*!
    \fn void Core::ICore::contextChanged(const Core::Context &context)
    Indicates that a new \a context just became the current context. This includes the context
    from the focus object as well as the additional context.
*/

#include "dialogs/newdialogwidget.hpp"
#include "dialogs/newdialog.hpp"
#include "iwizardfactory.hpp"
#include "mainwindow.hpp"
#include "documentmanager.hpp"

#include <utils/hostosinfo.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>

using namespace Core::Internal;
using namespace ExtensionSystem;
using namespace Utils;

namespace Core {

// The Core Singleton
static ICore *m_instance = nullptr;
static MainWindow *m_mainwindow = nullptr;

static auto defaultDialogFactory(QWidget *parent) -> NewDialog*
{
  return new NewDialogWidget(parent);
}

static std::function m_new_dialog_factory = defaultDialogFactory;

/*!
    Returns the pointer to the instance. Only use for connecting to signals.
*/
auto ICore::instance() -> ICore*
{
  return m_instance;
}

/*!
    Returns whether the new item dialog is currently open.
*/
auto ICore::isNewItemDialogRunning() -> bool
{
  return NewDialog::currentDialog() || IWizardFactory::isWizardRunning();
}

/*!
    Returns the currently open new item dialog widget, or \c nullptr if there is none.

    \sa isNewItemDialogRunning()
    \sa showNewItemDialog()
*/
auto ICore::newItemDialog() -> QWidget*
{
  if (NewDialog::currentDialog())
    return NewDialog::currentDialog();
  return IWizardFactory::currentWizard();
}

/*!
    \internal
*/
ICore::ICore(MainWindow *mainwindow)
{
  m_instance = this;
  m_mainwindow = mainwindow;

  // Save settings once after all plugins are initialized:
  connect(PluginManager::instance(), &PluginManager::initializationDone, this, [] { saveSettings(InitializationDone); });

  connect(PluginManager::instance(), &PluginManager::testsFinished, [this](const int failed_tests) {
    emit coreAboutToClose();
    if (failed_tests != 0)
      qWarning("Test run was not successful: %d test(s) failed.", failed_tests);
    QCoreApplication::exit(failed_tests);
  });

  connect(PluginManager::instance(), &PluginManager::scenarioFinished, [this](const int exit_code) {
    emit coreAboutToClose();
    QCoreApplication::exit(exit_code);
  });

  FileUtils::setDialogParentGetter(&ICore::dialogParent);
}

/*!
    \internal
*/
ICore::~ICore()
{
  m_instance = nullptr;
  m_mainwindow = nullptr;
}

/*!
    Opens a dialog where the user can choose from a set of \a factories that
    create new files or projects.

    The \a title argument is shown as the dialog title. The path where the
    files will be created (if the user does not change it) is set
    in \a defaultLocation. Defaults to DocumentManager::projectsDirectory()
    or DocumentManager::fileDialogLastVisitedDirectory(), depending on wizard
    kind.

    Additional variables for the wizards are set in \a extraVariables.

    \sa Core::DocumentManager
    \sa isNewItemDialogRunning()
    \sa newItemDialog()
*/
auto ICore::showNewItemDialog(const QString &title, const QList<IWizardFactory*> &factories, const FilePath &default_location, const QVariantMap &extra_variables) -> void
{
  QTC_ASSERT(!isNewItemDialogRunning(), return);

  /* This is a workaround for QDS: In QDS, we currently have a "New Project" dialog box but we do
   * not also have a "New file" dialog box (yet). Therefore, when requested to add a new file, we
   * need to use Orca's dialog box. In QDS, if `factories` contains project wizard factories
   * (even though it may contain file wizard factories as well), then we consider it to be a
   * request for "New Project". Otherwise, if we only have file wizard factories, we defer to
   * Orca's dialog and request "New File"
   */
  auto dialog_factory = m_new_dialog_factory;

  if (const auto have_project_wizards = anyOf(factories, [](const IWizardFactory *f) { return f->kind() == IWizardFactory::ProjectWizard; }); !have_project_wizards)
    dialog_factory = defaultDialogFactory;

  const auto new_dialog = dialog_factory(dialogParent());
  connect(new_dialog->widget(), &QObject::destroyed, m_instance, &ICore::updateNewItemDialogState);

  new_dialog->setWizardFactories(factories, default_location, extra_variables);
  new_dialog->setWindowTitle(title);
  new_dialog->showDialog();

  updateNewItemDialogState();
}

/*!
    Opens the options dialog on the specified \a page. The dialog's \a parent
    defaults to dialogParent(). If the dialog is already shown when this method
    is called, it is just switched to the specified \a page.

    Returns whether the user accepted the dialog.

    \sa msgShowOptionsDialog()
    \sa msgShowOptionsDialogToolTip()
*/
auto ICore::showOptionsDialog(const Id page, QWidget *parent) -> bool
{
  return executeSettingsDialog(parent ? parent : dialogParent(), page);
}

/*!
    Returns the text to use on buttons that open the options dialog.

    \sa showOptionsDialog()
    \sa msgShowOptionsDialogToolTip()
*/
auto ICore::msgShowOptionsDialog() -> QString
{
  return QCoreApplication::translate("Core", "Configure...", "msgShowOptionsDialog");
}

/*!
    Returns the tool tip to use on buttons that open the options dialog.

    \sa showOptionsDialog()
    \sa msgShowOptionsDialog()
*/
auto ICore::msgShowOptionsDialogToolTip() -> QString
{
  if constexpr (HostOsInfo::isMacHost())
    return QCoreApplication::translate("Core", "Open Preferences dialog.", "msgShowOptionsDialogToolTip (mac version)");
  else
    return QCoreApplication::translate("Core", "Open Options dialog.", "msgShowOptionsDialogToolTip (non-mac version)");
}

/*!
    Creates a message box with \a parent that contains a \uicontrol Configure
    button for opening the settings page specified by \a settingsId.

    The dialog has \a title and displays the message \a text and detailed
    information specified by \a details.

    Use this function to display configuration errors and to point users to the
    setting they should fix.

    Returns \c true if the user accepted the settings dialog.

    \sa showOptionsDialog()
*/
auto ICore::showWarningWithOptions(const QString &title, const QString &text, const QString &details, const Id settings_id, QWidget *parent) -> bool
{
  if (!parent)
    parent = m_mainwindow;

  QMessageBox msg_box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);

  if (!details.isEmpty())
    msg_box.setDetailedText(details);

  const QAbstractButton *settings_button = nullptr;

  if (settings_id.isValid())
    settings_button = msg_box.addButton(msgShowOptionsDialog(), QMessageBox::AcceptRole);

  msg_box.exec();

  if (settings_button && msg_box.clickedButton() == settings_button)
    return showOptionsDialog(settings_id);

  return false;
}

/*!
    Returns the application's main settings object.

    You can use it to retrieve or set application-wide settings
    (in contrast to session or project specific settings).

    If \a scope is \c QSettings::UserScope (the default), the
    settings will be read from the user's settings, with
    a fallback to global settings provided with \QC.

    If \a scope is \c QSettings::SystemScope, only the installation settings
    shipped with the current version of \QC will be read. This
    functionality exists for internal purposes only.

    \sa settingsDatabase()
*/
auto ICore::settings(const QSettings::Scope scope) -> QtcSettings*
{
  if (scope == QSettings::UserScope)
    return PluginManager::settings();
  return PluginManager::globalSettings();
}

/*!
    Returns the application's settings database.

    The settings database is meant as an alternative to the regular settings
    object. It is more suitable for storing large amounts of data. The settings
    are application wide.

    \sa SettingsDatabase
    \sa settings()
*/
auto ICore::settingsDatabase() -> SettingsDatabase*
{
  return m_mainwindow->settingsDatabase();
}

/*!
    Returns the application's printer object.

    Always use this printer object for printing, so the different parts of the
    application re-use its settings.
*/
auto ICore::printer() -> QPrinter*
{
  return m_mainwindow->printer();
}

/*!
    Returns the locale string for the user interface language that is currently
    configured in \QC. Use this to install your plugin's translation file with
    QTranslator.
*/
auto ICore::userInterfaceLanguage() -> QString
{
  return qApp->property("qtc_locale").toString();
}

static auto pathHelper(const QString &rel) -> QString
{
  if (rel.isEmpty())
    return rel;

  if (rel.startsWith('/'))
    return rel;

  return '/' + rel;
}

/*!
    Returns the absolute path for the relative path \a rel that is used for resources like
    project templates and the debugger macros.

    This abstraction is needed to avoid platform-specific code all over
    the place, since on \macos, for example, the resources are part of the
    application bundle.

    \sa userResourcePath()
*/
auto ICore::resourcePath(const QString &rel) -> FilePath
{
  return FilePath::fromString(QDir::cleanPath(QCoreApplication::applicationDirPath() + '/' + RELATIVE_DATA_PATH)) / rel;
}

/*!
    Returns the absolute path for the relative path \a rel in the users directory that is used for
    resources like project templates.

    Use this function for finding the place for resources that the user may
    write to, for example, to allow for custom palettes or templates.

    \sa resourcePath()
*/

auto ICore::userResourcePath(const QString &rel) -> FilePath
{
  // Create orca dir if it doesn't yet exist
  const auto config_dir = QFileInfo(settings(QSettings::UserScope)->fileName()).path();
  const QString urp = config_dir + '/' + QLatin1String(Constants::IDE_ID);

  if (!QFileInfo::exists(urp + QLatin1Char('/'))) {
    if (const QDir dir; !dir.mkpath(urp))
      qWarning() << "could not create" << urp;
  }

  return FilePath::fromString(urp + pathHelper(rel));
}

/*!
    Returns a writable path for the relative path \a rel that can be used for persistent cache files.
*/
auto ICore::cacheResourcePath(const QString &rel) -> FilePath
{
  return FilePath::fromString(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + pathHelper(rel));
}

/*!
    Returns the path, based on the relative path \a rel, to resources written by the installer,
    for example pre-defined kits and toolchains.
*/
auto ICore::installerResourcePath(const QString &rel) -> FilePath
{
  return FilePath::fromString(settings(QSettings::SystemScope)->fileName()).parentDir() / Constants::IDE_ID / rel;
}

/*!
    Returns the path to the plugins that are included in the \QC installation.

    \internal
*/
auto ICore::pluginPath() -> QString
{
  return QDir::cleanPath(QCoreApplication::applicationDirPath() + '/' + RELATIVE_PLUGIN_PATH);
}

/*!
    Returns the path where user-specific plugins should be written.

    \internal
*/
auto ICore::userPluginPath() -> QString
{
  auto plugin_path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

  if constexpr (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost())
    plugin_path += "/data";

  plugin_path += '/' + QLatin1String(Constants::IDE_SETTINGSVARIANT_STR) + '/';
  plugin_path += QLatin1String(HostOsInfo::isMacHost() ? Constants::IDE_DISPLAY_NAME : Constants::IDE_ID);
  plugin_path += "/plugins/";
  plugin_path += QString::number(IDE_VERSION_MAJOR) + '.' + QString::number(IDE_VERSION_MINOR) + '.' + QString::number(IDE_VERSION_RELEASE);

  return plugin_path;
}

/*!
    Returns the path, based on the relative path \a rel, to the command line tools that are
    included in the \QC installation.
 */
auto ICore::libexecPath(const QString &rel) -> FilePath
{
  return FilePath::fromString(QDir::cleanPath(QApplication::applicationDirPath() + pathHelper(RELATIVE_LIBEXEC_PATH))) / rel;
}

auto ICore::crashReportsPath() -> FilePath
{
  if constexpr (HostOsInfo::isMacHost())
    return libexecPath("crashpad_reports/completed");
  else
    return libexecPath("crashpad_reports/reports");
}

auto ICore::ideDisplayName() -> QString
{
  return Constants::IDE_DISPLAY_NAME;
}

static auto clangIncludePath(const QString &clang_version) -> QString
{
  return "/lib/clang/" + clang_version + "/include";
}

/*!
    \internal
*/
auto ICore::clangIncludeDirectory(const QString &clang_version, const FilePath &clang_fallback_include_dir) -> FilePath
{
  auto dir = libexecPath("clang" + clangIncludePath(clang_version));

  if (!dir.exists() || !dir.pathAppended("stdint.hpp").exists())
    dir = clang_fallback_include_dir;

  return dir.canonicalPath();
}

/*!
    \internal
*/
static auto clangBinary(const QString &binary_base_name, const FilePath &clang_bin_directory) -> FilePath
{
  auto executable = ICore::libexecPath("clang/bin").pathAppended(binary_base_name).withExecutableSuffix();

  if (!executable.exists())
    executable = clang_bin_directory.pathAppended(binary_base_name).withExecutableSuffix();

  return executable.canonicalPath();
}

/*!
    \internal
*/
auto ICore::clangExecutable(const FilePath &clang_bin_directory) -> FilePath
{
  return clangBinary("clang", clang_bin_directory);
}

/*!
    \internal
*/
auto ICore::clangdExecutable(const FilePath &clang_bin_directory) -> FilePath
{
  return clangBinary("clangd", clang_bin_directory);
}

/*!
    \internal
*/
auto ICore::clangTidyExecutable(const FilePath &clang_bin_directory) -> FilePath
{
  return clangBinary("clang-tidy", clang_bin_directory);
}

/*!
    \internal
*/
auto ICore::clazyStandaloneExecutable(const FilePath &clang_bin_directory) -> FilePath
{
  return clangBinary("clazy-standalone", clang_bin_directory);
}

static auto compilerString() -> QString
{
  #if defined(Q_CC_CLANG) // must be before GNU, because clang claims to be GNU too
  QString platformSpecific;
  #if defined(__apple_build_version__) // Apple clang has other version numbers
  platformSpecific = QLatin1String(" (Apple)");
  #elif defined(Q_CC_MSVC)
  platformSpecific = QLatin1String(" (clang-cl)");
  #endif
  return QLatin1String("Clang " ) + QString::number(__clang_major__) + QLatin1Char('.')
    + QString::number(__clang_minor__) + platformSpecific;
  #elif defined(Q_CC_GNU)
  return QLatin1String("GCC " ) + QLatin1String(__VERSION__);
  #elif defined(Q_CC_MSVC)
  if constexpr (_MSC_VER > 1999)
    return QLatin1String("MSVC <unknown>");
  if constexpr (_MSC_VER >= 1930)
    return QLatin1String("MSVC 2022");
  if constexpr (_MSC_VER >= 1920)
    return QLatin1String("MSVC 2019");
  if constexpr (_MSC_VER >= 1910)
    return QLatin1String("MSVC 2017");
  if constexpr (_MSC_VER >= 1900)
    return QLatin1String("MSVC 2015");
  #endif
  return QLatin1String("<unknown compiler>");
}

/*!
    Returns a string with the IDE's name and version, in the form "\QC X.Y.Z".
    Use this for "Generated by" strings and similar tasks.
*/
auto ICore::versionString() -> QString
{
  QString ide_version_description;

  if (QLatin1String(Constants::IDE_VERSION_LONG) != QLatin1String(Constants::IDE_VERSION_DISPLAY))
    ide_version_description = tr(" (%1)").arg(QLatin1String(Constants::IDE_VERSION_LONG));

  return tr("%1 %2%3").arg(QLatin1String(Constants::IDE_DISPLAY_NAME), QLatin1String(Constants::IDE_VERSION_DISPLAY), ide_version_description);
}

/*!
    \internal
*/
auto ICore::buildCompatibilityString() -> QString
{
  return tr("Based on Qt %1 (%2, %3 bit)").arg(QLatin1String(qVersion()), compilerString(), QString::number(QSysInfo::WordSize));
}

/*!
    Returns the top level IContext of the current context, or \c nullptr if
    there is none.

    \sa updateAdditionalContexts()
    \sa addContextObject()
    \sa {The Action Manager and Commands}
*/
auto ICore::currentContextObject() -> IContext*
{
  return m_mainwindow->currentContextObject();
}

/*!
    Returns the widget of the top level IContext of the current context, or \c
    nullptr if there is none.

    \sa currentContextObject()
*/
auto ICore::currentContextWidget() -> QWidget*
{
  const auto context = currentContextObject();
  return context ? context->widget() : nullptr;
}

/*!
    Returns the registered IContext instance for the specified \a widget,
    if any.
*/
auto ICore::contextObject(QWidget *widget) -> IContext*
{
  return m_mainwindow->contextObject(widget);
}

/*!
    Returns the main window of the application.

    For dialog parents use dialogParent().

    \sa dialogParent()
*/
auto ICore::mainWindow() -> QMainWindow*
{
  return m_mainwindow;
}

/*!
    Returns a widget pointer suitable to use as parent for QDialogs.
*/
auto ICore::dialogParent() -> QWidget*
{
  auto active = QApplication::activeModalWidget();

  if (!active)
    active = QApplication::activeWindow();

  if (!active || (active && active->windowFlags().testFlag(Qt::SplashScreen)))
    active = m_mainwindow;

  return active;
}

/*!
    \internal
*/
auto ICore::statusBar() -> QStatusBar*
{
  return m_mainwindow->statusBar();
}

/*!
    Returns a central InfoBar that is shown in \QC's main window.
    Use for notifying the user of something without interrupting with
    dialog. Use sparingly.
*/
auto ICore::infoBar() -> InfoBar*
{
  return m_mainwindow->infoBar();
}

/*!
    Raises and activates the window for \a widget. This contains workarounds
    for X11.
*/
auto ICore::raiseWindow(const QWidget *widget) -> void
{
  if (!widget)
    return;

  if (const auto window = widget->window(); window && window == m_mainwindow) {
    m_mainwindow->raiseWindow();
  } else {
    window->raise();
    window->activateWindow();
  }
}

/*!
    Removes the contexts specified by \a remove from the list of active
    additional contexts, and adds the contexts specified by \a add with \a
    priority.

    The additional contexts are not associated with an IContext instance.

    High priority additional contexts have higher priority than the contexts
    added by IContext instances, low priority additional contexts have lower
    priority than the contexts added by IContext instances.

    \sa addContextObject()
    \sa {The Action Manager and Commands}
*/
auto ICore::updateAdditionalContexts(const Context &remove, const Context &add, const ContextPriority priority) -> void
{
  m_mainwindow->updateAdditionalContexts(remove, add, priority);
}

/*!
    Adds \a context with \a priority to the list of active additional contexts.

    \sa updateAdditionalContexts()
*/
auto ICore::addAdditionalContext(const Context &context, const ContextPriority priority) -> void
{
  m_mainwindow->updateAdditionalContexts(Context(), context, priority);
}

/*!
    Removes \a context from the list of active additional contexts.

    \sa updateAdditionalContexts()
*/
auto ICore::removeAdditionalContext(const Context &context) -> void
{
  m_mainwindow->updateAdditionalContexts(context, Context(), ContextPriority::Low);
}

/*!
    Adds \a context to the list of registered IContext instances.
    Whenever the IContext's \l{IContext::widget()}{widget} is in the application
    focus widget's parent hierarchy, its \l{IContext::context()}{context} is
    added to the list of active contexts.

    \sa removeContextObject()
    \sa updateAdditionalContexts()
    \sa currentContextObject()
    \sa {The Action Manager and Commands}
*/
auto ICore::addContextObject(IContext *context) -> void
{
  m_mainwindow->addContextObject(context);
}

/*!
    Unregisters a \a context object from the list of registered IContext
    instances. IContext instances are automatically removed when they are
    deleted.

    \sa addContextObject()
    \sa updateAdditionalContexts()
    \sa currentContextObject()
*/
auto ICore::removeContextObject(IContext *context) -> void
{
  m_mainwindow->removeContextObject(context);
}

/*!
    Registers a \a window with the specified \a context. Registered windows are
    shown in the \uicontrol Window menu and get registered for the various
    window related actions, like the minimize, zoom, fullscreen and close
    actions.

    Whenever the application focus is in \a window, its \a context is made
    active.
*/
auto ICore::registerWindow(QWidget *window, const Context &context) -> void
{
  new WindowSupport(window, context); // deletes itself when widget is destroyed
}

/*!
    Opens files using \a filePaths and \a flags like it would be
    done if they were given to \QC on the command line, or
    they were opened via \uicontrol File > \uicontrol Open.
*/

auto ICore::openFiles(const FilePaths &file_paths, const OpenFilesFlags flags) -> void
{
  MainWindow::openFiles(file_paths, flags);
}

/*!
    Provides a hook for plugins to veto on closing the application.

    When the application window requests a close, all listeners are called. If
    one of the \a listener calls returns \c false, the process is aborted and
    the event is ignored. If all calls return \c true, coreAboutToClose()
    is emitted and the event is accepted or performed.
*/
auto ICore::addPreCloseListener(const std::function<bool ()> &listener) -> void
{
  m_mainwindow->addPreCloseListener(listener);
}

/*!
    \internal
*/
auto ICore::systemInformation() -> QString
{
  QString result = PluginManager::systemInformation() + '\n';
  result += versionString() + '\n';
  result += buildCompatibilityString() + '\n';
  #ifdef IDE_REVISION
  result += QString("From revision %1\n").arg(QString::fromLatin1(Constants::IDE_REVISION_STR).left(10));
  #endif
  return result;
}

static auto screenShotsPath() -> const QByteArray&
{
  static const auto path = qgetenv("QTC_SCREENSHOTS_PATH");
  return path;
}

class ScreenShooter final : public QObject {
public:
  ScreenShooter(QWidget *widget, QString name, const QRect &rc) : m_widget(widget), m_name(std::move(name)), m_rc(rc)
  {
    m_widget->installEventFilter(this);
  }

  auto eventFilter(QObject *watched, QEvent *event) -> bool override
  {
    QTC_ASSERT(watched == m_widget, return false);

    if (event->type() == QEvent::Show)
      QMetaObject::invokeMethod(this, &ScreenShooter::helper, Qt::QueuedConnection);

    return false;
  }

  auto helper() -> void
  {
    if (m_widget) {
      const auto rc = m_rc.isValid() ? m_rc : m_widget->rect();
      const auto pm = m_widget->grab(rc);
      for (auto i = 0; ; ++i) {
        if (QString file_name = screenShotsPath() + '/' + m_name + QString("-%1.png").arg(i); !QFileInfo::exists(file_name)) {
          pm.save(file_name);
          break;
        }
      }
    }
    deleteLater();
  }

  QPointer<QWidget> m_widget;
  QString m_name;
  QRect m_rc;
};

/*!
    \internal
*/
auto ICore::setupScreenShooter(const QString &name, QWidget *w, const QRect &rc) -> void
{
  if (!screenShotsPath().isEmpty())
    new ScreenShooter(w, name, rc);
}

/*!
    Restarts \QC and restores the last session.
*/
auto ICore::restart() -> void
{
  m_mainwindow->restart();
}

/*!
    \internal
*/
auto ICore::saveSettings(const SaveSettingsReason reason) -> void
{
  emit m_instance->saveSettingsRequested(reason);
  m_mainwindow->saveSettings();
  settings(QSettings::SystemScope)->sync();
  settings(QSettings::UserScope)->sync();
}

/*!
    \internal
*/
auto ICore::additionalAboutInformation() -> QStringList
{
  return m_mainwindow->additionalAboutInformation();
}

/*!
    \internal
*/
auto ICore::appendAboutInformation(const QString &line) -> void
{
  m_mainwindow->appendAboutInformation(line);
}

auto ICore::updateNewItemDialogState() -> void
{
  static auto was_running = false;
  static QWidget *previous_dialog = nullptr;

  if (was_running == isNewItemDialogRunning() && previous_dialog == newItemDialog())
    return;

  was_running = isNewItemDialogRunning();
  previous_dialog = newItemDialog();

  emit instance()->newItemDialogStateChanged();
}

/*!
    \internal
*/
auto ICore::setNewDialogFactory(const std::function<NewDialog *(QWidget *)> &newFactory) -> void
{
  m_new_dialog_factory = newFactory;
}

} // namespace Core
