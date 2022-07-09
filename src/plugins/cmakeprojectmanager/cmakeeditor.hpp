// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeEditorWidget;

class CMakeEditor : public TextEditor::BaseTextEditor {
  Q_OBJECT

public:
  auto contextHelp(const HelpCallback &callback) const -> void override;

  friend class CMakeEditorWidget;
};

class CMakeEditorFactory : public TextEditor::TextEditorFactory {
public:
  CMakeEditorFactory();
};

} // namespace Internal
} // namespace CMakeProjectManager
