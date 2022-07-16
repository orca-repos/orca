// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippetoverlay.hpp"

#include <utils/algorithm.hpp>

namespace TextEditor {
namespace Internal {

auto SnippetOverlay::clear() -> void
{
  TextEditorOverlay::clear();
  m_selections.clear();
  m_variables.clear();
}

auto SnippetOverlay::addSnippetSelection(const QTextCursor &cursor, const QColor &color, NameMangler *mangler, int variableIndex) -> void
{
  m_variables[variableIndex] << selections().size();
  SnippetSelection selection;
  selection.variableIndex = variableIndex;
  selection.mangler = mangler;
  m_selections << selection;
  addOverlaySelection(cursor, color, color, ExpandBegin);
}

auto SnippetOverlay::setFinalSelection(const QTextCursor &cursor, const QColor &color) -> void
{
  m_finalSelectionIndex = selections().size();
  addOverlaySelection(cursor, color, color, ExpandBegin);
}

auto SnippetOverlay::updateEquivalentSelections(const QTextCursor &cursor) -> void
{
  const auto &currentIndex = indexForCursor(cursor);
  if (currentIndex < 0)
    return;
  const auto &currentText = cursorForIndex(currentIndex).selectedText();
  const auto &equivalents = m_variables.value(m_selections[currentIndex].variableIndex);
  for (const auto i : equivalents) {
    if (i == currentIndex)
      continue;
    auto cursor = cursorForIndex(i);
    const auto &equivalentText = cursor.selectedText();
    if (currentText != equivalentText) {
      cursor.joinPreviousEditBlock();
      cursor.insertText(currentText);
      cursor.endEditBlock();
    }
  }
}

auto SnippetOverlay::accept() -> void
{
  hide();
  for (auto i = 0; i < m_selections.size(); ++i) {
    if (const auto mangler = m_selections[i].mangler) {
      auto cursor = cursorForIndex(i);
      const auto current = cursor.selectedText();
      const auto result = mangler->mangle(current);
      if (result != current) {
        cursor.joinPreviousEditBlock();
        cursor.insertText(result);
        cursor.endEditBlock();
      }
    }
  }
  clear();
}

auto SnippetOverlay::hasCursorInSelection(const QTextCursor &cursor) const -> bool
{
  return indexForCursor(cursor) >= 0;
}

auto SnippetOverlay::firstSelectionCursor() const -> QTextCursor
{
  const auto selections = TextEditorOverlay::selections();
  return selections.isEmpty() ? QTextCursor() : cursorForSelection(selections.first());
}

auto SnippetOverlay::nextSelectionCursor(const QTextCursor &cursor) const -> QTextCursor
{
  const auto selections = TextEditorOverlay::selections();
  if (selections.isEmpty())
    return {};
  const auto &currentSelection = selectionForCursor(cursor);
  if (currentSelection.variableIndex >= 0) {
    auto nextVariableIndex = currentSelection.variableIndex + 1;
    if (!m_variables.contains(nextVariableIndex)) {
      if (m_finalSelectionIndex >= 0)
        return cursorForIndex(m_finalSelectionIndex);
      nextVariableIndex = m_variables.firstKey();
    }

    for (const auto selectionIndex : m_variables[nextVariableIndex]) {
      if (selections[selectionIndex].m_cursor_begin.position() > cursor.position())
        return cursorForIndex(selectionIndex);
    }
    return cursorForIndex(m_variables[nextVariableIndex].first());
  }
  // currently not over a variable simply select the next available one
  for (const auto &candidate : selections) {
    if (candidate.m_cursor_begin.position() > cursor.position())
      return cursorForSelection(candidate);
  }
  return cursorForSelection(selections.first());
}

auto SnippetOverlay::previousSelectionCursor(const QTextCursor &cursor) const -> QTextCursor
{
  const auto selections = TextEditorOverlay::selections();
  if (selections.isEmpty())
    return {};
  const auto &currentSelection = selectionForCursor(cursor);
  if (currentSelection.variableIndex >= 0) {
    auto previousVariableIndex = currentSelection.variableIndex - 1;
    if (!m_variables.contains(previousVariableIndex))
      previousVariableIndex = m_variables.lastKey();

    const auto &equivalents = m_variables[previousVariableIndex];
    for (int i = equivalents.size() - 1; i >= 0; --i) {
      if (selections.at(equivalents.at(i)).m_cursor_end.position() < cursor.position())
        return cursorForIndex(equivalents.at(i));
    }
    return cursorForIndex(m_variables[previousVariableIndex].last());
  }
  // currently not over a variable simply select the previous available one
  for (int i = selections.size() - 1; i >= 0; --i) {
    if (selections.at(i).m_cursor_end.position() < cursor.position())
      return cursorForIndex(i);
  }
  return cursorForSelection(selections.last());
}

auto SnippetOverlay::isFinalSelection(const QTextCursor &cursor) const -> bool
{
  return m_finalSelectionIndex >= 0 ? cursor == cursorForIndex(m_finalSelectionIndex) : false;
}

auto SnippetOverlay::indexForCursor(const QTextCursor &cursor) const -> int
{
  return Utils::indexOf(selections(), [pos = cursor.position()](const OverlaySelection &selection) {
    return selection.m_cursor_begin.position() <= pos && selection.m_cursor_end.position() >= pos;
  });
}

auto SnippetOverlay::selectionForCursor(const QTextCursor &cursor) const -> SnippetSelection
{
  return m_selections.value(indexForCursor(cursor));
}

} // namespace Internal
} // namespace TextEditor
