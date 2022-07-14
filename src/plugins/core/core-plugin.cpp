// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-plugin.hpp"

#include "core-action-manager.hpp"
#include "core-constants.hpp"
#include "core-design-mode.hpp"
#include "core-document-interface.hpp"
#include "core-document-manager.hpp"
#include "core-edit-mode.hpp"
#include "core-editor-manager.hpp"
#include "core-file-utils.hpp"
#include "core-find-plugin.hpp"
#include "core-folder-navigation-widget.hpp"
#include "core-help-manager.hpp"
#include "core-interface.hpp"
#include "core-locator.hpp"
#include "core-main-window.hpp"
#include "core-mode-manager.hpp"
#include "core-search-result-window.hpp"
#include "core-theme-chooser.hpp"
#include "core-wizard-factory-interface.hpp"

#include <app/app_version.hpp>

#include <extensionsystem/pluginerroroverview.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <extensionsystem/pluginspec.hpp>

#include <utils/algorithm.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/commandline.hpp>
#include <utils/infobar.hpp>
#include <utils/macroexpander.hpp>
#include <utils/pathchooser.hpp>
#include <utils/savefile.hpp>
#include <utils/stringutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/theme/theme.hpp>
#include <utils/theme/theme_p.hpp>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QUuid>

#include <cstdlib>

using namespace Utils;

