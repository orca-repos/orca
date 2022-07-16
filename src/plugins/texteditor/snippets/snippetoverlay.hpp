// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "snippet.hpp"
#include "texteditor/texteditoroverlay.hpp"

#include <QTextEdit>

namespace TextEditor {
class NameMangler;

namespace Internal {

class SnippetOverlay : public TextEditorOverlay {
public:
  using TextEditorOverlay::TextEditorOverlay;

  auto clear() -> void override;
  auto addSnippetSelection(const QTextCursor &cursor, const QColor &color, NameMangler *mangler, int variableGoup) -> void;
  auto setFinalSelection(const QTextCursor &cursor, const QColor &color) -> void;
  auto updateEquivalentSelections(const QTextCursor &cursor) -> void;
  auto accept() -> void;
  auto hasCursorInSelection(const QTextCursor &cursor) const -> bool;
  auto firstSelectionCursor() const -> QTextCursor;
  auto nextSelectionCursor(const QTextCursor &cursor) const -> QTextCursor;
  auto previousSelectionCursor(const QTextCursor &cursor) const -> QTextCursor;
  auto isFinalSelection(const QTextCursor &cursor) const -> bool;

private:
  struct SnippetSelection {
    int variableIndex = -1;
    NameMangler *mangler;
  };

  auto indexForCursor(const QTextCursor &cursor) const -> int;
  auto selectionForCursor(const QTextCursor &cursor) const -> SnippetSelection;

  QList<SnippetSelection> m_selections;
  int m_finalSelectionIndex = -1;
  QMap<int, QList<int>> m_variables;
};

} // namespace Internal
} // namespace TextEditor

