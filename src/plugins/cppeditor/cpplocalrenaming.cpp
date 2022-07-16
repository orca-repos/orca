// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpplocalrenaming.hpp"

#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/fontsettings.hpp>

#include <utils/qtcassert.hpp>

/*!
    \class CppEditor::Internal::CppLocalRenaming
    \brief A helper class of CppEditorWidget that implements renaming local usages.

    \internal

    Local use selections must be first set/updated with updateLocalUseSelections().
    Afterwards the local renaming can be started with start(). The CppEditorWidget
    can then delegate work related to the local renaming mode to the handle*
    functions.

    \sa CppEditor::Internal::CppEditorWidget
 */

namespace {

auto modifyCursorSelection(QTextCursor &cursor, int position, int anchor) -> void
{
  cursor.setPosition(anchor);
  cursor.setPosition(position, QTextCursor::KeepAnchor);
}

} // anonymous namespace

namespace CppEditor {
namespace Internal {

CppLocalRenaming::CppLocalRenaming(TextEditor::TextEditorWidget *editorWidget) : m_editorWidget(editorWidget), m_modifyingSelections(false), m_renameSelectionChanged(false), m_firstRenameChangeExpected(false)
{
  forgetRenamingSelection();
}

auto CppLocalRenaming::updateSelectionsForVariableUnderCursor(const QList<QTextEdit::ExtraSelection> &selections) -> void
{
  if (isActive())
    return;

  m_selections = selections;
}

auto CppLocalRenaming::start() -> bool
{
  stop();

  if (findRenameSelection(m_editorWidget->textCursor().position())) {
    updateRenamingSelectionFormat(textCharFormat(TextEditor::C_OCCURRENCES_RENAME));
    m_firstRenameChangeExpected = true;
    updateEditorWidgetWithSelections();
    return true;
  }

  return false;
}

auto CppLocalRenaming::handlePaste() -> bool
{
  if (!isActive())
    return false;

  startRenameChange();
  m_editorWidget->TextEditorWidget::paste();
  finishRenameChange();
  return true;
}

auto CppLocalRenaming::handleCut() -> bool
{
  if (!isActive())
    return false;

  startRenameChange();
  m_editorWidget->TextEditorWidget::cut();
  finishRenameChange();
  return true;
}

auto CppLocalRenaming::handleSelectAll() -> bool
{
  if (!isActive())
    return false;

  auto cursor = m_editorWidget->textCursor();
  if (!isWithinRenameSelection(cursor.position()))
    return false;

  modifyCursorSelection(cursor, renameSelectionBegin(), renameSelectionEnd());
  m_editorWidget->setTextCursor(cursor);
  return true;
}

auto CppLocalRenaming::isActive() const -> bool
{
  return m_renameSelectionIndex != -1;
}

auto CppLocalRenaming::handleKeyPressEvent(QKeyEvent *e) -> bool
{
  if (!isActive())
    return false;

  auto cursor = m_editorWidget->textCursor();
  const auto cursorPosition = cursor.position();
  const auto moveMode = (e->modifiers() & Qt::ShiftModifier) ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;

  switch (e->key()) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
  case Qt::Key_Escape:
    stop();
    e->accept();
    return true;
  case Qt::Key_Home: {
    // Send home to start of name when within the name and not at the start
    if (renameSelectionBegin() < cursorPosition && cursorPosition <= renameSelectionEnd()) {
      cursor.setPosition(renameSelectionBegin(), moveMode);
      m_editorWidget->setTextCursor(cursor);
      e->accept();
      return true;
    }
    break;
  }
  case Qt::Key_End: {
    // Send end to end of name when within the name and not at the end
    if (renameSelectionBegin() <= cursorPosition && cursorPosition < renameSelectionEnd()) {
      cursor.setPosition(renameSelectionEnd(), moveMode);
      m_editorWidget->setTextCursor(cursor);
      e->accept();
      return true;
    }
    break;
  }
  case Qt::Key_Backspace: {
    if (cursorPosition == renameSelectionBegin() && !cursor.hasSelection()) {
      // Eat backspace at start of name when there is no selection
      e->accept();
      return true;
    }
    break;
  }
  case Qt::Key_Delete: {
    if (cursorPosition == renameSelectionEnd() && !cursor.hasSelection()) {
      // Eat delete at end of name when there is no selection
      e->accept();
      return true;
    }
    break;
  }
  default: {
    break;
  }
  } // switch

  startRenameChange();