namespace Orca::Plugin::Core {

static CorePlugin *m_instance = nullptr;

constexpr char kWarnCrashReportingSetting[] = "WarnCrashReporting";
constexpr char kEnvironmentChanges[] = "Core/EnvironmentChanges";

auto CorePlugin::setupSystemEnvironment() -> void
{
  m_instance->m_startup_system_environment = Environment::systemEnvironment();
  const auto changes = EnvironmentItem::fromStringList(ICore::settings()->value(kEnvironmentChanges).toStringList());
  setEnvironmentChanges(changes);
}

CorePlugin::CorePlugin()
{
  qRegisterMetaType<Id>();
  qRegisterMetaType<TextPosition>();
  qRegisterMetaType<CommandLine>();
  qRegisterMetaType<FilePath>();
  m_instance = this;
  setupSystemEnvironment();
}

CorePlugin::~CorePlugin()
{
  IWizardFactory::destroyFeatureProvider();
  Find::destroy();

  delete m_locator;
  delete m_folder_navigation_widget_factory;
  delete m_edit_mode;

  DesignMode::destroyModeIfRequired();

  delete m_main_window;
  setOrcaTheme(nullptr);
}

auto CorePlugin::instance() -> CorePlugin*
{
  return m_instance;
}

struct CoreArguments {
  QColor override_color;
  Id theme_id;
  bool presentation_mode = false;
};

auto parseArguments(const QStringList &arguments) -> CoreArguments
{
  CoreArguments args;

  for (auto i = 0; i < arguments.size(); ++i) {
    if (arguments.at(i) == QLatin1String("-color")) {
      const auto colorcode(arguments.at(i + 1));
      args.override_color = QColor(colorcode);
      i++; // skip the argument
    }

    if (arguments.at(i) == QLatin1String("-presentationMode"))
      args.presentation_mode = true;

    if (arguments.at(i) == QLatin1String("-theme")) {
      args.theme_id = Id::fromString(arguments.at(static_cast<qsizetype>(i) + 1));
      i++; // skip the argument
    }
  }

  return args;
}

auto CorePlugin::initialize(const QStringList &arguments, QString *error_message) -> bool
{
  // register all mime types from all plugins
  for (const auto plugin : ExtensionSystem::PluginManager::plugins()) {
    if (!plugin->isEffectivelyEnabled())
      continue;

    const auto meta_data = plugin->metaData();
    const auto mime_types = meta_data.value("Mimetypes");

    QString mime_type_string;
    if (readMultiLineString(mime_types, &mime_type_string))
      addMimeTypes(plugin->name() + ".mimetypes", mime_type_string.trimmed().toUtf8());
  }

  if (ThemeEntry::availableThemes().isEmpty()) {
    *error_message = tr("No themes found in installation.");
    return false;
  }
  const auto [override_color, theme_id, presentation_mode] = parseArguments(arguments);
  const auto theme_from_arg = ThemeEntry::createTheme(theme_id);
  const auto theme = theme_from_arg ? theme_from_arg : ThemeEntry::createTheme(ThemeEntry::themeSetting());

  Theme::setInitialPalette(theme); // Initialize palette before setting it
  setOrcaTheme(theme);

  InfoBar::initialize(ICore::settings());

  new ActionManager(this);
  ActionManager::setPresentationModeEnabled(presentation_mode);

  m_main_window = new MainWindow;
  if (override_color.isValid())
    m_main_window->setOverrideColor(override_color);

  m_locator = new Locator;

  std::srand(static_cast<unsigned>(QDateTime::currentDateTime().toSecsSinceEpoch()));

  m_main_window->init();
  m_edit_mode = new EditMode;

  ModeManager::activateMode(m_edit_mode->id());

  m_folder_navigation_widget_factory = new FolderNavigationWidgetFactory;

  IWizardFactory::initialize();
  SaveFile::initializeUmask();   // Make sure we respect the process's umask when creating new files
  Find::initialize();
  m_locator->initialize();

  const auto expander = globalMacroExpander();
  expander->registerVariable("CurrentDate:ISO", tr("The current date (ISO)."), [] { return QDate::currentDate().toString(Qt::ISODate); });
  expander->registerVariable("CurrentTime:ISO", tr("The current time (ISO)."), [] { return QTime::currentTime().toString(Qt::ISODate); });
  expander->registerVariable("CurrentDate:RFC", tr("The current date (RFC2822)."), [] { return QDate::currentDate().toString(Qt::RFC2822Date); });
  expander->registerVariable("CurrentTime:RFC", tr("The current time (RFC2822)."), [] { return QTime::currentTime().toString(Qt::RFC2822Date); });
  expander->registerVariable("CurrentDate:Locale", tr("The current date (Locale)."), [] { return QLocale::system().toString(QDate::currentDate(), QLocale::ShortFormat); });
  expander->registerVariable("CurrentTime:Locale", tr("The current time (Locale)."), [] { return QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat); });
  expander->registerVariable("Config:DefaultProjectDirectory", tr("The configured default directory for projects."), [] { return DocumentManager::projectsDirectory().toString(); });
  expander->registerVariable("Config:LastFileDialogDirectory", tr("The directory last visited in a file dialog."), [] { return DocumentManager::fileDialogLastVisitedDirectory().toString(); });
  expander->registerVariable("HostOs:isWindows", tr("Is %1 running on Windows?").arg(IDE_DISPLAY_NAME), [] { return QVariant(HostOsInfo::isWindowsHost()).toString(); });
  expander->registerVariable("HostOs:isOSX", tr("Is %1 running on OS X?").arg(IDE_DISPLAY_NAME), [] { return QVariant(HostOsInfo::isMacHost()).toString(); });
  expander->registerVariable("HostOs:isLinux", tr("Is %1 running on Linux?").arg(IDE_DISPLAY_NAME), [] { return QVariant(HostOsInfo::isLinuxHost()).toString(); });
  expander->registerVariable("HostOs:isUnix", tr("Is %1 running on any unix-based platform?").arg(IDE_DISPLAY_NAME), [] { return QVariant(HostOsInfo::isAnyUnixHost()).toString(); });
  expander->registerVariable("HostOs:PathListSeparator", tr("The path list separator for the platform."), [] { return QString(HostOsInfo::pathListSeparator()); });
  expander->registerVariable("HostOs:ExecutableSuffix", tr("The platform executable suffix."), [] { return QString(HostOsInfo::withExecutableSuffix("")); });
  expander->registerVariable("IDE:ResourcePath", tr("The directory where %1 finds its pre-installed resources.").arg(IDE_DISPLAY_NAME), [] { return ICore::resourcePath().toString(); });
  expander->registerPrefix("CurrentDate:", tr("The current date (QDate formatstring)."), [](const QString &fmt) { return QDate::currentDate().toString(fmt); });
  expander->registerPrefix("CurrentTime:", tr("The current time (QTime formatstring)."), [](const QString &fmt) { return QTime::currentTime().toString(fmt); });
  expander->registerVariable("UUID", tr("Generate a new UUID."), [] { return QUuid::createUuid().toString(); });
  expander->registerPrefix("#:", tr("A comment."), [](const QString &) { return QString(); });

