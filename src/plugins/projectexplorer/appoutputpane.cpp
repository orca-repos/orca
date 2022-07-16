// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "appoutputpane.hpp"

#include "projectexplorer.hpp"
#include "projectexplorerconstants.hpp"
#include "projectexplorericons.hpp"
#include "runcontrol.hpp"
#include "session.hpp"
#include "showoutputtaskhandler.hpp"
#include "windebuginterface.hpp"

#include <core/core-action-manager.hpp>
#include <core/core-command.hpp>
#include <core/core-constants.hpp>
#include <core/core-interface.hpp>
#include <core/core-output-window.hpp>
#include <texteditor/behaviorsettings.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <extensionsystem/invoker.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <utils/algorithm.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static Q_LOGGING_CATEGORY(appOutputLog, "qtc.projectexplorer.appoutput", QtWarningMsg);

using namespace ProjectExplorer;
using namespace Internal;

constexpr char OPTIONS_PAGE_ID[] = "B.ProjectExplorer.AppOutputOptions";

static auto debuggerPlugin() -> QObject*
{
  return ExtensionSystem::PluginManager::getObjectByName("DebuggerPlugin");
}

static auto msgAttachDebuggerTooltip(const QString &handleDescription = QString()) -> QString
{
  return handleDescription.isEmpty() ? AppOutputPane::tr("Attach debugger to this process") : AppOutputPane::tr("Attach debugger to %1").arg(handleDescription);
}

namespace {
constexpr char SETTINGS_KEY[] = "ProjectExplorer/AppOutput/Zoom";
constexpr char C_APP_OUTPUT[] = "ProjectExplorer.ApplicationOutput";
constexpr char POP_UP_FOR_RUN_OUTPUT_KEY[] = "ProjectExplorer/Settings/ShowRunOutput";
constexpr char POP_UP_FOR_DEBUG_OUTPUT_KEY[] = "ProjectExplorer/Settings/ShowDebugOutput";
constexpr char CLEAN_OLD_OUTPUT_KEY[] = "ProjectExplorer/Settings/CleanOldAppOutput";
constexpr char MERGE_CHANNELS_KEY[] = "ProjectExplorer/Settings/MergeStdErrAndStdOut";
constexpr char WRAP_OUTPUT_KEY[] = "ProjectExplorer/Settings/WrapAppOutput";
constexpr char MAX_LINES_KEY[] = "ProjectExplorer/Settings/MaxAppOutputLines";
}

namespace ProjectExplorer {
namespace Internal {

class TabWidget : public QTabWidget {
  Q_OBJECT public:
  TabWidget(QWidget *parent = nullptr);
signals:
  auto contextMenuRequested(const QPoint &pos, int index) -> void;
protected:
  auto eventFilter(QObject *object, QEvent *event) -> bool override;
private:
  auto slotContextMenuRequested(const QPoint &pos) -> void;
  int m_tabIndexForMiddleClick = -1;
};

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent)
{
  tabBar()->installEventFilter(this);
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &QWidget::customContextMenuRequested, this, &TabWidget::slotContextMenuRequested);
}

