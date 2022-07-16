// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textindenter.hpp"

#include <QTextDocument>
#include <QTextCursor>

using namespace TextEditor;

TextIndenter::TextIndenter(QTextDocument *doc) : Indenter(doc) {}

TextIndenter::~TextIndenter() = default;

// Indent a text block based on previous line.
// Simple text paragraph layout:
// aaaa aaaa
//
//   bbb bb
//   bbb bb
//
//  - list
//    list line2
//
//  - listn
//
// ccc
//
// @todo{Add formatting to wrap paragraphs. This requires some
// hoops as the current indentation routines are not prepared
// for additional block being inserted. It might be possible
// to do in 2 steps (indenting/wrapping)}
auto TextIndenter::indentFor(const QTextBlock &block, const TabSettings &tabSettings, int cursorPositionInEditor) -> int
{
  Q_UNUSED(tabSettings)
  Q_UNUSED(cursorPositionInEditor)

  const auto previous = block.previous();
  if (!previous.isValid())
    return 0;

  const auto previousText = previous.text();
  // Empty line indicates a start of a new paragraph. Leave as is.
  if (previousText.isEmpty() || previousText.trimmed().isEmpty())
    return 0;

  return tabSettings.indentationColumn(previousText);
}

auto TextIndenter::indentationForBlocks(const QVector<QTextBlock> &blocks, const TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> IndentationForBlock
{
  IndentationForBlock ret;
  for (const auto &block : blocks)
    ret.insert(block.blockNumber(), indentFor(block, tabSettings));
  return ret;
}

auto TextIndenter::indentBlock(const QTextBlock &block, const QChar &typedChar, const TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> void
{
  Q_UNUSED(typedChar)
  const auto indent = indentFor(block, tabSettings);
  if (indent < 0)
    return;
  tabSettings.indentLine(block, indent);
}

auto TextIndenter::indent(const QTextCursor &cursor, const QChar &typedChar, const TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> void
{
  if (cursor.hasSelection()) {
    auto block = m_doc->findBlock(cursor.selectionStart());
    const auto end = m_doc->findBlock(cursor.selectionEnd()).next();
    do {
      indentBlock(block, typedChar, tabSettings);
      block = block.next();
    } while (block.isValid() && block != end);
  } else {
    indentBlock(cursor.block(), typedChar, tabSettings);
  }
}

auto TextIndenter::reindent(const QTextCursor &cursor, const TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> void
{
  if (cursor.hasSelection()) {
    auto block = m_doc->findBlock(cursor.selectionStart());
    const auto end = m_doc->findBlock(cursor.selectionEnd()).next();

    // skip empty blocks
    while (block.isValid() && block != end) {
      auto bt = block.text();
      if (TabSettings::firstNonSpace(bt) < bt.size())
        break;
      indentBlock(block, QChar::Null, tabSettings);
      block = block.next();
    }

    const auto previousIndentation = tabSettings.indentationColumn(block.text());
    indentBlock(block, QChar::Null, tabSettings);
    const auto currentIndentation = tabSettings.indentationColumn(block.text());
    const auto delta = currentIndentation - previousIndentation;

    block = block.next();
    while (block.isValid() && block != end) {
      tabSettings.reindentLine(block, delta);
      block = block.next();
    }
  } else {
    indentBlock(cursor.block(), QChar::Null, tabSettings);
  }
}

auto TextIndenter::tabSettings() const -> Utils::optional<TabSettings>
{
  return Utils::optional<TabSettings>();
}
