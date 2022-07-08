// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mainwindow.hpp"
#include "icore.hpp"
#include "jsexpander.hpp"
#include "mimetypesettings.hpp"
#include "fancytabwidget.hpp"
#include "documentmanager.hpp"
#include "generalsettings.hpp"
#include "idocumentfactory.hpp"
#include "loggingviewer.hpp"
#include "messagemanager.hpp"
#include "modemanager.hpp"
#include "outputpanemanager.hpp"
#include "plugindialog.hpp"
#include "vcsmanager.hpp"
#include "versiondialog.hpp"
#include "statusbarmanager.hpp"
#include "manhattanstyle.hpp"
#include "navigationwidget.hpp"
#include "rightpane.hpp"
#include "editormanager/ieditorfactory.hpp"
#include "systemsettings.hpp"
#include "externaltoolmanager.hpp"
#include "editormanager/systemeditor.hpp"
#include "windowsupport.hpp"
#include "coreicons.hpp"

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/dialogs/externaltoolconfig.hpp>
#include <core/iwizardfactory.hpp>
#include <core/dialogs/shortcutsettings.hpp>
#include <core/editormanager/documentmodel_p.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/editormanager_p.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/inavigationwidgetfactory.hpp>
#include <core/progressmanager/progressmanager_p.hpp>
#include <core/progressmanager/progressview.hpp>
#include <core/settingsdatabase.hpp>

#include <utils/algorithm.hpp>
#include <utils/historycompleter.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/proxyaction.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>
#include <utils/stringutils.hpp>
#include <utils/touchbar/touchbar.hpp>
#include <utils/utilsicons.hpp>

#include <app/app_version.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrinter>
#include <QSettings>
#include <QStatusBar>
#include <QStyleFactory>
#include <QToolButton>
#include <QWindow>
#include <ranges>

using namespace ExtensionSystem;
using namespace Utils;

namespace Core {
namespace Internal {

enum {
  debugMainWindow = 0
};

MainWindow::MainWindow() : AppMainWindow(), m_core_impl(new ICore(this)), m_low_prio_additional_contexts(Constants::C_GLOBAL), m_settings_database(new SettingsDatabase(QFileInfo(PluginManager::settings()->fileName()).path(), QLatin1String(Constants::IDE_CASED_ID), this)), m_progress_manager(new ProgressManagerPrivate), m_js_expander(JsExpander::createGlobalJsExpander()), m_vcs_manager(new VcsManager), m_mode_stack(new FancyTabWidget(this)), m_general_settings(new GeneralSettings), m_system_settings(new SystemSettings), m_shortcut_settings(new ShortcutSettings), m_tool_settings(new ToolSettings), m_mime_type_settings(new MimeTypeSettings), m_system_editor(new SystemEditor), m_toggle_left_side_bar_button(new QToolButton), m_toggle_right_side_bar_button(new QToolButton)
{
  (void)new DocumentManager(this);

  HistoryCompleter::setSettings(PluginManager::settings());

  setWindowTitle(Constants::IDE_DISPLAY_NAME);

  if constexpr (HostOsInfo::isLinuxHost())
    QApplication::setWindowIcon(Icons::ORCALOGO_BIG.icon());

  auto base_name = QApplication::style()->objectName();

  // Sometimes we get the standard windows 95 style as a fallback
  if constexpr (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost() && base_name == QLatin1String("windows")) {
    base_name = QLatin1String("fusion");
  }

  // if the user has specified as base style in the theme settings,
  // prefer that
  const auto available = QStyleFactory::keys();
  for(const auto &s: orcaTheme()->preferredStyles()) {
    if (available.contains(s, Qt::CaseInsensitive)) {
      base_name = s;
      break;
    }
  }

  QApplication::setStyle(new ManhattanStyle(base_name));
  m_general_settings->setShowShortcutsInContextMenu(GeneralSettings::showShortcutsInContextMenu());

  setDockNestingEnabled(true);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

  m_mode_manager = new ModeManager(this, m_mode_stack);

  connect(m_mode_stack, &FancyTabWidget::topAreaClicked, this, [](Qt::MouseButton, const Qt::KeyboardModifiers modifiers) {
    if (modifiers & Qt::ShiftModifier) {
      if (const auto color = QColorDialog::getColor(StyleHelper::requestedBaseColor(), ICore::dialogParent()); color.isValid())
        StyleHelper::setBaseColor(color);
    }
  });

  registerDefaultContainers();
  registerDefaultActions();

  m_left_navigation_widget = new NavigationWidget(m_toggle_left_side_bar_action, Side::Left);
  m_right_navigation_widget = new NavigationWidget(m_toggle_right_side_bar_action, Side::Right);
  m_right_pane_widget = new RightPaneWidget();
  m_message_manager = new MessageManager;
  m_editor_manager = new EditorManager(this);
  m_external_tool_manager = new ExternalToolManager();

  setCentralWidget(m_mode_stack);
  m_progress_manager->progressView()->setParent(this);
  connect(qApp, &QApplication::focusChanged, this, &MainWindow::updateFocusWidget);

  // Add small Toolbuttons for toggling the navigation widgets
  StatusBarManager::addStatusBarWidget(m_toggle_left_side_bar_button, StatusBarManager::First);
  const auto childs_count = static_cast<int>(statusBar()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).count());

  statusBar()->insertPermanentWidget(childs_count - 1, m_toggle_right_side_bar_button); // before QSizeGrip
  statusBar()->setProperty("p_styled", true);

  const auto drop_support = new DropSupport(this, [](const QDropEvent *event, DropSupport *) {
    return event->source() == nullptr; // only accept drops from the "outside" (e.g. file manager)
  });