  PathChooser::setAboutToShowContextMenuHandler(&CorePlugin::addToPathChooserContextMenu);

  #ifdef ENABLE_CRASHPAD
  connect(ICore::instance(), &ICore::coreOpened, this, &CorePlugin::warnAboutCrashReporing, Qt::QueuedConnection);
  #endif

  return true;
}

auto CorePlugin::extensionsInitialized() -> void
{
  DesignMode::createModeIfRequired();
  Find::extensionsInitialized();

  m_locator->extensionsInitialized();
  m_main_window->extensionsInitialized();

  if (ExtensionSystem::PluginManager::hasError()) {
    const auto error_overview = new ExtensionSystem::PluginErrorOverview(m_main_window);
    error_overview->setAttribute(Qt::WA_DeleteOnClose);
    error_overview->setModal(true);
    error_overview->show();
  }

  checkSettings();
}

auto CorePlugin::delayedInitialize() -> bool
{
  m_locator->delayedInitialize();
  IWizardFactory::allWizardFactories(); // scan for all wizard factories
  return true;
}

auto CorePlugin::remoteCommand(const QStringList & /* options */, const QString &working_directory, const QStringList &args) -> QObject*
{
  if (!ExtensionSystem::PluginManager::isInitializationDone()) {
    connect(ExtensionSystem::PluginManager::instance(), &ExtensionSystem::PluginManager::initializationDone, this, [this, working_directory, args] {
      remoteCommand(QStringList(), working_directory, args);
    });
    return nullptr;
  }

  const auto file_paths = transform(args, FilePath::fromUserInput);
  const auto res = MainWindow::openFiles(file_paths, static_cast<ICore::OpenFilesFlags>(ICore::SwitchMode | ICore::CanContainLineAndColumnNumbers | ICore::SwitchSplitIfAlreadyVisible), working_directory);

  m_main_window->raiseWindow();
  return res;
}

auto CorePlugin::startupSystemEnvironment() -> Environment
{
  return m_instance->m_startup_system_environment;
}

auto CorePlugin::environmentChanges() -> EnvironmentItems
{
  return m_instance->m_environment_changes;
}

auto CorePlugin::setEnvironmentChanges(const EnvironmentItems &changes) -> void
{
  if (m_instance->m_environment_changes == changes)
    return;

  m_instance->m_environment_changes = changes;
  auto system_env = m_instance->m_startup_system_environment;
  system_env.modify(changes);

  Environment::setSystemEnvironment(system_env);
  ICore::settings()->setValueWithDefault(kEnvironmentChanges, EnvironmentItem::toStringList(changes));

  if (ICore::instance())
    emit ICore::instance()->systemEnvironmentChanged();
}

auto CorePlugin::fileOpenRequest(const QString &f) -> void
{
  remoteCommand(QStringList(), QString(), QStringList(f));
}

auto CorePlugin::addToPathChooserContextMenu(PathChooser *path_chooser, QMenu *menu) -> void
{
  auto actions = menu->actions();
  const auto first_action = actions.isEmpty() ? nullptr : actions.first();

  if (QDir().exists(path_chooser->filePath().toString())) {
    auto *show_in_graphical_shell = new QAction(FileUtils::msgGraphicalShellAction(), menu);
    connect(show_in_graphical_shell, &QAction::triggered, path_chooser, [path_chooser] {
      FileUtils::showInGraphicalShell(path_chooser, path_chooser->filePath());
    });
    menu->insertAction(first_action, show_in_graphical_shell);
    auto *show_in_terminal = new QAction(FileUtils::msgTerminalHereAction(), menu);
    connect(show_in_terminal, &QAction::triggered, path_chooser, [path_chooser] {
      if (path_chooser->openTerminalHandler())
        path_chooser->openTerminalHandler()();
      else
        FileUtils::openTerminal(path_chooser->filePath());
    });
    menu->insertAction(first_action, show_in_terminal);
  } else {
    auto *mk_path_act = new QAction(tr("Create Folder"), menu);
    connect(mk_path_act, &QAction::triggered, path_chooser, [path_chooser] {
      QDir().mkpath(path_chooser->filePath().toString());
      path_chooser->triggerChanged();
    });
    menu->insertAction(first_action, mk_path_act);
  }

  if (first_action)
    menu->insertSeparator(first_action);
}

