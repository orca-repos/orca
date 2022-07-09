// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/icodestylepreferencesfactory.hpp>

namespace CppEditor {
class CppCodeStyleWidget;

class CPPEDITOR_EXPORT CppCodeStylePreferencesFactory : public TextEditor::ICodeStylePreferencesFactory {
public:
  CppCodeStylePreferencesFactory();

  auto languageId() -> Utils::Id override;
  auto displayName() -> QString override;
  auto createCodeStyle() const -> TextEditor::ICodeStylePreferences* override;
  auto createEditor(TextEditor::ICodeStylePreferences *settings, ProjectExplorer::Project *project, QWidget *parent) const -> QWidget* override;
  auto createIndenter(QTextDocument *doc) const -> TextEditor::Indenter* override;
  auto snippetProviderGroupId() const -> QString override;
  auto previewText() const -> QString override;
  virtual auto additionalTab(ProjectExplorer::Project *project, QWidget *parent) const -> std::pair<CppCodeStyleWidget*, QString>;
};

} // namespace CppEditor