  connect(drop_support, &DropSupport::filesDropped, this, &MainWindow::openDroppedFiles);
}

auto MainWindow::navigationWidget(const Side side) const -> NavigationWidget*
{
  return side == Side::Left ? m_left_navigation_widget : m_right_navigation_widget;
}

auto MainWindow::setSidebarVisible(const bool visible, const Side side) const -> void
{
  if (NavigationWidgetPlaceHolder::current(side))
    navigationWidget(side)->setShown(visible);
}

auto MainWindow::askConfirmationBeforeExit() const -> bool
{
  return m_ask_confirmation_before_exit;
}

auto MainWindow::setAskConfirmationBeforeExit(const bool ask) -> void
{
  m_ask_confirmation_before_exit = ask;
}

auto MainWindow::setOverrideColor(const QColor &color) -> void
{
  m_override_color = color;
}

auto MainWindow::additionalAboutInformation() const -> QStringList
{
  return m_about_information;
}

auto MainWindow::appendAboutInformation(const QString &line) -> void
{
  m_about_information.append(line);
}

auto MainWindow::addPreCloseListener(const std::function<bool ()> &listener) -> void
{
  m_pre_close_listeners.append(listener);
}

MainWindow::~MainWindow()
{
  // Explicitly delete window support, because that calls methods from ICore that call methods
  // from mainwindow, so mainwindow still needs to be alive
  delete m_window_support;
  m_window_support = nullptr;
  delete m_external_tool_manager;
  m_external_tool_manager = nullptr;
  delete m_message_manager;
  m_message_manager = nullptr;
  delete m_shortcut_settings;
  m_shortcut_settings = nullptr;
  delete m_general_settings;
  m_general_settings = nullptr;
  delete m_system_settings;
  m_system_settings = nullptr;
  delete m_tool_settings;
  m_tool_settings = nullptr;
  delete m_mime_type_settings;
  m_mime_type_settings = nullptr;
  delete m_system_editor;
  m_system_editor = nullptr;
  delete m_printer;
  m_printer = nullptr;
  delete m_vcs_manager;
  m_vcs_manager = nullptr;
  // We need to delete editormanager and statusbarmanager explicitly before the end of the destructor,
  // because they might trigger stuff that tries to access data from editorwindow, like removeContextWidget
  // All modes are now gone
  OutputPaneManager::destroy();
  delete m_left_navigation_widget;
  delete m_right_navigation_widget;
  m_left_navigation_widget = nullptr;
  m_right_navigation_widget = nullptr;
  delete m_editor_manager;
  m_editor_manager = nullptr;
  delete m_progress_manager;
  m_progress_manager = nullptr;
  delete m_core_impl;
  m_core_impl = nullptr;
  delete m_right_pane_widget;
  m_right_pane_widget = nullptr;
  delete m_mode_manager;
  m_mode_manager = nullptr;
  delete m_js_expander;
  m_js_expander = nullptr;
}

auto MainWindow::init() const -> void
{
  m_progress_manager->init(); // needs the status bar manager
  MessageManager::init();
  OutputPaneManager::create();
}

auto MainWindow::extensionsInitialized() -> void
{
  EditorManagerPrivate::extensionsInitialized();
  MimeTypeSettings::restoreSettings();

  m_window_support = new WindowSupport(this, Context("Core.MainWindow"));
  m_window_support->setCloseActionEnabled(false);

  OutputPaneManager::initialize();
  VcsManager::extensionsInitialized();

  m_left_navigation_widget->setFactories(INavigationWidgetFactory::allNavigationFactories());
  m_right_navigation_widget->setFactories(INavigationWidgetFactory::allNavigationFactories());

  ModeManager::extensionsInitialized();

  readSettings();
  updateContext();

  emit m_core_impl->coreAboutToOpen();

  // Delay restoreWindowState, since it is overridden by LayoutRequest event
  QMetaObject::invokeMethod(this, &MainWindow::restoreWindowState, Qt::QueuedConnection);
  QMetaObject::invokeMethod(m_core_impl, &ICore::coreOpened, Qt::QueuedConnection);
}

static auto setRestart(const bool restart) -> void
{
  qApp->setProperty("restart", restart);
}

auto MainWindow::restart() -> void
{
  setRestart(true);
  exit();
}

auto MainWindow::closeEvent(QCloseEvent *event) -> void
{
  const auto cancel_close = [event] {
    event->ignore();
    setRestart(false);
  };

  // work around QTBUG-43344
  static auto already_closed = false;
  if (already_closed) {
    event->accept();
    return;
  }

  if (m_ask_confirmation_before_exit && (QMessageBox::question(this, tr("Exit %1?").arg(Constants::IDE_DISPLAY_NAME), tr("Exit %1?").arg(Constants::IDE_DISPLAY_NAME), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)) {
    event->ignore();
    return;
  }

  ICore::saveSettings(ICore::MainWindowClosing);

  // Save opened files
  if (!DocumentManager::saveAllModifiedDocuments()) {
    cancel_close();
    return;
  }

  for(const auto &listener: m_pre_close_listeners) {
    if (!listener()) {
      cancel_close();
      return;
    }
  }

  emit m_core_impl->coreAboutToClose();
  saveWindowSettings();
  m_left_navigation_widget->closeSubWidgets();
  m_right_navigation_widget->closeSubWidgets();

  event->accept();
  already_closed = true;
}

auto MainWindow::openDroppedFiles(const QList<DropSupport::FileSpec> &files) -> void
{
  raiseWindow();
  const auto file_paths = transform(files, &DropSupport::FileSpec::filePath);
  openFiles(file_paths, ICore::SwitchMode);
}

auto MainWindow::currentContextObject() const -> IContext*
{
  return m_active_context.isEmpty() ? nullptr : m_active_context.first();
}

auto MainWindow::statusBar() const -> QStatusBar*
{
  return m_mode_stack->statusBar();
}

auto MainWindow::infoBar() const -> InfoBar*
{
  return m_mode_stack->infoBar();
}

auto MainWindow::registerDefaultContainers() -> void
{
  const auto menubar = ActionManager::createMenuBar(Constants::MENU_BAR);

  if constexpr (!HostOsInfo::isMacHost()) // System menu bar on Mac
    setMenuBar(menubar->menuBar());

  menubar->appendGroup(Constants::G_FILE);
  menubar->appendGroup(Constants::G_EDIT);
  menubar->appendGroup(Constants::G_VIEW);
  menubar->appendGroup(Constants::G_TOOLS);
  menubar->appendGroup(Constants::G_WINDOW);
  menubar->appendGroup(Constants::G_HELP);

  // File Menu
  const auto filemenu = ActionManager::createMenu(Constants::M_FILE);
  menubar->addMenu(filemenu, Constants::G_FILE);
  filemenu->menu()->setTitle(tr("&File"));
  filemenu->appendGroup(Constants::G_FILE_NEW);
  filemenu->appendGroup(Constants::G_FILE_OPEN);
  filemenu->appendGroup(Constants::G_FILE_PROJECT);
  filemenu->appendGroup(Constants::G_FILE_SAVE);
  filemenu->appendGroup(Constants::G_FILE_EXPORT);
  filemenu->appendGroup(Constants::G_FILE_CLOSE);
  filemenu->appendGroup(Constants::G_FILE_PRINT);
  filemenu->appendGroup(Constants::G_FILE_OTHER);
  connect(filemenu->menu(), &QMenu::aboutToShow, this, &MainWindow::aboutToShowRecentFiles);

  // Edit Menu
  const auto medit = ActionManager::createMenu(Constants::M_EDIT);
  menubar->addMenu(medit, Constants::G_EDIT);
  medit->menu()->setTitle(tr("&Edit"));
  medit->appendGroup(Constants::G_EDIT_UNDOREDO);
  medit->appendGroup(Constants::G_EDIT_COPYPASTE);
  medit->appendGroup(Constants::G_EDIT_SELECTALL);
  medit->appendGroup(Constants::G_EDIT_ADVANCED);
  medit->appendGroup(Constants::G_EDIT_FIND);
  medit->appendGroup(Constants::G_EDIT_OTHER);

  const auto mview = ActionManager::createMenu(Constants::M_VIEW);
  menubar->addMenu(mview, Constants::G_VIEW);
  mview->menu()->setTitle(tr("&View"));
  mview->appendGroup(Constants::G_VIEW_VIEWS);
  mview->appendGroup(Constants::G_VIEW_PANES);

  // Tools Menu
  auto ac = ActionManager::createMenu(Constants::M_TOOLS);
  menubar->addMenu(ac, Constants::G_TOOLS);
  ac->menu()->setTitle(tr("&Tools"));

  // Window Menu
  const auto mwindow = ActionManager::createMenu(Constants::M_WINDOW);
  menubar->addMenu(mwindow, Constants::G_WINDOW);
  mwindow->menu()->setTitle(tr("&Window"));
  mwindow->appendGroup(Constants::G_WINDOW_SIZE);
  mwindow->appendGroup(Constants::G_WINDOW_SPLIT);
  mwindow->appendGroup(Constants::G_WINDOW_NAVIGATE);
  mwindow->appendGroup(Constants::G_WINDOW_LIST);
  mwindow->appendGroup(Constants::G_WINDOW_OTHER);

  // Help Menu
  ac = ActionManager::createMenu(Constants::M_HELP);
  menubar->addMenu(ac, Constants::G_HELP);
  ac->menu()->setTitle(tr("&Help"));
  ac->appendGroup(Constants::G_HELP_HELP);
  ac->appendGroup(Constants::G_HELP_SUPPORT);
  ac->appendGroup(Constants::G_HELP_ABOUT);
  ac->appendGroup(Constants::G_HELP_UPDATES);

  // macOS touch bar
  ac = ActionManager::createTouchBar(Constants::TOUCH_BAR, QIcon(), "Main TouchBar" /*never visible*/);
  ac->appendGroup(Constants::G_TOUCHBAR_HELP);
  ac->appendGroup(Constants::G_TOUCHBAR_EDITOR);
  ac->appendGroup(Constants::G_TOUCHBAR_NAVIGATION);
  ac->appendGroup(Constants::G_TOUCHBAR_OTHER);
  ac->touchBar()->setApplicationTouchBar();
}

auto MainWindow::registerDefaultActions() -> void
{
  const auto mfile = ActionManager::actionContainer(Constants::M_FILE);
  const auto medit = ActionManager::actionContainer(Constants::M_EDIT);
  const auto mview = ActionManager::actionContainer(Constants::M_VIEW);
  const auto mtools = ActionManager::actionContainer(Constants::M_TOOLS);
  const auto mwindow = ActionManager::actionContainer(Constants::M_WINDOW);
  const auto mhelp = ActionManager::actionContainer(Constants::M_HELP);

  // File menu separators
  mfile->addSeparator(Constants::G_FILE_SAVE);
  mfile->addSeparator(Constants::G_FILE_EXPORT);
  mfile->addSeparator(Constants::G_FILE_PRINT);
  mfile->addSeparator(Constants::G_FILE_CLOSE);
  mfile->addSeparator(Constants::G_FILE_OTHER);

  // Edit menu separators
  medit->addSeparator(Constants::G_EDIT_COPYPASTE);
  medit->addSeparator(Constants::G_EDIT_SELECTALL);
  medit->addSeparator(Constants::G_EDIT_FIND);
  medit->addSeparator(Constants::G_EDIT_ADVANCED);

  // Return to editor shortcut: Note this requires Qt to fix up
  // handling of shortcut overrides in menus, item views, combos....
  m_focus_to_editor = new QAction(tr("Return to Editor"), this);
  auto cmd = ActionManager::registerAction(m_focus_to_editor, Constants::S_RETURNTOEDITOR);
  cmd->setDefaultKeySequence(QKeySequence(Qt::Key_Escape));
  connect(m_focus_to_editor, &QAction::triggered, this, &MainWindow::setFocusToEditor);

  // New File Action
  auto icon = QIcon::fromTheme(QLatin1String("document-new"), Utils::Icons::NEWFILE.icon());

  m_new_action = new QAction(icon, tr("&New Project..."), this);
  cmd = ActionManager::registerAction(m_new_action, Constants::NEW);
  cmd->setDefaultKeySequence(QKeySequence("Ctrl+Shift+N"));
  mfile->addAction(cmd, Constants::G_FILE_NEW);

  connect(m_new_action, &QAction::triggered, this, []() {
    if (!ICore::isNewItemDialogRunning()) {
      ICore::showNewItemDialog(tr("New Project", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), equal(&IWizardFactory::kind, IWizardFactory::ProjectWizard)), FilePath());
    } else {
      ICore::raiseWindow(ICore::newItemDialog());
    }
  });

  const auto action = new QAction(icon, tr("New File..."), this);
  cmd = ActionManager::registerAction(action, Constants::NEW_FILE);
  cmd->setDefaultKeySequence(QKeySequence::New);
  mfile->addAction(cmd, Constants::G_FILE_NEW);

  connect(action, &QAction::triggered, this, []() {
    if (!ICore::isNewItemDialogRunning()) {
      ICore::showNewItemDialog(tr("New File", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), equal(&IWizardFactory::kind, IWizardFactory::FileWizard)), FilePath());
    } else {
      ICore::raiseWindow(ICore::newItemDialog());
    }
  });