auto CorePlugin::checkSettings() -> void
{
  const auto show_msg_box = [this](const QString &msg, QMessageBox::Icon icon) {
    connect(ICore::instance(), &ICore::coreOpened, this, [msg, icon] {
      QMessageBox msg_box(ICore::dialogParent());
      msg_box.setWindowTitle(tr("Settings File Error"));
      msg_box.setText(msg);
      msg_box.setIcon(icon);
      msg_box.exec();
    }, Qt::QueuedConnection);
  };

  const QSettings *const user_settings = ICore::settings();
  QString error_details;

  switch (user_settings->status()) {
  case QSettings::NoError: {
    if (const QFileInfo fi(user_settings->fileName()); fi.exists() && !fi.isWritable()) {
      const auto error_msg = tr("The settings file \"%1\" is not writable.\n" "You will not be able to store any %2 settings.").arg(QDir::toNativeSeparators(user_settings->fileName()), QLatin1String(IDE_DISPLAY_NAME));
      show_msg_box(error_msg, QMessageBox::Warning);
    }
    return;
  }
  case QSettings::AccessError:
    error_details = tr("The file is not readable.");
    break;
  case QSettings::FormatError:
    error_details = tr("The file is invalid.");
    break;
  }

  const auto error_msg = tr("Error reading settings file \"%1\": %2\n" "You will likely experience further problems using this instance of %3.").arg(QDir::toNativeSeparators(user_settings->fileName()), error_details, QLatin1String(IDE_DISPLAY_NAME));
  show_msg_box(error_msg, QMessageBox::Critical);
}

auto CorePlugin::warnAboutCrashReporing() -> void
{
  if (!ICore::infoBar()->canInfoBeAdded(kWarnCrashReportingSetting))
    return;

  auto warn_str = ICore::settings()->value("CrashReportingEnabled", false).toBool() ? tr("%1 collects crash reports for the sole purpose of fixing bugs. " "To disable this feature go to %2.") : tr("%1 can collect crash reports for the sole purpose of fixing bugs. " "To enable this feature go to %2.");

  if constexpr (HostOsInfo::isMacHost()) {
    warn_str = warn_str.arg(QLatin1String(IDE_DISPLAY_NAME), IDE_DISPLAY_NAME + tr(" > Preferences > Environment > System"));
  } else {
    warn_str = warn_str.arg(QLatin1String(IDE_DISPLAY_NAME), tr("Edit > Preferences > Environment > System"));
  }

  InfoBarEntry info(kWarnCrashReportingSetting, warn_str, InfoBarEntry::GlobalSuppression::Enabled);
  info.addCustomButton(tr("Configure..."), [] {
    ICore::infoBar()->removeInfo(kWarnCrashReportingSetting);
    ICore::infoBar()->globallySuppressInfo(kWarnCrashReportingSetting);
    ICore::showOptionsDialog(SETTINGS_ID_SYSTEM);
  });

  info.setDetailsWidgetCreator([]() -> QWidget* {
    const auto label = new QLabel;
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    label->setText(msgCrashpadInformation());
    label->setContentsMargins(0, 0, 0, 8);
    return label;
  });

  ICore::infoBar()->addInfo(info);
}

// static
auto CorePlugin::msgCrashpadInformation() -> QString
{
  return tr("%1 uses Google Crashpad for collecting crashes and sending them to our backend " "for processing. Crashpad may capture arbitrary contents from crashed process’ " "memory, including user sensitive information, URLs, and whatever other content " "users have trusted %1 with. The collected crash reports are however only used " "for the sole purpose of fixing bugs.").arg(IDE_DISPLAY_NAME) + "<br><br>" + tr("More information:") + "<br><a href='https://chromium.googlesource.com/crashpad/crashpad/+/master/doc/" "overview_design.md'>" + tr("Crashpad Overview") + "</a>" "<br><a href='https://sentry.io/security/'>" + tr("%1 security policy").arg("Sentry.io") + "</a>";
}

auto CorePlugin::aboutToShutdown() -> ShutdownFlag
{
  Find::aboutToShutdown();
  const auto shutdown_flag = m_locator->aboutToShutdown([this] { emit asynchronousShutdownFinished(); });
  m_main_window->aboutToShutdown();
  return shutdown_flag;
}

} // namespace Orca::Plugin::Core
