// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "icodestylepreferencesfactory.hpp"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Project;
}

namespace TextEditor {

class ICodeStylePreferencesFactory;
class ICodeStylePreferences;
class SnippetEditorWidget;

class TEXTEDITOR_EXPORT CodeStyleEditor : public CodeStyleEditorWidget {
  Q_OBJECT

public:
  CodeStyleEditor(ICodeStylePreferencesFactory *factory, ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project = nullptr, QWidget *parent = nullptr);

private:
  auto updatePreview() -> void;

  QVBoxLayout *m_layout;
  ICodeStylePreferencesFactory *m_factory;
  ICodeStylePreferences *m_codeStyle;
  SnippetEditorWidget *m_preview;
};

} // namespace TextEditor