  // Open Action
  icon = QIcon::fromTheme(QLatin1String("document-open"), Utils::Icons::OPENFILE.icon());
  m_open_action = new QAction(icon, tr("&Open File or Project..."), this);
  cmd = ActionManager::registerAction(m_open_action, Constants::OPEN);
  cmd->setDefaultKeySequence(QKeySequence::Open);
  mfile->addAction(cmd, Constants::G_FILE_OPEN);
  connect(m_open_action, &QAction::triggered, this, &MainWindow::openFile);

  // Open With Action
  m_open_with_action = new QAction(tr("Open File &With..."), this);
  cmd = ActionManager::registerAction(m_open_with_action, Constants::OPEN_WITH);
  mfile->addAction(cmd, Constants::G_FILE_OPEN);
  connect(m_open_with_action, &QAction::triggered, this, &MainWindow::openFileWith);

  // File->Recent Files Menu
  const auto ac = ActionManager::createMenu(Constants::M_FILE_RECENTFILES);
  mfile->addMenu(ac, Constants::G_FILE_OPEN);
  ac->menu()->setTitle(tr("Recent &Files"));
  ac->setOnAllDisabledBehavior(ActionContainer::on_all_disabled_behavior::Show);

  // Save Action
  icon = QIcon::fromTheme(QLatin1String("document-save"), Utils::Icons::SAVEFILE.icon());
  auto tmpaction = new QAction(icon, EditorManager::tr("&Save"), this);
  tmpaction->setEnabled(false);
  cmd = ActionManager::registerAction(tmpaction, Constants::SAVE);
  cmd->setDefaultKeySequence(QKeySequence::Save);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(tr("Save"));
  mfile->addAction(cmd, Constants::G_FILE_SAVE);