  const auto wantEditBlock = isWithinRenameSelection(cursorPosition);
  const auto undoSizeBeforeEdit = m_editorWidget->document()->availableUndoSteps();
  if (wantEditBlock) {
    if (m_firstRenameChangeExpected) // Change inside rename selection
      cursor.beginEditBlock();
    else
      cursor.joinPreviousEditBlock();
  }
  emit processKeyPressNormally(e);
  if (wantEditBlock) {
    cursor.endEditBlock();
    if (m_firstRenameChangeExpected
      // QTCREATORBUG-16350
      && m_editorWidget->document()->availableUndoSteps() != undoSizeBeforeEdit) {
      m_firstRenameChangeExpected = false;
    }
  }
  finishRenameChange();
  return true;
}

auto CppLocalRenaming::encourageApply() -> bool
{
  if (!isActive())
    return false;
  finishRenameChange();
  return true;
}

auto CppLocalRenaming::renameSelection() -> QTextEdit::ExtraSelection&
{
  return m_selections[m_renameSelectionIndex];
}

auto CppLocalRenaming::updateRenamingSelectionCursor(const QTextCursor &cursor) -> void
{
  QTC_ASSERT(isActive(), return);
  renameSelection().cursor = cursor;
}

auto CppLocalRenaming::updateRenamingSelectionFormat(const QTextCharFormat &format) -> void
{
  QTC_ASSERT(isActive(), return);
  renameSelection().format = format;
}

auto CppLocalRenaming::forgetRenamingSelection() -> void
{
  m_renameSelectionIndex = -1;
}

auto CppLocalRenaming::isWithinSelection(const QTextEdit::ExtraSelection &selection, int position) -> bool
{
  return selection.cursor.selectionStart() <= position && position <= selection.cursor.selectionEnd();
}

auto CppLocalRenaming::isWithinRenameSelection(int position) -> bool
{
  return isWithinSelection(renameSelection(), position);
}

auto CppLocalRenaming::isSameSelection(int cursorPosition) const -> bool
{
  if (!isActive())
    return false;

  const auto &sel = m_selections[m_renameSelectionIndex];
  return isWithinSelection(sel, cursorPosition);
}

auto CppLocalRenaming::findRenameSelection(int cursorPosition) -> bool
{
  for (int i = 0, total = m_selections.size(); i < total; ++i) {
    const auto &sel = m_selections.at(i);
    if (isWithinSelection(sel, cursorPosition)) {
      m_renameSelectionIndex = i;
      return true;
    }
  }

  return false;
}

auto CppLocalRenaming::changeOtherSelectionsText(const QString &text) -> void
{
  for (int i = 0, total = m_selections.size(); i < total; ++i) {
    if (i == m_renameSelectionIndex)
      continue;

    auto &selection = m_selections[i];
    const auto pos = selection.cursor.selectionStart();
    selection.cursor.removeSelectedText();
    selection.cursor.insertText(text);
    selection.cursor.setPosition(pos, QTextCursor::KeepAnchor);
  }
}

auto CppLocalRenaming::onContentsChangeOfEditorWidgetDocument(int position, int charsRemoved, int charsAdded) -> void
{
  Q_UNUSED(charsRemoved)

  if (!isActive() || m_modifyingSelections)
    return;

  if (position + charsAdded == renameSelectionBegin()) // Insert at beginning, expand cursor
    modifyCursorSelection(renameSelection().cursor, position, renameSelectionEnd());

  // Keep in mind that cursor position and anchor move automatically
  m_renameSelectionChanged = isWithinRenameSelection(position) && isWithinRenameSelection(position + charsAdded);

  if (!m_renameSelectionChanged)
    stop();
}

auto CppLocalRenaming::startRenameChange() -> void
{
  m_renameSelectionChanged = false;
}

auto CppLocalRenaming::updateEditorWidgetWithSelections() -> void
{
  m_editorWidget->setExtraSelections(TextEditor::TextEditorWidget::CodeSemanticsSelection, m_selections);
}

auto CppLocalRenaming::textCharFormat(TextEditor::TextStyle category) const -> QTextCharFormat
{
  return m_editorWidget->textDocument()->fontSettings().toTextCharFormat(category);
}

auto CppLocalRenaming::finishRenameChange() -> void
{
  if (!m_renameSelectionChanged)
    return;

  m_modifyingSelections = true;

  auto cursor = m_editorWidget->textCursor();
  cursor.joinPreviousEditBlock();

  modifyCursorSelection(cursor, renameSelectionBegin(), renameSelectionEnd());
  updateRenamingSelectionCursor(cursor);
  changeOtherSelectionsText(cursor.selectedText());
  updateEditorWidgetWithSelections();

  cursor.endEditBlock();

  m_modifyingSelections = false;
}

auto CppLocalRenaming::stop() -> void
{
  if (!isActive())
    return;

  updateRenamingSelectionFormat(textCharFormat(TextEditor::C_OCCURRENCES));
  updateEditorWidgetWithSelections();
  forgetRenamingSelection();

  emit finished();
}

} // namespace Internal
} // namespace CppEditor
