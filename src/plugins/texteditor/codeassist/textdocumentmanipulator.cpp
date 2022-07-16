// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textdocumentmanipulator.hpp"

#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>

namespace TextEditor {

TextDocumentManipulator::TextDocumentManipulator(TextEditorWidget *textEditorWidget) : m_textEditorWidget(textEditorWidget) {}

auto TextDocumentManipulator::currentPosition() const -> int
{
  return m_textEditorWidget->position();
}

auto TextDocumentManipulator::positionAt(TextPositionOperation textPositionOperation) const -> int
{
  return m_textEditorWidget->position(textPositionOperation);
}

auto TextDocumentManipulator::characterAt(int position) const -> QChar
{
  return m_textEditorWidget->characterAt(position);
}

auto TextDocumentManipulator::textAt(int position, int length) const -> QString
{
  return m_textEditorWidget->textAt(position, length);
}

auto TextDocumentManipulator::textCursorAt(int position) const -> QTextCursor
{
  auto cursor = m_textEditorWidget->textCursor();
  cursor.setPosition(position);

  return cursor;
}

auto TextDocumentManipulator::setCursorPosition(int position) -> void
{
  m_textEditorWidget->setCursorPosition(position);
}

auto TextDocumentManipulator::setAutoCompleteSkipPosition(int position) -> void
{
  auto cursor = m_textEditorWidget->textCursor();
  cursor.setPosition(position);
  m_textEditorWidget->setAutoCompleteSkipPosition(cursor);
}

auto TextDocumentManipulator::replace(int position, int length, const QString &text) -> bool
{
  const auto textWillBeReplaced = textIsDifferentAt(position, length, text);

  if (textWillBeReplaced)
    replaceWithoutCheck(position, length, text);

  return textWillBeReplaced;
}

auto TextDocumentManipulator::insertCodeSnippet(int position, const QString &text, const SnippetParser &parse) -> void
{
  auto cursor = m_textEditorWidget->textCursor();
  cursor.setPosition(position, QTextCursor::KeepAnchor);
  m_textEditorWidget->insertCodeSnippet(cursor, text, parse);
}

auto TextDocumentManipulator::paste() -> void
{
  m_textEditorWidget->paste();
}

auto TextDocumentManipulator::encourageApply() -> void
{
  m_textEditorWidget->encourageApply();
}

namespace {

auto hasOnlyBlanksBeforeCursorInLine(QTextCursor textCursor) -> bool
{
  textCursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);

  const auto textBeforeCursor = textCursor.selectedText();

  const auto nonSpace = std::find_if(textBeforeCursor.cbegin(), textBeforeCursor.cend(), [](const QChar &signBeforeCursor) {
    return !signBeforeCursor.isSpace();
  });

  return nonSpace == textBeforeCursor.cend();
}

}

auto TextDocumentManipulator::autoIndent(int position, int length) -> void
{
  auto cursor = m_textEditorWidget->textCursor();
  cursor.setPosition(position);
  if (hasOnlyBlanksBeforeCursorInLine(cursor)) {
    cursor.setPosition(position + length, QTextCursor::KeepAnchor);

    m_textEditorWidget->textDocument()->autoIndent(cursor);
  }
}

auto TextDocumentManipulator::textIsDifferentAt(int position, int length, const QString &text) const -> bool
{
  const auto textToBeReplaced = m_textEditorWidget->textAt(position, length);

  return text != textToBeReplaced;
}

auto TextDocumentManipulator::replaceWithoutCheck(int position, int length, const QString &text) -> void
{
  auto cursor = m_textEditorWidget->textCursor();
  cursor.setPosition(position);
  cursor.setPosition(position + length, QTextCursor::KeepAnchor);
  cursor.insertText(text);
}

} // namespace TextEditor