  // Save As Action
  icon = QIcon::fromTheme(QLatin1String("document-save-as"));
  tmpaction = new QAction(icon, EditorManager::tr("Save &As..."), this);
  tmpaction->setEnabled(false);
  cmd = ActionManager::registerAction(tmpaction, Constants::SAVEAS);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Ctrl+Shift+S") : QString()));
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(tr("Save As..."));
  mfile->addAction(cmd, Constants::G_FILE_SAVE);

  // SaveAll Action
  DocumentManager::registerSaveAllAction();

  // Print Action
  icon = QIcon::fromTheme(QLatin1String("document-print"));
  tmpaction = new QAction(icon, tr("&Print..."), this);
  tmpaction->setEnabled(false);
  cmd = ActionManager::registerAction(tmpaction, Constants::PRINT);
  cmd->setDefaultKeySequence(QKeySequence::Print);
  mfile->addAction(cmd, Constants::G_FILE_PRINT);

  // Exit Action
  icon = QIcon::fromTheme(QLatin1String("application-exit"));
  m_exit_action = new QAction(icon, tr("E&xit"), this);
  m_exit_action->setMenuRole(QAction::QuitRole);
  cmd = ActionManager::registerAction(m_exit_action, Constants::EXIT);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Q")));
  mfile->addAction(cmd, Constants::G_FILE_OTHER);
  connect(m_exit_action, &QAction::triggered, this, &MainWindow::exit);

