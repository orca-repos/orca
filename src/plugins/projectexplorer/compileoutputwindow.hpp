// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildstep.hpp"
#include "projectexplorersettings.hpp"
#include <core/dialogs/ioptionspage.hpp>
#include <core/ioutputpane.hpp>


QT_BEGIN_NAMESPACE
class QToolButton;
QT_END_NAMESPACE

namespace Core {
class OutputWindow;
}

namespace Utils {
class OutputFormatter;
}

namespace ProjectExplorer {
class Task;

namespace Internal {
class ShowOutputTaskHandler;
class CompileOutputTextEdit;

class CompileOutputWindow final : public Core::IOutputPane {
  Q_OBJECT

public:
  explicit CompileOutputWindow(QAction *cancelBuildAction);
  ~CompileOutputWindow() override;

  auto outputWidget(QWidget *) -> QWidget* override;
  auto toolBarWidgets() const -> QList<QWidget*> override;
  auto displayName() const -> QString override { return tr("Compile Output"); }
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
  auto appendText(const QString &text, BuildStep::OutputFormat format) -> void;
  auto registerPositionOf(const Task &task, int linkedOutputLines, int skipLines, int offset = 0) -> void;
  auto flush() -> void;
  auto reset() -> void;
  auto settings() const -> const CompileOutputSettings& { return m_settings; }
  auto setSettings(const CompileOutputSettings &settings) -> void;
  auto outputFormatter() const -> Utils::OutputFormatter*;

private:
  auto updateFilter() -> void override;
  auto outputWindows() const -> QList<Core::OutputWindow*> override { return {m_outputWindow}; }
  auto loadSettings() -> void;
  auto storeSettings() const -> void;
  auto updateFromSettings() -> void;

  Core::OutputWindow *m_outputWindow;
  ShowOutputTaskHandler *m_handler;
  QToolButton *m_cancelBuildButton;
  QToolButton *const m_settingsButton;
  CompileOutputSettings m_settings;
};

class CompileOutputSettingsPage final : public Core::IOptionsPage {
public:
  CompileOutputSettingsPage();
};

} // namespace Internal
} // namespace ProjectExplorer
