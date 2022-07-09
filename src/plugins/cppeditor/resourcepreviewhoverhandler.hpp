// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/basehoverhandler.hpp>

#include <QString>

namespace CppEditor {
namespace Internal {

class ResourcePreviewHoverHandler : public TextEditor::BaseHoverHandler {
  auto identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void override;
  auto operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) -> void override;
  auto makeTooltip() const -> QString;

  QString m_resPath;
};

} // namespace Internal
} // namespace CppEditor
