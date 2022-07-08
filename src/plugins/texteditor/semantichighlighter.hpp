// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "fontsettings.hpp"

#include <QFuture>
#include <QTextCharFormat>

#include <functional>
#include <utility>

QT_BEGIN_NAMESPACE
class QTextBlock;
QT_END_NAMESPACE

namespace TextEditor {

class SyntaxHighlighter;

class TEXTEDITOR_EXPORT HighlightingResult {
public:
  int line = 0;   // 1-based
  int column = 0; // 1-based
  int length = 0;
  TextStyles textStyles;
  int kind = 0; /// The various highlighters can define their own kind of results.
  bool useTextSyles = false;

  auto isValid() const -> bool { return line != 0; }
  auto isInvalid() const -> bool { return line == 0; }

  HighlightingResult() = default;
  HighlightingResult(int line, int column, int length, int kind) : line(line), column(column), length(length), kind(kind), useTextSyles(false) {}
  HighlightingResult(int line, int column, int length, TextStyles textStyles) : line(line), column(column), length(length), textStyles(textStyles), useTextSyles(true) {}

  auto operator==(const HighlightingResult &other) const -> bool
  {
    return line == other.line && column == other.column && length == other.length && kind == other.kind;
  }

  auto operator!=(const HighlightingResult &other) const -> bool { return !(*this == other); }
};

using HighlightingResults = QList<HighlightingResult>;

namespace SemanticHighlighter {

using Splitter = std::function<const QList<std::pair<HighlightingResult, QTextBlock>> (const HighlightingResult &, const QTextBlock &)>;

// Applies the future results [from, to) and applies the extra formats
// indicated by Result::kind and kindToFormat to the correct location using
// SyntaxHighlighter::setExtraAdditionalFormats.
// It is incremental in the sense that it clears the extra additional formats
// from all lines that have no results between the (from-1).line result and
// the (to-1).line result.
// Requires that results of the Future are ordered by line.
TEXTEDITOR_EXPORT auto incrementalApplyExtraAdditionalFormats(SyntaxHighlighter *highlighter, const QFuture<HighlightingResult> &future, int from, int to, const QHash<int, QTextCharFormat> &kindToFormat, const Splitter &splitter = {}) -> void;

// Clears all extra highlights and applies the extra formats
// indicated by Result::kind and kindToFormat to the correct location using
// SyntaxHighlighter::setExtraFormats. In contrast to
// incrementalApplyExtraAdditionalFormats the results do not have to be ordered by line.
TEXTEDITOR_EXPORT auto setExtraAdditionalFormats(SyntaxHighlighter *highlighter, const HighlightingResults &results, const QHash<int, QTextCharFormat> &kindToFormat) -> void;

// Cleans the extra additional formats after the last result of the Future
// until the end of the document.
// Requires that results of the Future are ordered by line.
TEXTEDITOR_EXPORT auto clearExtraAdditionalFormatsUntilEnd(SyntaxHighlighter *highlighter, const QFuture<HighlightingResult> &future) -> void;


} // namespace SemanticHighlighter
} // namespace TextEditor