// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "textdocumentmanipulatorinterface.hpp"

namespace TextEditor {

class TextEditorWidget;

class TextDocumentManipulator final : public TextDocumentManipulatorInterface {
public:
  TextDocumentManipulator(TextEditorWidget *textEditorWidget);

  auto currentPosition() const -> int final;
  auto positionAt(TextPositionOperation textPositionOperation) const -> int final;
  auto characterAt(int position) const -> QChar final;
  auto textAt(int position, int length) const -> QString final;
  auto textCursorAt(int position) const -> QTextCursor final;

  auto setCursorPosition(int position) -> void final;
  auto setAutoCompleteSkipPosition(int position) -> void final;
  auto replace(int position, int length, const QString &text) -> bool final;
  auto insertCodeSnippet(int position, const QString &text, const SnippetParser &parse) -> void final;
  auto paste() -> void final;
  auto encourageApply() -> void final;
  auto autoIndent(int position, int length) -> void override;

private:
  auto textIsDifferentAt(int position, int length, const QString &text) const -> bool;
  auto replaceWithoutCheck(int position, int length, const QString &text) -> void;

  TextEditorWidget *m_textEditorWidget;
};

} // namespace TextEditor
