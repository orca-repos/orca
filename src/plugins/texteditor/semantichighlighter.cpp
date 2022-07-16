// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "semantichighlighter.hpp"

#include "syntaxhighlighter.hpp"
#include "texteditorsettings.hpp"

#include <utils/qtcassert.hpp>

#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

using namespace TextEditor;
using namespace SemanticHighlighter;

namespace {

class Range {
public:
  QTextLayout::FormatRange formatRange;
  QTextBlock block;
};

using Ranges = QVector<Range>;

auto rangesForResult(const HighlightingResult &result, const QTextBlock &startBlock, const QHash<int, QTextCharFormat> &kindToFormat) -> const Ranges
{
  const auto format = result.useTextSyles ? TextEditorSettings::fontSettings().toTextCharFormat(result.textStyles) : kindToFormat.value(result.kind);
  if (!format.isValid())
    return {};

  auto curResult = result;
  auto curBlock = startBlock;
  Ranges ranges;
  while (curBlock.isValid()) {
    Range range;
    range.block = curBlock;
    range.formatRange.format = format;
    range.formatRange.start = curResult.column - 1;
    range.formatRange.length = std::min(curResult.length, curBlock.length() - range.formatRange.start);
    ranges << range;
    if (range.formatRange.length == curResult.length)
      break;
    curBlock = curBlock.next();
    curResult.column = 1;
    curResult.length -= range.formatRange.length;
  }

  return ranges;
}

auto rangesForResult(const HighlightingResult &result, QTextDocument *doc, const QHash<int, QTextCharFormat> &kindToFormat, const Splitter &splitter = {}) -> const Ranges
{
  const auto startBlock = doc->findBlockByNumber(result.line - 1);
  if (splitter) {
    Ranges ranges;
    for (const auto &[newResult, newBlock] : splitter(result, startBlock))
      ranges << rangesForResult(newResult, newBlock, kindToFormat);
    return ranges;
  }
  return rangesForResult(result, startBlock, kindToFormat);
}

}

auto SemanticHighlighter::incrementalApplyExtraAdditionalFormats(SyntaxHighlighter *highlighter, const QFuture<HighlightingResult> &future, int from, int to, const QHash<int, QTextCharFormat> &kindToFormat, const Splitter &splitter) -> void
{
  if (to <= from)
    return;

  const auto firstResultBlockNumber = int(future.resultAt(from).line) - 1;

  // blocks between currentBlockNumber and the last block with results will
  // be cleaned of additional extra formats if they have no results
  auto currentBlockNumber = 0;
  for (auto i = from - 1; i >= 0; --i) {
    const auto &result = future.resultAt(i);
    const auto blockNumber = int(result.line) - 1;
    if (blockNumber < firstResultBlockNumber) {
      // stop! found where last format stopped
      currentBlockNumber = blockNumber + 1;
      // add previous results for the same line to avoid undoing their formats
      from = i + 1;
      break;
    }
  }

  const auto doc = highlighter->document();
  QTC_ASSERT(currentBlockNumber < doc->blockCount(), return);
  auto currentBlock = doc->findBlockByNumber(currentBlockNumber);

  std::map<QTextBlock, QVector<QTextLayout::FormatRange>> formatRanges;
  for (auto i = from; i < to; ++i) {
    for (const auto &range : rangesForResult(future.resultAt(i), doc, kindToFormat, splitter))
      formatRanges[range.block].append(range.formatRange);
  }

  for (auto &[block, ranges] : formatRanges) {
    while (currentBlock < block) {
      highlighter->clearExtraFormats(currentBlock);
      currentBlock = currentBlock.next();
    }
    highlighter->setExtraFormats(block, std::move(ranges));
    currentBlock = block.next();
  }
}

auto SemanticHighlighter::setExtraAdditionalFormats(SyntaxHighlighter *highlighter, const QList<HighlightingResult> &results, const QHash<int, QTextCharFormat> &kindToFormat) -> void
{
  if (!highlighter)
    return;
  highlighter->clearAllExtraFormats();

  const auto doc = highlighter->document();
  QTC_ASSERT(doc, return);

  std::map<QTextBlock, QVector<QTextLayout::FormatRange>> formatRanges;

  for (auto result : results) {
    for (const auto &range : rangesForResult(result, doc, kindToFormat))
      formatRanges[range.block].append(range.formatRange);
  }

  for (auto &[block, ranges] : formatRanges)
    highlighter->setExtraFormats(block, std::move(ranges));
}

auto SemanticHighlighter::clearExtraAdditionalFormatsUntilEnd(SyntaxHighlighter *highlighter, const QFuture<HighlightingResult> &future) -> void
{
  const QTextDocument *const doc = highlighter->document();
  auto firstBlockToClear = doc->begin();
  for (auto i = future.resultCount() - 1; i >= 0; --i) {
    const auto &result = future.resultAt(i);
    if (result.line) {
      const auto blockForLine = doc->findBlockByNumber(result.line - 1);
      const auto lastBlockWithResults = doc->findBlock(blockForLine.position() + result.column - 1 + result.length);
      firstBlockToClear = lastBlockWithResults.next();
      break;
    }
  }

  for (auto b = firstBlockToClear; b.isValid(); b = b.next())
    highlighter->clearExtraFormats(b);
}
