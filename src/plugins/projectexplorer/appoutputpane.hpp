// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorersettings.hpp"

#include <core/core-output-pane-interface.hpp>
#include <core/core-options-page-interface.hpp>

#include <utils/outputformat.hpp>

#include <QPointer>
#include <QVector>

QT_BEGIN_NAMESPACE
class QTabWidget;
class QToolButton;
class QAction;
class QPoint;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {
class OutputWindow;
}

namespace ProjectExplorer {

class RunControl;
class Project;

namespace Internal {

class ShowOutputTaskHandler;
class TabWidget;

class AppOutputPane : public Orca::Plugin::Core::IOutputPane {
  Q_OBJECT

public:
  enum CloseTabMode {
    CloseTabNoPrompt,
    CloseTabWithPrompt
  };

  AppOutputPane();
  ~AppOutputPane() override;

  auto outputWidget(QWidget *) -> QWidget* override;
  auto toolBarWidgets() const -> QList<QWidget*> override;
  auto displayName() const -> QString override;
  auto priorityInStatusBar() const -> int override;
  auto clearContents() -> void override;
  auto canFocus() const -> bool override;
  auto hasFocus() const -> bool override;
  auto setFocus() -> void override;
  auto canNext() const -> bool override;
  auto canPrevious() const -> bool override;
  auto goToNext() -> void override;
  auto goToPrev() -> void override;
  auto canNavigate() const -> bool override;
  auto createNewOutputWindow(RunControl *rc) -> void;
  auto showTabFor(RunControl *rc) -> void;
  auto setBehaviorOnOutput(RunControl *rc, AppOutputPaneMode mode) -> void;
  auto aboutToClose() const -> bool;
  auto closeTabs(CloseTabMode mode) -> void;
  auto allRunControls() const -> QList<RunControl*>;

  // ApplicationOutput specifics
  auto projectRemoved() -> void;
  auto appendMessage(RunControl *rc, const QString &out, Utils::OutputFormat format) -> void;
  auto settings() const -> const AppOutputSettings& { return m_settings; }
  auto setSettings(const AppOutputSettings &settings) -> void;

private:
  auto reRunRunControl() -> void;
  auto stopRunControl() -> void;
  auto attachToRunControl() -> void;
  auto tabChanged(int) -> void;
  auto contextMenuRequested(const QPoint &pos, int index) -> void;
  auto slotRunControlChanged() -> void;
  auto slotRunControlFinished() -> void;
  auto slotRunControlFinished2(RunControl *sender) -> void;
  auto aboutToUnloadSession() -> void;
  auto updateFromSettings() -> void;
  auto enableDefaultButtons() -> void;
  auto zoomIn(int range) -> void;
  auto zoomOut(int range) -> void;
  auto resetZoom() -> void;
  auto enableButtons(const RunControl *rc) -> void;

  class RunControlTab {
  public:
    explicit RunControlTab(RunControl *runControl = nullptr, Orca::Plugin::Core::OutputWindow *window = nullptr);
    QPointer<RunControl> runControl;
    QPointer<Orca::Plugin::Core::OutputWindow> window;
    AppOutputPaneMode behaviorOnOutput = AppOutputPaneMode::FlashOnOutput;
  };

  auto closeTab(int index, CloseTabMode cm = CloseTabWithPrompt) -> void;
  auto optionallyPromptToStop(RunControl *runControl) -> bool;
  auto indexOf(const RunControl *) const -> int;
  auto indexOf(const QWidget *outputWindow) const -> int;
  auto currentIndex() const -> int;
  auto currentRunControl() const -> RunControl*;
  auto tabWidgetIndexOf(int runControlIndex) const -> int;
  auto handleOldOutput(Orca::Plugin::Core::OutputWindow *window) const -> void;
  auto updateCloseActions() -> void;
  auto updateFilter() -> void override;
  auto outputWindows() const -> QList<Orca::Plugin::Core::OutputWindow*> override;
  auto ensureWindowVisible(Orca::Plugin::Core::OutputWindow *ow) -> void override;
  auto loadSettings() -> void;
  auto storeSettings() const -> void;

  QWidget *m_mainWidget;
  TabWidget *m_tabWidget;
  QVector<RunControlTab> m_runControlTabs;
  int m_runControlCount = 0;
  QAction *m_stopAction;
  QAction *m_closeCurrentTabAction;
  QAction *m_closeAllTabsAction;
  QAction *m_closeOtherTabsAction;
  QToolButton *m_reRunButton;
  QToolButton *m_stopButton;
  QToolButton *m_attachButton;
  QToolButton *const m_settingsButton;
  QWidget *m_formatterWidget;
  ShowOutputTaskHandler *const m_handler;
  AppOutputSettings m_settings;
};

class AppOutputSettingsPage final : public Orca::Plugin::Core::IOptionsPage {
public:
  AppOutputSettingsPage();
};

} // namespace Internal
} // namespace ProjectExplorer
