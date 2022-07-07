// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "linecolumn.hpp"
#include "utils_global.hpp"

#include <QString>

QT_FORWARD_DECLARE_CLASS(QTextDocument)
QT_FORWARD_DECLARE_CLASS(QTextCursor)

namespace Utils {
namespace Text {

struct Replacement {
  Replacement() = default;
  Replacement(int offset, int length, const QString &text) : offset(offset), length(length), text(text) {}

  int offset = -1;
  int length = -1;
  QString text;

  auto isValid() const -> bool { return offset >= 0 && length >= 0; }
};

using Replacements = std::vector<Replacement>;

ORCA_UTILS_EXPORT auto applyReplacements(QTextDocument *doc, const Replacements &replacements) -> void;
// line is 1-based, column is 1-based
ORCA_UTILS_EXPORT auto convertPosition(const QTextDocument *document, int pos, int *line, int *column) -> bool;
ORCA_UTILS_EXPORT auto convertPosition(const QTextDocument *document, int pos) -> OptionalLineColumn;
// line and column are 1-based
ORCA_UTILS_EXPORT auto positionInText(const QTextDocument *textDocument, int line, int column) -> int;
ORCA_UTILS_EXPORT auto textAt(QTextCursor tc, int pos, int length) -> QString;
ORCA_UTILS_EXPORT auto selectAt(QTextCursor textCursor, int line, int column, uint length) -> QTextCursor;
ORCA_UTILS_EXPORT auto flippedCursor(const QTextCursor &cursor) -> QTextCursor;
ORCA_UTILS_EXPORT auto wordStartCursor(const QTextCursor &cursor) -> QTextCursor;
ORCA_UTILS_EXPORT auto wordUnderCursor(const QTextCursor &cursor) -> QString;
ORCA_UTILS_EXPORT auto utf8AdvanceCodePoint(const char *&current) -> bool;
ORCA_UTILS_EXPORT auto utf8NthLineOffset(const QTextDocument *textDocument, const QByteArray &buffer, int line) -> int;
ORCA_UTILS_EXPORT auto utf16LineColumn(const QByteArray &utf8Buffer, int utf8Offset) -> LineColumn;
ORCA_UTILS_EXPORT auto utf16LineTextInUtf8Buffer(const QByteArray &utf8Buffer, int currentUtf8Offset) -> QString;

} // Text
} // Utils