auto TabWidget::eventFilter(QObject *object, QEvent *event) -> bool
{
  if (object == tabBar()) {
    if (event->type() == QEvent::MouseButtonPress) {
      const auto *me = static_cast<QMouseEvent*>(event);
      if (me->button() == Qt::MiddleButton) {
        m_tabIndexForMiddleClick = tabBar()->tabAt(me->pos());
        event->accept();
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      const auto *me = static_cast<QMouseEvent*>(event);
      if (me->button() == Qt::MiddleButton) {
        const auto tab = tabBar()->tabAt(me->pos());
        if (tab != -1 && tab == m_tabIndexForMiddleClick) emit tabCloseRequested(tab);
        m_tabIndexForMiddleClick = -1;
        event->accept();
        return true;
      }
    }
  }
  return QTabWidget::eventFilter(object, event);
}

auto TabWidget::slotContextMenuRequested(const QPoint &pos) -> void
{
  emit contextMenuRequested(pos, tabBar()->tabAt(pos));
}

AppOutputPane::RunControlTab::RunControlTab(RunControl *runControl, Orca::Plugin::Core::OutputWindow *w) : runControl(runControl), window(w)
{
  if (runControl && w) {
    w->reset();
    runControl->setupFormatter(w->outputFormatter());
  }
}

AppOutputPane::AppOutputPane() : m_mainWidget(new QWidget), m_tabWidget(new TabWidget), m_stopAction(new QAction(tr("Stop"), this)), m_closeCurrentTabAction(new QAction(tr("Close Tab"), this)), m_closeAllTabsAction(new QAction(tr("Close All Tabs"), this)), m_closeOtherTabsAction(new QAction(tr("Close Other Tabs"), this)), m_reRunButton(new QToolButton), m_stopButton(new QToolButton), m_attachButton(new QToolButton), m_settingsButton(new QToolButton), m_formatterWidget(new QWidget), m_handler(new ShowOutputTaskHandler(this, tr("Show &App Output"), tr("Show the output that generated this issue in the Application Output pane."), tr("A")))
{
  ExtensionSystem::PluginManager::addObject(m_handler);

  setObjectName("AppOutputPane"); // Used in valgrind engine
  loadSettings();

  // Rerun
  m_reRunButton->setIcon(Utils::Icons::RUN_SMALL_TOOLBAR.icon());
  m_reRunButton->setToolTip(tr("Re-run this run-configuration."));
  m_reRunButton->setEnabled(false);
  connect(m_reRunButton, &QToolButton::clicked, this, &AppOutputPane::reRunRunControl);

  // Stop
  m_stopAction->setIcon(Utils::Icons::STOP_SMALL_TOOLBAR.icon());
  m_stopAction->setToolTip(tr("Stop running program."));
  m_stopAction->setEnabled(false);

  const auto cmd = Orca::Plugin::Core::ActionManager::registerAction(m_stopAction, Constants::STOP);
  cmd->setDescription(m_stopAction->toolTip());

  m_stopButton->setDefaultAction(cmd->action());

  connect(m_stopAction, &QAction::triggered, this, &AppOutputPane::stopRunControl);

  // Attach
  m_attachButton->setToolTip(msgAttachDebuggerTooltip());
  m_attachButton->setEnabled(false);
  m_attachButton->setIcon(Icons::DEBUG_START_SMALL_TOOLBAR.icon());

  connect(m_attachButton, &QToolButton::clicked, this, &AppOutputPane::attachToRunControl);

  connect(this, &IOutputPane::zoomInRequested, this, &AppOutputPane::zoomIn);
  connect(this, &IOutputPane::zoomOutRequested, this, &AppOutputPane::zoomOut);
  connect(this, &IOutputPane::resetZoomRequested, this, &AppOutputPane::resetZoom);

  m_settingsButton->setToolTip(tr("Open Settings Page"));
  m_settingsButton->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
  connect(m_settingsButton, &QToolButton::clicked, this, [] {
    Orca::Plugin::Core::ICore::showOptionsDialog(OPTIONS_PAGE_ID);
  });

  const auto formatterWidgetsLayout = new QHBoxLayout;
  formatterWidgetsLayout->setContentsMargins(QMargins());
  m_formatterWidget->setLayout(formatterWidgetsLayout);

  // Spacer (?)

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  m_tabWidget->setDocumentMode(true);
  m_tabWidget->setTabsClosable(true);
  m_tabWidget->setMovable(true);
  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
  layout->addWidget(m_tabWidget);

  connect(m_tabWidget, &QTabWidget::currentChanged, this, &AppOutputPane::tabChanged);
  connect(m_tabWidget, &TabWidget::contextMenuRequested, this, &AppOutputPane::contextMenuRequested);

  m_mainWidget->setLayout(layout);

  connect(SessionManager::instance(), &SessionManager::aboutToUnloadSession, this, &AppOutputPane::aboutToUnloadSession);

  setupFilterUi("AppOutputPane.Filter");
  setFilteringEnabled(false);
  setZoomButtonsEnabled(false);
  setupContext("Core.AppOutputPane", m_mainWidget);
}

AppOutputPane::~AppOutputPane()
{
  qCDebug(appOutputLog) << "AppOutputPane::~AppOutputPane: Entries left" << m_runControlTabs.size();

  for (const auto &rt : qAsConst(m_runControlTabs)) {
    delete rt.window;
    delete rt.runControl;
  }
  delete m_mainWidget;
  ExtensionSystem::PluginManager::removeObject(m_handler);
  delete m_handler;
}

auto AppOutputPane::currentIndex() const -> int
{
  if (const QWidget *w = m_tabWidget->currentWidget())
    return indexOf(w);
  return -1;
}

auto AppOutputPane::currentRunControl() const -> RunControl*
{
  const auto index = currentIndex();
  if (index != -1)
    return m_runControlTabs.at(index).runControl;
  return nullptr;
}

auto AppOutputPane::indexOf(const RunControl *rc) const -> int
{
  for (int i = m_runControlTabs.size() - 1; i >= 0; i--)
    if (m_runControlTabs.at(i).runControl == rc)
      return i;
  return -1;
}

auto AppOutputPane::indexOf(const QWidget *outputWindow) const -> int
{
  for (int i = m_runControlTabs.size() - 1; i >= 0; i--)
    if (m_runControlTabs.at(i).window == outputWindow)
      return i;
  return -1;
}

auto AppOutputPane::tabWidgetIndexOf(int runControlIndex) const -> int
{
  if (runControlIndex >= 0 && runControlIndex < m_runControlTabs.size())
    return m_tabWidget->indexOf(m_runControlTabs.at(runControlIndex).window);
  return -1;
}

auto AppOutputPane::updateCloseActions() -> void
{
  const auto tabCount = m_tabWidget->count();
  m_closeCurrentTabAction->setEnabled(tabCount > 0);
  m_closeAllTabsAction->setEnabled(tabCount > 0);
  m_closeOtherTabsAction->setEnabled(tabCount > 1);
}

auto AppOutputPane::aboutToClose() const -> bool
{
  return Utils::allOf(m_runControlTabs, [](const RunControlTab &rt) {
    return !rt.runControl || !rt.runControl->isRunning() || rt.runControl->promptToStop();
  });
}

auto AppOutputPane::aboutToUnloadSession() -> void
{
  closeTabs(CloseTabWithPrompt);
}

auto AppOutputPane::outputWidget(QWidget *) -> QWidget*
{
  return m_mainWidget;
}

auto AppOutputPane::toolBarWidgets() const -> QList<QWidget*>
{
  return QList<QWidget*>{m_reRunButton, m_stopButton, m_attachButton, m_settingsButton, m_formatterWidget} + IOutputPane::toolBarWidgets();
}

auto AppOutputPane::displayName() const -> QString
{
  return tr("Application Output");
}

auto AppOutputPane::priorityInStatusBar() const -> int
{
  return 60;
}

auto AppOutputPane::clearContents() -> void
{
  const auto *currentWindow = qobject_cast<Orca::Plugin::Core::OutputWindow*>(m_tabWidget->currentWidget());
  if (currentWindow)
    currentWindow->clear();
}

auto AppOutputPane::hasFocus() const -> bool
{
  const auto widget = m_tabWidget->currentWidget();
  if (!widget)
    return false;
  return widget->window()->focusWidget() == widget;
}

auto AppOutputPane::canFocus() const -> bool
{
  return m_tabWidget->currentWidget();
}

auto AppOutputPane::setFocus() -> void
{
  if (m_tabWidget->currentWidget())
    m_tabWidget->currentWidget()->setFocus();
}

auto AppOutputPane::updateFilter() -> void
{
  const auto index = currentIndex();
  if (index != -1) {
    m_runControlTabs.at(index).window->updateFilterProperties(filterText(), filterCaseSensitivity(), filterUsesRegexp(), filterIsInverted());
  }
}

auto AppOutputPane::outputWindows() const -> QList<Orca::Plugin::Core::OutputWindow*>
{
  QList<Orca::Plugin::Core::OutputWindow*> windows;
  for (const auto &tab : qAsConst(m_runControlTabs)) {
    if (tab.window)
      windows << tab.window;
  }
  return windows;
}

auto AppOutputPane::ensureWindowVisible(Orca::Plugin::Core::OutputWindow *ow) -> void
{
  m_tabWidget->setCurrentWidget(ow);
}

auto AppOutputPane::createNewOutputWindow(RunControl *rc) -> void
{
  QTC_ASSERT(rc, return);

  connect(rc, &RunControl::aboutToStart, this, &AppOutputPane::slotRunControlChanged);
  connect(rc, &RunControl::started, this, &AppOutputPane::slotRunControlChanged);
  connect(rc, &RunControl::stopped, this, &AppOutputPane::slotRunControlFinished);
  connect(rc, &RunControl::applicationProcessHandleChanged, this, &AppOutputPane::enableDefaultButtons);
  connect(rc, &RunControl::appendMessage, this, [this, rc](const QString &out, Utils::OutputFormat format) {
    appendMessage(rc, out, format);
  });

  // First look if we can reuse a tab
  const auto thisRunnable = rc->runnable();
  const auto tabIndex = Utils::indexOf(m_runControlTabs, [&](const RunControlTab &tab) {
    if (!tab.runControl || tab.runControl->isRunning())
      return false;
    const auto otherRunnable = tab.runControl->runnable();
    return thisRunnable.command == otherRunnable.command && thisRunnable.workingDirectory == otherRunnable.workingDirectory && thisRunnable.environment == otherRunnable.environment;
  });
  if (tabIndex != -1) {
    auto &tab = m_runControlTabs[tabIndex];
    // Reuse this tab
    if (tab.runControl)
      tab.runControl->initiateFinish();
    tab.runControl = rc;
    tab.window->reset();
    rc->setupFormatter(tab.window->outputFormatter());

    handleOldOutput(tab.window);

    // Update the title.
    m_tabWidget->setTabText(tabIndex, rc->displayName());

    tab.window->scrollToBottom();
    qCDebug(appOutputLog) << "AppOutputPane::createNewOutputWindow: Reusing tab" << tabIndex << "for" << rc;
    return;
  }
  // Create new
  static auto counter = 0;
  const auto contextId = Utils::Id(C_APP_OUTPUT).withSuffix(counter++);
  const Orca::Plugin::Core::Context context(contextId);
  auto ow = new Orca::Plugin::Core::OutputWindow(context, SETTINGS_KEY, m_tabWidget);
  ow->setWindowTitle(tr("Application Output Window"));
  ow->setWindowIcon(Icons::WINDOW.icon());
  ow->setWordWrapEnabled(m_settings.wrapOutput);
  ow->setMaxCharCount(m_settings.maxCharCount);

  auto updateFontSettings = [ow] {
    ow->setBaseFont(TextEditor::TextEditorSettings::fontSettings().font());
  };

  auto updateBehaviorSettings = [ow] {
    ow->setWheelZoomEnabled(TextEditor::TextEditorSettings::behaviorSettings().m_scrollWheelZooming);
  };

  updateFontSettings();
  updateBehaviorSettings();

  connect(ow, &Orca::Plugin::Core::OutputWindow::wheelZoom, this, [this, ow]() {
    const auto fontZoom = ow->fontZoom();
    for (const auto &tab : qAsConst(m_runControlTabs))
      tab.window->setFontZoom(fontZoom);
  });
  connect(TextEditor::TextEditorSettings::instance(), &TextEditor::TextEditorSettings::fontSettingsChanged, ow, updateFontSettings);
  connect(TextEditor::TextEditorSettings::instance(), &TextEditor::TextEditorSettings::behaviorSettingsChanged, ow, updateBehaviorSettings);

  m_runControlTabs.push_back(RunControlTab(rc, ow));
  m_tabWidget->addTab(ow, rc->displayName());
  qCDebug(appOutputLog) << "AppOutputPane::createNewOutputWindow: Adding tab for" << rc;
  updateCloseActions();
  setFilteringEnabled(m_tabWidget->count() > 0);
}

auto AppOutputPane::handleOldOutput(Orca::Plugin::Core::OutputWindow *window) const -> void
{
  if (m_settings.cleanOldOutput)
    window->clear();
  else
    window->grayOutOldContent();
}

auto AppOutputPane::updateFromSettings() -> void
{
  for (const auto &tab : qAsConst(m_runControlTabs)) {
    tab.window->setWordWrapEnabled(m_settings.wrapOutput);
    tab.window->setMaxCharCount(m_settings.maxCharCount);
  }
}

auto AppOutputPane::appendMessage(RunControl *rc, const QString &out, Utils::OutputFormat format) -> void
{
  const auto index = indexOf(rc);
  if (index != -1) {
    const Orca::Plugin::Core::OutputWindow *window = m_runControlTabs.at(index).window;
    QString stringToWrite;
    if (format == Utils::NormalMessageFormat || format == Utils::ErrorMessageFormat) {
      stringToWrite = QTime::currentTime().toString();
      stringToWrite += ": ";
    }
    stringToWrite += out;
    window->appendMessage(stringToWrite, format);
    if (format != Utils::NormalMessageFormat) {
      auto &tab = m_runControlTabs[index];
      switch (tab.behaviorOnOutput) {
      case AppOutputPaneMode::FlashOnOutput:
        flash();
        break;
      case AppOutputPaneMode::PopupOnFirstOutput:
        tab.behaviorOnOutput = AppOutputPaneMode::FlashOnOutput;
        Q_FALLTHROUGH();
      case AppOutputPaneMode::PopupOnOutput:
        popup(NoModeSwitch);
        break;
      }
    }
  }
}

auto AppOutputPane::setSettings(const AppOutputSettings &settings) -> void
{
  m_settings = settings;
  storeSettings();
  updateFromSettings();
}

const AppOutputPaneMode kRunOutputModeDefault = AppOutputPaneMode::PopupOnFirstOutput;
const AppOutputPaneMode kDebugOutputModeDefault = AppOutputPaneMode::FlashOnOutput;
const bool kCleanOldOutputDefault = false;
const bool kMergeChannelsDefault = false;
const bool kWrapOutputDefault = true;

auto AppOutputPane::storeSettings() const -> void
{
  const auto s = Orca::Plugin::Core::ICore::settings();
  s->setValueWithDefault(POP_UP_FOR_RUN_OUTPUT_KEY, int(m_settings.runOutputMode), int(kRunOutputModeDefault));
  s->setValueWithDefault(POP_UP_FOR_DEBUG_OUTPUT_KEY, int(m_settings.debugOutputMode), int(kDebugOutputModeDefault));
  s->setValueWithDefault(CLEAN_OLD_OUTPUT_KEY, m_settings.cleanOldOutput, kCleanOldOutputDefault);
  s->setValueWithDefault(MERGE_CHANNELS_KEY, m_settings.mergeChannels, kMergeChannelsDefault);
  s->setValueWithDefault(WRAP_OUTPUT_KEY, m_settings.wrapOutput, kWrapOutputDefault);
  s->setValueWithDefault(MAX_LINES_KEY, m_settings.maxCharCount / 100, Orca::Plugin::Core::DEFAULT_MAX_CHAR_COUNT);
}

auto AppOutputPane::loadSettings() -> void
{
  QSettings *const s = Orca::Plugin::Core::ICore::settings();
  const auto modeFromSettings = [s](const QString key, AppOutputPaneMode defaultValue) {
    return static_cast<AppOutputPaneMode>(s->value(key, int(defaultValue)).toInt());
  };
  m_settings.runOutputMode = modeFromSettings(POP_UP_FOR_RUN_OUTPUT_KEY, kRunOutputModeDefault);
  m_settings.debugOutputMode = modeFromSettings(POP_UP_FOR_DEBUG_OUTPUT_KEY, kDebugOutputModeDefault);
  m_settings.cleanOldOutput = s->value(CLEAN_OLD_OUTPUT_KEY, kCleanOldOutputDefault).toBool();
  m_settings.mergeChannels = s->value(MERGE_CHANNELS_KEY, kMergeChannelsDefault).toBool();
  m_settings.wrapOutput = s->value(WRAP_OUTPUT_KEY, kWrapOutputDefault).toBool();
  m_settings.maxCharCount = s->value(MAX_LINES_KEY, Orca::Plugin::Core::DEFAULT_MAX_CHAR_COUNT).toInt() * 100;
}

auto AppOutputPane::showTabFor(RunControl *rc) -> void
{
  m_tabWidget->setCurrentIndex(tabWidgetIndexOf(indexOf(rc)));
}

auto AppOutputPane::setBehaviorOnOutput(RunControl *rc, AppOutputPaneMode mode) -> void
{
  const auto index = indexOf(rc);
  if (index != -1)
    m_runControlTabs[index].behaviorOnOutput = mode;
}

auto AppOutputPane::reRunRunControl() -> void
{
  const auto index = currentIndex();
  const auto &tab = m_runControlTabs.at(index);
  QTC_ASSERT(tab.runControl, return);
  QTC_ASSERT(index != -1 && !tab.runControl->isRunning(), return);

  handleOldOutput(tab.window);
  tab.window->scrollToBottom();
  tab.runControl->initiateReStart();
}

auto AppOutputPane::attachToRunControl() -> void
{
  const auto index = currentIndex();
  QTC_ASSERT(index != -1, return);
  RunControl *rc = m_runControlTabs.at(index).runControl;
  QTC_ASSERT(rc && rc->isRunning(), return);
  ExtensionSystem::Invoker<void>(debuggerPlugin(), "attachExternalApplication", rc);
}

auto AppOutputPane::stopRunControl() -> void
{
  const auto index = currentIndex();
  QTC_ASSERT(index != -1, return);
  RunControl *rc = m_runControlTabs.at(index).runControl;
  QTC_ASSERT(rc, return);

  if (rc->isRunning()) {
    if (optionallyPromptToStop(rc))
      rc->initiateStop();
  } else {
    QTC_CHECK(false);
    rc->forceStop();
  }

  qCDebug(appOutputLog) << "AppOutputPane::stopRunControl" << rc;
}

auto AppOutputPane::closeTabs(CloseTabMode mode) -> void
{
  for (auto t = m_tabWidget->count() - 1; t >= 0; t--)
    closeTab(t, mode);
}

auto AppOutputPane::allRunControls() const -> QList<RunControl*>
{
  const auto list = Utils::transform<QList>(m_runControlTabs, [](const RunControlTab &tab) {
    return tab.runControl.data();
  });
  return Utils::filtered(list, [](RunControl *rc) { return rc; });
}

auto AppOutputPane::closeTab(int tabIndex, CloseTabMode closeTabMode) -> void
{
  auto index = indexOf(m_tabWidget->widget(tabIndex));
  QTC_ASSERT(index != -1, return);

  RunControl *runControl = m_runControlTabs[index].runControl;
  const Orca::Plugin::Core::OutputWindow *window = m_runControlTabs[index].window;
  qCDebug(appOutputLog) << "AppOutputPane::closeTab tab" << tabIndex << runControl << window;
  // Prompt user to stop
  if (closeTabMode == CloseTabWithPrompt) {
    const auto tabWidget = m_tabWidget->widget(tabIndex);
    if (runControl && runControl->isRunning() && !runControl->promptToStop())
      return;
    // The event loop has run, thus the ordering might have changed, a tab might
    // have been closed, so do some strange things...
    tabIndex = m_tabWidget->indexOf(tabWidget);
    index = indexOf(tabWidget);
    if (tabIndex == -1 || index == -1)
      return;
  }

  m_tabWidget->removeTab(tabIndex);
  delete window;

  if (runControl)
    runControl->initiateFinish(); // Will self-destruct.
  m_runControlTabs.removeAt(index);
  updateCloseActions();
  setFilteringEnabled(m_tabWidget->count() > 0);

  if (m_runControlTabs.isEmpty())
    hide();
}

auto AppOutputPane::optionallyPromptToStop(RunControl *runControl) -> bool
{
  auto settings = ProjectExplorerPlugin::projectExplorerSettings();
  if (!runControl->promptToStop(&settings.prompToStopRunControl))
    return false;
  ProjectExplorerPlugin::setProjectExplorerSettings(settings);
  return true;
}

auto AppOutputPane::projectRemoved() -> void
{
  tabChanged(m_tabWidget->currentIndex());
}

auto AppOutputPane::enableDefaultButtons() -> void
{
  enableButtons(currentRunControl());
}

auto AppOutputPane::zoomIn(int range) -> void
{
  for (const auto &tab : qAsConst(m_runControlTabs))
    tab.window->zoomIn(range);
}

auto AppOutputPane::zoomOut(int range) -> void
{
  for (const auto &tab : qAsConst(m_runControlTabs))
    tab.window->zoomOut(range);
}

auto AppOutputPane::resetZoom() -> void
{
  for (const auto &tab : qAsConst(m_runControlTabs))
    tab.window->resetZoom();
}

auto AppOutputPane::enableButtons(const RunControl *rc) -> void
{
  if (rc) {
    const auto isRunning = rc->isRunning();
    m_reRunButton->setEnabled(rc->isStopped() && rc->supportsReRunning());
    m_reRunButton->setIcon(rc->icon().icon());
    m_stopAction->setEnabled(isRunning);
    if (isRunning && debuggerPlugin() && rc->applicationProcessHandle().isValid()) {
      m_attachButton->setEnabled(true);
      const auto h = rc->applicationProcessHandle();
      const auto tip = h.isValid() ? RunControl::tr("PID %1").arg(h.pid()) : RunControl::tr("Invalid");
      m_attachButton->setToolTip(msgAttachDebuggerTooltip(tip));
    } else {
      m_attachButton->setEnabled(false);
      m_attachButton->setToolTip(msgAttachDebuggerTooltip());
    }
    setZoomButtonsEnabled(true);
  } else {
    m_reRunButton->setEnabled(false);
    m_reRunButton->setIcon(Utils::Icons::RUN_SMALL_TOOLBAR.icon());
    m_attachButton->setEnabled(false);
    m_attachButton->setToolTip(msgAttachDebuggerTooltip());
    m_stopAction->setEnabled(false);
    setZoomButtonsEnabled(false);
  }
  m_formatterWidget->setVisible(m_formatterWidget->layout()->count());
}

auto AppOutputPane::tabChanged(int i) -> void
{
  const auto index = indexOf(m_tabWidget->widget(i));
  if (i != -1 && index != -1) {
    const auto &controlTab = m_runControlTabs[index];
    controlTab.window->updateFilterProperties(filterText(), filterCaseSensitivity(), filterUsesRegexp(), filterIsInverted());
    enableButtons(controlTab.runControl);
  } else {
    enableDefaultButtons();
  }
}

auto AppOutputPane::contextMenuRequested(const QPoint &pos, int index) -> void
{
  const QList<QAction*> actions = {m_closeCurrentTabAction, m_closeAllTabsAction, m_closeOtherTabsAction};
  const auto action = QMenu::exec(actions, m_tabWidget->mapToGlobal(pos), nullptr, m_tabWidget);
  const auto currentIdx = index != -1 ? index : currentIndex();
  if (action == m_closeCurrentTabAction) {
    if (currentIdx >= 0)
      closeTab(currentIdx);
  } else if (action == m_closeAllTabsAction) {
    closeTabs(CloseTabWithPrompt);
  } else if (action == m_closeOtherTabsAction) {
    for (auto t = m_tabWidget->count() - 1; t >= 0; t--)
      if (t != currentIdx)
        closeTab(t);
  }
}

auto AppOutputPane::slotRunControlChanged() -> void
{
  const auto current = currentRunControl();
  if (current && current == sender())
    enableButtons(current); // RunControl::isRunning() cannot be trusted in signal handler.
}

auto AppOutputPane::slotRunControlFinished() -> void
{
  auto *rc = qobject_cast<RunControl*>(sender());
  QTimer::singleShot(0, this, [this, rc]() { slotRunControlFinished2(rc); });
  for (const auto &t : qAsConst(m_runControlTabs)) {
    if (t.runControl == rc) {
      t.window->flush();
      break;
    }
  }
}

auto AppOutputPane::slotRunControlFinished2(RunControl *sender) -> void
{
  const auto senderIndex = indexOf(sender);

  // This slot is queued, so the stop() call in closeTab might lead to this slot, after closeTab already cleaned up
  if (senderIndex == -1)
    return;

  // Enable buttons for current
  const auto current = currentRunControl();

  qCDebug(appOutputLog) << "AppOutputPane::runControlFinished" << sender << senderIndex << "current" << current << m_runControlTabs.size();

  if (current && current == sender)
    enableButtons(current);

  ProjectExplorerPlugin::updateRunActions();

  #ifdef Q_OS_WIN
  const auto isRunning = Utils::anyOf(m_runControlTabs, [](const RunControlTab &rt) {
    return rt.runControl && rt.runControl->isRunning();
  });
  if (!isRunning)
    WinDebugInterface::instance()->stop();
  #endif
}

auto AppOutputPane::canNext() const -> bool
{
  return false;
}

auto AppOutputPane::canPrevious() const -> bool
{
  return false;
}

auto AppOutputPane::goToNext() -> void {}

auto AppOutputPane::goToPrev() -> void {}

auto AppOutputPane::canNavigate() const -> bool
{
  return false;
}

class AppOutputSettingsWidget : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::AppOutputSettingsPage)
public:
  AppOutputSettingsWidget()
  {
    const auto &settings = ProjectExplorerPlugin::appOutputSettings();
    m_wrapOutputCheckBox.setText(tr("Word-wrap output"));
    m_wrapOutputCheckBox.setChecked(settings.wrapOutput);
    m_cleanOldOutputCheckBox.setText(tr("Clear old output on a new run"));
    m_cleanOldOutputCheckBox.setChecked(settings.cleanOldOutput);
    m_mergeChannelsCheckBox.setText(tr("Merge stderr and stdout"));
    m_mergeChannelsCheckBox.setChecked(settings.mergeChannels);
    for (const auto modeComboBox : {&m_runOutputModeComboBox, &m_debugOutputModeComboBox}) {
      modeComboBox->addItem(tr("Always"), int(AppOutputPaneMode::PopupOnOutput));
      modeComboBox->addItem(tr("Never"), int(AppOutputPaneMode::FlashOnOutput));
      modeComboBox->addItem(tr("On First Output Only"), int(AppOutputPaneMode::PopupOnFirstOutput));
    }
    m_runOutputModeComboBox.setCurrentIndex(m_runOutputModeComboBox.findData(int(settings.runOutputMode)));
    m_debugOutputModeComboBox.setCurrentIndex(m_debugOutputModeComboBox.findData(int(settings.debugOutputMode)));
    m_maxCharsBox.setMaximum(100000000);
    m_maxCharsBox.setValue(settings.maxCharCount);
    const auto layout = new QVBoxLayout(this);
    layout->addWidget(&m_wrapOutputCheckBox);
    layout->addWidget(&m_cleanOldOutputCheckBox);
    layout->addWidget(&m_mergeChannelsCheckBox);
    const auto maxCharsLayout = new QHBoxLayout;
    const auto msg = tr("Limit output to %1 characters");
    const auto parts = msg.split("%1") << QString() << QString();
    maxCharsLayout->addWidget(new QLabel(parts.at(0).trimmed()));
    maxCharsLayout->addWidget(&m_maxCharsBox);
    maxCharsLayout->addWidget(new QLabel(parts.at(1).trimmed()));
    maxCharsLayout->addStretch(1);
    const auto outputModeLayout = new QFormLayout;
    outputModeLayout->addRow(tr("Open pane on output when running:"), &m_runOutputModeComboBox);
    outputModeLayout->addRow(tr("Open pane on output when debugging:"), &m_debugOutputModeComboBox);
    layout->addLayout(outputModeLayout);
    layout->addLayout(maxCharsLayout);
    layout->addStretch(1);
  }

  auto apply() -> void final
  {
    AppOutputSettings s;
    s.wrapOutput = m_wrapOutputCheckBox.isChecked();
    s.cleanOldOutput = m_cleanOldOutputCheckBox.isChecked();
    s.mergeChannels = m_mergeChannelsCheckBox.isChecked();
    s.runOutputMode = static_cast<AppOutputPaneMode>(m_runOutputModeComboBox.currentData().toInt());
    s.debugOutputMode = static_cast<AppOutputPaneMode>(m_debugOutputModeComboBox.currentData().toInt());
    s.maxCharCount = m_maxCharsBox.value();

    ProjectExplorerPlugin::setAppOutputSettings(s);
  }

private:
  QCheckBox m_wrapOutputCheckBox;
  QCheckBox m_cleanOldOutputCheckBox;
  QCheckBox m_mergeChannelsCheckBox;
  QComboBox m_runOutputModeComboBox;
  QComboBox m_debugOutputModeComboBox;
  QSpinBox m_maxCharsBox;
};

AppOutputSettingsPage::AppOutputSettingsPage()
{
  setId(OPTIONS_PAGE_ID);
  setDisplayName(AppOutputSettingsWidget::tr("Application Output"));
  setCategory(Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new AppOutputSettingsWidget; });
}

} // namespace Internal
} // namespace ProjectExplorer

#include "appoutputpane.moc"

