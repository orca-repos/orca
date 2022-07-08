// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippeteditor.hpp"

#include <texteditor/textdocument.hpp>
#include <texteditor/texteditorconstants.hpp>

#include <QFocusEvent>

namespace TextEditor {

/*!
    \class TextEditor::SnippetEditorWidget
    \brief The SnippetEditorWidget class is a lightweight editor for code snippets
    with basic support for syntax highlighting, indentation, and others.
    \ingroup Snippets
*/

SnippetEditorWidget::SnippetEditorWidget(QWidget *parent) : TextEditorWidget(parent)
{
  setupFallBackEditor(Constants::SNIPPET_EDITOR_ID);
  setFrameStyle(StyledPanel | Sunken);
  setHighlightCurrentLine(false);
  setLineNumbersVisible(false);
  setParenthesesMatchingEnabled(true);
}

auto SnippetEditorWidget::focusOutEvent(QFocusEvent *event) -> void
{
  if (event->reason() != Qt::ActiveWindowFocusReason && document()->isModified()) {
    document()->setModified(false);
    emit snippetContentChanged();
  }
  TextEditorWidget::focusOutEvent(event);
}

auto SnippetEditorWidget::contextMenuEvent(QContextMenuEvent *e) -> void
{
  QPlainTextEdit::contextMenuEvent(e);
}

} // namespace