  // Undo Action
  icon = QIcon::fromTheme(QLatin1String("edit-undo"), Utils::Icons::UNDO.icon());
  tmpaction = new QAction(icon, tr("&Undo"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::UNDO);
  cmd->setDefaultKeySequence(QKeySequence::Undo);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(tr("Undo"));
  medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
  tmpaction->setEnabled(false);

  // Redo Action
  icon = QIcon::fromTheme(QLatin1String("edit-redo"), Utils::Icons::REDO.icon());
  tmpaction = new QAction(icon, tr("&Redo"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::REDO);
  cmd->setDefaultKeySequence(QKeySequence::Redo);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(tr("Redo"));
  medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
  tmpaction->setEnabled(false);

  // Cut Action
  icon = QIcon::fromTheme(QLatin1String("edit-cut"), Utils::Icons::CUT.icon());
  tmpaction = new QAction(icon, tr("Cu&t"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::CUT);
  cmd->setDefaultKeySequence(QKeySequence::Cut);
  medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
  tmpaction->setEnabled(false);

  // Copy Action
  icon = QIcon::fromTheme(QLatin1String("edit-copy"), Utils::Icons::COPY.icon());
  tmpaction = new QAction(icon, tr("&Copy"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::COPY);
  cmd->setDefaultKeySequence(QKeySequence::Copy);
  medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
  tmpaction->setEnabled(false);

  // Paste Action
  icon = QIcon::fromTheme(QLatin1String("edit-paste"), Utils::Icons::PASTE.icon());
  tmpaction = new QAction(icon, tr("&Paste"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::PASTE);
  cmd->setDefaultKeySequence(QKeySequence::Paste);
  medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
  tmpaction->setEnabled(false);

  // Select All
  icon = QIcon::fromTheme(QLatin1String("edit-select-all"));
  tmpaction = new QAction(icon, tr("Select &All"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::SELECTALL);
  cmd->setDefaultKeySequence(QKeySequence::SelectAll);
  medit->addAction(cmd, Constants::G_EDIT_SELECTALL);
  tmpaction->setEnabled(false);

  // Goto Action
  icon = QIcon::fromTheme(QLatin1String("go-jump"));
  tmpaction = new QAction(icon, tr("&Go to Line..."), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::GOTO);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+L")));
  medit->addAction(cmd, Constants::G_EDIT_OTHER);
  tmpaction->setEnabled(false);

  // Zoom In Action
  icon = QIcon::hasThemeIcon("zoom-in") ? QIcon::fromTheme("zoom-in") : Utils::Icons::ZOOMIN_TOOLBAR.icon();
  tmpaction = new QAction(icon, tr("Zoom In"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_IN);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl++")));
  tmpaction->setEnabled(false);

  // Zoom Out Action
  icon = QIcon::hasThemeIcon("zoom-out") ? QIcon::fromTheme("zoom-out") : Utils::Icons::ZOOMOUT_TOOLBAR.icon();
  tmpaction = new QAction(icon, tr("Zoom Out"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_OUT);
  if constexpr (use_mac_shortcuts)
    cmd->setDefaultKeySequences({QKeySequence(tr("Ctrl+-")), QKeySequence(tr("Ctrl+Shift+-"))});
  else
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+-")));
  tmpaction->setEnabled(false);

  // Zoom Reset Action
  icon = QIcon::hasThemeIcon("zoom-original") ? QIcon::fromTheme("zoom-original") : Utils::Icons::EYE_OPEN_TOOLBAR.icon();
  tmpaction = new QAction(icon, tr("Original Size"), this);
  cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_RESET);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+0") : tr("Ctrl+0")));
  tmpaction->setEnabled(false);

  // Debug Qt Creator menu
  mtools->appendGroup(Constants::G_TOOLS_DEBUG);
  const auto mtoolsdebug = ActionManager::createMenu(Constants::M_TOOLS_DEBUG);
  mtoolsdebug->menu()->setTitle(tr("Debug %1").arg(Constants::IDE_DISPLAY_NAME));
  mtools->addMenu(mtoolsdebug, Constants::G_TOOLS_DEBUG);

  m_logger_action = new QAction(tr("Show Logs..."), this);
  cmd = ActionManager::registerAction(m_logger_action, Constants::LOGGER);
  mtoolsdebug->addAction(cmd);
  connect(m_logger_action, &QAction::triggered, this, [] { LoggingViewer::showLoggingView(); });

  // Preferences Action
  medit->appendGroup(Constants::G_EDIT_PREFERENCES);
  medit->addSeparator(Constants::G_EDIT_PREFERENCES);

  m_options_action = new QAction(tr("&Preferences..."), this);
  m_options_action->setMenuRole(QAction::PreferencesRole);
  cmd = ActionManager::registerAction(m_options_action, Constants::OPTIONS);
  cmd->setDefaultKeySequence(QKeySequence::Preferences);
  medit->addAction(cmd, Constants::G_EDIT_PREFERENCES);
  connect(m_options_action, &QAction::triggered, this, [] { ICore::showOptionsDialog(Id()); });

  mwindow->addSeparator(Constants::G_WINDOW_LIST);

  if constexpr (use_mac_shortcuts) {
    // Minimize Action
    auto minimize_action = new QAction(tr("Minimize"), this);
    minimize_action->setEnabled(false); // actual implementation in WindowSupport
    cmd = ActionManager::registerAction(minimize_action, Constants::MINIMIZE_WINDOW);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+M")));
    mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

    // Zoom Action
    auto zoom_action = new QAction(tr("Zoom"), this);
    zoom_action->setEnabled(false); // actual implementation in WindowSupport
    cmd = ActionManager::registerAction(zoom_action, Constants::ZOOM_WINDOW);
    mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);
  }

  // Full Screen Action
  const auto toggle_full_screen_action = new QAction(tr("Full Screen"), this);
  toggle_full_screen_action->setCheckable(!HostOsInfo::isMacHost());
  toggle_full_screen_action->setEnabled(false); // actual implementation in WindowSupport
  cmd = ActionManager::registerAction(toggle_full_screen_action, Constants::TOGGLE_FULLSCREEN);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Ctrl+Meta+F") : tr("Ctrl+Shift+F11")));
  if constexpr (HostOsInfo::isMacHost())
    cmd->setAttribute(Command::CA_UpdateText);
  mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

  if constexpr (use_mac_shortcuts) {
    mwindow->addSeparator(Constants::G_WINDOW_SIZE);
    auto close_action = new QAction(tr("Close Window"), this);
    close_action->setEnabled(false);
    cmd = ActionManager::registerAction(close_action, Constants::CLOSE_WINDOW);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Meta+W")));
    mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);
    mwindow->addSeparator(Constants::G_WINDOW_SIZE);
  }

  // Show Left Sidebar Action
  m_toggle_left_side_bar_action = new QAction(Utils::Icons::TOGGLE_LEFT_SIDEBAR.icon(), QCoreApplication::translate("Core", Constants::TR_SHOW_LEFT_SIDEBAR), this);
  m_toggle_left_side_bar_action->setCheckable(true);
  cmd = ActionManager::registerAction(m_toggle_left_side_bar_action, Constants::TOGGLE_LEFT_SIDEBAR);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Ctrl+0") : tr("Alt+0")));
  connect(m_toggle_left_side_bar_action, &QAction::triggered, this, [this](bool visible) { setSidebarVisible(visible, Side::Left); });
  const auto toggle_left_side_bar_proxy_action = ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_LEFT_SIDEBAR_TOOLBAR.icon());
  m_toggle_left_side_bar_button->setDefaultAction(toggle_left_side_bar_proxy_action);
  mview->addAction(cmd, Constants::G_VIEW_VIEWS);
  m_toggle_left_side_bar_action->setEnabled(false);

  // Show Right Sidebar Action
  m_toggle_right_side_bar_action = new QAction(Utils::Icons::TOGGLE_RIGHT_SIDEBAR.icon(), QCoreApplication::translate("Core", Constants::TR_SHOW_RIGHT_SIDEBAR), this);
  m_toggle_right_side_bar_action->setCheckable(true);
  cmd = ActionManager::registerAction(m_toggle_right_side_bar_action, Constants::TOGGLE_RIGHT_SIDEBAR);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Ctrl+Shift+0") : tr("Alt+Shift+0")));
  connect(m_toggle_right_side_bar_action, &QAction::triggered, this, [this](bool visible) { setSidebarVisible(visible, Side::Right); });
  const auto toggle_right_side_bar_proxy_action = ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_RIGHT_SIDEBAR_TOOLBAR.icon());
  m_toggle_right_side_bar_button->setDefaultAction(toggle_right_side_bar_proxy_action);
  mview->addAction(cmd, Constants::G_VIEW_VIEWS);
  m_toggle_right_side_bar_button->setEnabled(false);

  // Window->Views
  const auto mviews = ActionManager::createMenu(Constants::M_VIEW_VIEWS);
  mview->addMenu(mviews, Constants::G_VIEW_VIEWS);
  mviews->menu()->setTitle(tr("&Views"));

  // "Help" separators
  mhelp->addSeparator(Constants::G_HELP_SUPPORT);
  if constexpr (!HostOsInfo::isMacHost())
    mhelp->addSeparator(Constants::G_HELP_ABOUT);

  // About IDE Action
  icon = QIcon::fromTheme(QLatin1String("help-about"));
  if constexpr (HostOsInfo::isMacHost())
    tmpaction = new QAction(icon, tr("About &%1").arg(Constants::IDE_DISPLAY_NAME), this); // it's convention not to add dots to the about menu
  else
    tmpaction = new QAction(icon, tr("About &%1...").arg(Constants::IDE_DISPLAY_NAME), this);
  tmpaction->setMenuRole(QAction::AboutRole);
  cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_ORCA);
  mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
  tmpaction->setEnabled(true);
  connect(tmpaction, &QAction::triggered, this, &MainWindow::aboutOrca);

  //About Plugins Action
  tmpaction = new QAction(tr("About &Plugins..."), this);
  tmpaction->setMenuRole(QAction::ApplicationSpecificRole);
  cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_PLUGINS);
  mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
  tmpaction->setEnabled(true);
  connect(tmpaction, &QAction::triggered, this, &MainWindow::aboutPlugins);

  // Contact
  tmpaction = new QAction(tr("Contact..."), this);
  cmd = ActionManager::registerAction(tmpaction, "Orca.Contact");
  mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
  tmpaction->setEnabled(true);
  connect(tmpaction, &QAction::triggered, this, &MainWindow::contact);

  // About sep
  if constexpr (!HostOsInfo::isMacHost()) {
    // doesn't have the "About" actions in the Help menu
    tmpaction = new QAction(this);
    tmpaction->setSeparator(true);
    cmd = ActionManager::registerAction(tmpaction, "Orca.Help.Sep.About");
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
  }
}

auto MainWindow::openFile() -> void
{
  openFiles(EditorManager::getOpenFilePaths(), ICore::SwitchMode);
}

static auto findDocumentFactory(const QList<IDocumentFactory*> &fileFactories, const FilePath &filePath) -> IDocumentFactory*
{
  const auto type_name = mimeTypeForFile(filePath).name();
  return findOrDefault(fileFactories, [type_name](IDocumentFactory *f) {
    return f->mimeTypes().contains(type_name);
  });
}

/*!
 * \internal
 * Either opens \a filePaths with editors or loads a project.
 *
 *  \a flags can be used to stop on first failure, indicate that a file name
 *  might include line numbers and/or switch mode to edit mode.
 *
 *  \a workingDirectory is used when files are opened by a remote client, since
 *  the file names are relative to the client working directory.
 *
 *  Returns the first opened document. Required to support the \c -block flag
 *  for client mode.
 *
 *  \sa IPlugin::remoteArguments()
 */
auto MainWindow::openFiles(const FilePaths &file_paths, const ICore::OpenFilesFlags flags, const QString &working_directory) -> IDocument*
{
  const auto document_factories = IDocumentFactory::allDocumentFactories();
  IDocument *res = nullptr;
  const auto working_dir_base = working_directory.isEmpty() ? QDir::currentPath() : working_directory;

  for (const auto &file_path : file_paths) {
    const auto working_dir = file_path.withNewPath(working_dir_base);
    FilePath absolute_file_path;
    if (file_path.isAbsolutePath()) {
      absolute_file_path = file_path;
    } else {
      QTC_CHECK(!file_path.needsDevice());
      absolute_file_path = FilePath::fromString(working_dir_base).resolvePath(file_path.path());
    }
    if (const auto document_factory = findDocumentFactory(document_factories, file_path)) {
      if (const auto document = document_factory->open(absolute_file_path); !document) {
        if (flags & ICore::StopOnLoadFail)
          return res;
      } else {
        if (!res)
          res = document;
        if (flags & ICore::SwitchMode)
          ModeManager::activateMode(Id(Constants::MODE_EDIT));
      }
    } else if (flags & (ICore::SwitchSplitIfAlreadyVisible | ICore::CanContainLineAndColumnNumbers) || !res) {
      QFlags<EditorManager::OpenEditorFlag> emFlags;
      if (flags & ICore::SwitchSplitIfAlreadyVisible)
        emFlags |= EditorManager::SwitchSplitIfAlreadyVisible;
      const IEditor *editor = nullptr;
      if (flags & ICore::CanContainLineAndColumnNumbers) {
        const auto &link = Link::fromFilePath(absolute_file_path, true);
        editor = EditorManager::openEditorAt(link, {}, emFlags);
      } else {
        editor = EditorManager::openEditor(absolute_file_path, {}, emFlags);
      }
      if (!editor) {
        if (flags & ICore::StopOnLoadFail)
          return res;
      } else if (!res) {
        res = editor->document();
      }
    } else {
      const auto factory = IEditorFactory::preferredEditorFactories(absolute_file_path).value(0);
      DocumentModelPrivate::addSuspendedDocument(absolute_file_path, {}, factory ? factory->id() : Id());
    }
  }

  return res;
}

auto MainWindow::setFocusToEditor() -> void
{
  EditorManagerPrivate::doEscapeKeyFocusMoveMagic();
}

static auto acceptModalDialogs() -> void
{
  const auto top_levels = QApplication::topLevelWidgets();
  QList<QDialog*> dialogs_to_close;

  for (const auto top_level : top_levels) {
    if (const auto dialog = qobject_cast<QDialog*>(top_level)) {
      if (dialog->isModal())
        dialogs_to_close.append(dialog);
    }
  }

  for (const auto dialog : dialogs_to_close)
    dialog->accept();
}

auto MainWindow::exit() -> void
{
  // this function is most likely called from a user action
  // that is from an event handler of an object
  // since on close we are going to delete everything
  // so to prevent the deleting of that object we
  // just append it
  QMetaObject::invokeMethod(this, [this] {
    // Modal dialogs block the close event. So close them, in case this was triggered from
    // a RestartDialog in the settings dialog.
    acceptModalDialogs();
    close();
  }, Qt::QueuedConnection);
}

auto MainWindow::openFileWith() -> void
{
  for (const auto file_paths = EditorManager::getOpenFilePaths(); const auto &file_path : file_paths) {
    bool is_external;
    const auto editor_id = EditorManagerPrivate::getOpenWithEditorId(file_path, &is_external);

    if (!editor_id.isValid())
      continue;

    if (is_external)
      EditorManager::openExternalEditor(file_path, editor_id);
    else
      EditorManagerPrivate::openEditorWith(file_path, editor_id);
  }
}

auto MainWindow::contextObject(QWidget *widget) const -> IContext*
{
  const auto it = m_context_widgets.find(widget);
  return it == m_context_widgets.end() ? nullptr : it->second;
}

auto MainWindow::addContextObject(IContext *context) -> void
{
  if (!context)
    return;

  auto widget = context->widget();

  if (m_context_widgets.contains(widget))
    return;

  m_context_widgets.insert(std::make_pair(widget, context));
  connect(context, &QObject::destroyed, this, [this, context] { removeContextObject(context); });
}

auto MainWindow::removeContextObject(IContext *context) -> void
{
  if (!context)
    return;

  disconnect(context, &QObject::destroyed, this, nullptr);

  const auto it = std::ranges::find_if(std::as_const(m_context_widgets), [context](const std::pair<QWidget*, IContext*> &v) {
    return v.second == context;
  });

  if (it == m_context_widgets.cend())
    return;

  m_context_widgets.erase(it);

  if (m_active_context.removeAll(context) > 0)
    updateContextObject(m_active_context);
}

auto MainWindow::updateFocusWidget(const QWidget *old, QWidget *now) -> void
{
  Q_UNUSED(old)

  // Prevent changing the context object just because the menu or a menu item is activated
  if (qobject_cast<QMenuBar*>(now) || qobject_cast<QMenu*>(now))
    return;

  QList<IContext*> new_context;

  if (auto p = QApplication::focusWidget()) {
    IContext *context = nullptr;
    while (p) {
      context = contextObject(p);
      if (context)
        new_context.append(context);
      p = p->parentWidget();
    }
  }

  // ignore toplevels that define no context, like popups without parent
  if (!new_context.isEmpty() || QApplication::focusWidget() == focusWidget())
    updateContextObject(new_context);
}

auto MainWindow::updateContextObject(const QList<IContext*> &context) -> void
{
  emit m_core_impl->contextAboutToChange(context);
  m_active_context = context;

  updateContext();

  if constexpr (debugMainWindow) {
    qDebug() << "new context objects =" << context;
    foreach(IContext *c, context)
      qDebug() << (c ? c->widget() : nullptr) << (c ? c->widget()->metaObject()->className() : nullptr);
  }
}

auto MainWindow::aboutToShutdown() -> void
{
  disconnect(qApp, &QApplication::focusChanged, this, &MainWindow::updateFocusWidget);

  for (const auto val : m_context_widgets | std::views::values)
    disconnect(val, &QObject::destroyed, this, nullptr);

  m_active_context.clear();
  hide();
}

static constexpr char g_settings_group[] = "MainWindow";
static constexpr char g_color_key[] = "Color";
static constexpr char g_ask_before_exit_key[] = "AskBeforeExit";
static constexpr char g_window_geometry_key[] = "WindowGeometry";
static constexpr char g_window_state_key[] = "WindowState";
static constexpr char g_mode_selector_layout_key[] = "ModeSelectorLayout";
static constexpr bool g_ask_before_exit_default = false;

auto MainWindow::readSettings() -> void
{
  QSettings *settings = PluginManager::settings();
  settings->beginGroup(QLatin1String(g_settings_group));

  if (m_override_color.isValid()) {
    StyleHelper::setBaseColor(m_override_color);
    // Get adapted base color.
    m_override_color = StyleHelper::baseColor();
  } else {
    StyleHelper::setBaseColor(settings->value(QLatin1String(g_color_key), QColor(StyleHelper::DEFAULT_BASE_COLOR)).value<QColor>());
  }

  m_ask_confirmation_before_exit = settings->value(g_ask_before_exit_key, g_ask_before_exit_default).toBool();

  settings->endGroup();
  EditorManagerPrivate::readSettings();

  m_left_navigation_widget->restoreSettings(settings);
  m_right_navigation_widget->restoreSettings(settings);
  m_right_pane_widget->readSettings(settings);
}

auto MainWindow::saveSettings() const -> void
{
  const auto settings = PluginManager::settings();
  settings->beginGroup(QLatin1String(g_settings_group));

  if (!(m_override_color.isValid() && StyleHelper::baseColor() == m_override_color))
    settings->setValueWithDefault(g_color_key, StyleHelper::requestedBaseColor(), QColor(StyleHelper::DEFAULT_BASE_COLOR));

  settings->setValueWithDefault(g_ask_before_exit_key, m_ask_confirmation_before_exit, g_ask_before_exit_default);
  settings->endGroup();

  DocumentManager::saveSettings();
  ActionManager::saveSettings();
  EditorManagerPrivate::saveSettings();

  m_left_navigation_widget->saveSettings(settings);
  m_right_navigation_widget->saveSettings(settings);
}

auto MainWindow::saveWindowSettings() -> void
{
  QSettings *settings = PluginManager::settings();
  settings->beginGroup(QLatin1String(g_settings_group));

  // On OS X applications usually do not restore their full screen state.
  // To be able to restore the correct non-full screen geometry, we have to put
  // the window out of full screen before saving the geometry.
  // Works around QTBUG-45241
  if constexpr (HostOsInfo::isMacHost() && isFullScreen())
    setWindowState(windowState() & ~Qt::WindowFullScreen);

  settings->setValue(QLatin1String(g_window_geometry_key), saveGeometry());
  settings->setValue(QLatin1String(g_window_state_key), saveState());
  settings->endGroup();
}

auto MainWindow::updateAdditionalContexts(const Context &remove, const Context &add, const ICore::ContextPriority priority) -> void
{
  for(const auto& id: remove) {
    if (!id.isValid())
      continue;
    auto index = m_low_prio_additional_contexts.indexOf(id);
    if (index != -1)
      m_low_prio_additional_contexts.removeAt(index);
    index = m_high_prio_additional_contexts.indexOf(id);
    if (index != -1)
      m_high_prio_additional_contexts.removeAt(index);
  }

  for(const auto& id: add) {
    if (!id.isValid())
      continue;
    if (auto &cref = (priority == ICore::ContextPriority::High ? m_high_prio_additional_contexts : m_low_prio_additional_contexts); !cref.contains(id))
      cref.prepend(id);
  }

  updateContext();
}

auto MainWindow::updateContext() -> void
{
  auto& contexts = m_high_prio_additional_contexts;

  for (const auto &context : m_active_context)
    contexts.add(context->context());

  contexts.add(m_low_prio_additional_contexts);

  Context uniquecontexts;

  for (const auto &id : qAsConst(contexts)) {
    if (!uniquecontexts.contains(id))
      uniquecontexts.add(id);
  }

  ActionManager::setContext(uniquecontexts);
  emit m_core_impl->contextChanged(uniquecontexts);
}

auto MainWindow::aboutToShowRecentFiles() -> void
{
  const auto aci = ActionManager::actionContainer(Constants::M_FILE_RECENTFILES);
  const auto menu = aci->menu();
  menu->clear();

  const auto recent_files = DocumentManager::recentFiles();
  for (auto i = 0; i < recent_files.count(); ++i) {
    const auto& file = recent_files[i];
    const auto file_path = quoteAmpersands(file.first.shortNativePath());
    const auto action_text = ActionManager::withNumberAccelerator(file_path, i + 1);
    const auto action = menu->addAction(action_text);
    connect(action, &QAction::triggered, this, [file] {
      EditorManager::openEditor(file.first, file.second);
    });
  }

  const auto has_recent_files = !recent_files.isEmpty();
  menu->setEnabled(has_recent_files);

  // add the Clear Menu item
  if (has_recent_files) {
    menu->addSeparator();
    const auto action = menu->addAction(QCoreApplication::translate("Core", Constants::TR_CLEAR_MENU));
    connect(action, &QAction::triggered, DocumentManager::instance(), &DocumentManager::clearRecentFiles);
  }
}

auto MainWindow::aboutOrca() -> void
{
  if (!m_version_dialog) {
    m_version_dialog = new VersionDialog(this);
    connect(m_version_dialog, &QDialog::finished, this, &MainWindow::destroyVersionDialog);
    ICore::registerWindow(m_version_dialog, Context("Core.VersionDialog"));
    m_version_dialog->show();
  } else {
    ICore::raiseWindow(m_version_dialog);
  }
}

auto MainWindow::destroyVersionDialog() -> void
{
  if (m_version_dialog) {
    m_version_dialog->deleteLater();
    m_version_dialog = nullptr;
  }
}

auto MainWindow::aboutPlugins() -> void
{
  PluginDialog dialog(this);
  dialog.exec();
}

auto MainWindow::contact() -> void
{
  QMessageBox dlg(QMessageBox::Information, tr("Contact"), tr("<p>Qt Creator developers can be reached at the Qt Creator mailing list:</p>" "%1" "<p>or the #qt-creator channel on Libera.Chat IRC:</p>" "%2" "<p>Our bug tracker is located at %3.</p>" "<p>Please use %4 for bigger chunks of text.</p>").arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;" "<a href=\"https://lists.qt-project.org/listinfo/qt-creator\">" "mailto:qt-creator@qt-project.org" "</a></p>").arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;" "<a href=\"https://web.libera.chat/#qt-creator\">" "https://web.libera.chat/#qt-creator" "</a></p>").arg("<a href=\"https://bugreports.qt.io/projects/ORCABUG\">" "https://bugreports.qt.io" "</a>").arg("<a href=\"https://pastebin.com\">" "https://pastebin.com" "</a>"), QMessageBox::Ok, this);
  dlg.exec();
}

auto MainWindow::printer() const -> QPrinter*
{
  if (!m_printer)
    m_printer = new QPrinter(QPrinter::HighResolution);
  return m_printer;
}

auto MainWindow::restoreWindowState() -> void
{
  QSettings *settings = PluginManager::settings();
  settings->beginGroup(QLatin1String(g_settings_group));

  if (!restoreGeometry(settings->value(QLatin1String(g_window_geometry_key)).toByteArray()))
    resize(1260, 700); // size without window decoration

  restoreState(settings->value(QLatin1String(g_window_state_key)).toByteArray());
  settings->endGroup();
  show();
  StatusBarManager::restoreSettings();
}

} // namespace Internal
} // namespace Core
