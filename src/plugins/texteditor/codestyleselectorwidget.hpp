// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QWidget>

namespace ProjectExplorer {
class Project;
}

namespace TextEditor {
namespace Internal {

namespace Ui {
class CodeStyleSelectorWidget;
} // namespace Ui
} // namespace Internal

class ICodeStylePreferences;
class ICodeStylePreferencesFactory;

class TEXTEDITOR_EXPORT CodeStyleSelectorWidget : public QWidget {
  Q_OBJECT

public:
  explicit CodeStyleSelectorWidget(ICodeStylePreferencesFactory *factory, ProjectExplorer::Project *project = nullptr, QWidget *parent = nullptr);
  ~CodeStyleSelectorWidget() override;

  auto setCodeStyle(ICodeStylePreferences *codeStyle) -> void;

private:
  auto slotComboBoxActivated(int index) -> void;
  auto slotCurrentDelegateChanged(ICodeStylePreferences *delegate) -> void;
  auto slotCopyClicked() -> void;
  auto slotEditClicked() -> void;
  auto slotRemoveClicked() -> void;
  auto slotImportClicked() -> void;
  auto slotExportClicked() -> void;
  auto slotCodeStyleAdded(ICodeStylePreferences *) -> void;
  auto slotCodeStyleRemoved(ICodeStylePreferences *) -> void;
  auto slotUpdateName() -> void;
  auto updateName(ICodeStylePreferences *codeStyle) -> void;
  auto displayName(ICodeStylePreferences *codeStyle) const -> QString;

  Internal::Ui::CodeStyleSelectorWidget *m_ui;
  ICodeStylePreferencesFactory *m_factory;
  ICodeStylePreferences *m_codeStyle = nullptr;
  ProjectExplorer::Project *m_project = nullptr;
  bool m_ignoreGuiSignals = false;
};

} // namespace TextEditor
