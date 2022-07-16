// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/snippets/snippetparser.hpp>
#include <texteditor/texteditor_global.hpp>

QT_BEGIN_NAMESPACE
class QChar;
class QString;
class QTextCursor;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT TextDocumentManipulatorInterface {
public:
  virtual ~TextDocumentManipulatorInterface() = default;

  virtual auto currentPosition() const -> int = 0;
  virtual auto positionAt(TextPositionOperation textPositionOperation) const -> int = 0;
  virtual auto characterAt(int position) const -> QChar = 0;
  virtual auto textAt(int position, int length) const -> QString = 0;
  virtual auto textCursorAt(int position) const -> QTextCursor = 0;
  virtual auto setCursorPosition(int position) -> void = 0;
  virtual auto setAutoCompleteSkipPosition(int position) -> void = 0;
  virtual auto replace(int position, int length, const QString &text) -> bool = 0;
  virtual auto insertCodeSnippet(int position, const QString &text, const SnippetParser &parse) -> void = 0;
  virtual auto paste() -> void = 0;
  virtual auto encourageApply() -> void = 0;
  virtual auto autoIndent(int position, int length) -> void = 0;
};

} // namespace TextEditor
