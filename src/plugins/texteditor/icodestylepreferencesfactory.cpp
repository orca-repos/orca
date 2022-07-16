// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "icodestylepreferencesfactory.hpp"

#include "codestyleeditor.hpp"

using namespace TextEditor;

ICodeStylePreferencesFactory::ICodeStylePreferencesFactory() {}

auto ICodeStylePreferencesFactory::createCodeStyleEditor(ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project, QWidget *parent) -> CodeStyleEditorWidget*
{
  return new CodeStyleEditor(this, codeStyle, project, parent);
}
