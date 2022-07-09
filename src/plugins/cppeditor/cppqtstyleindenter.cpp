// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppqtstyleindenter.hpp"

#include "cppcodeformatter.hpp"
#include "cpptoolssettings.hpp"
#include "cppcodestylepreferences.hpp"

#include <QChar>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

namespace CppEditor::Internal {

CppQtStyleIndenter::CppQtStyleIndenter(QTextDocument *doc) : TextEditor::TextIndenter(doc)
{
  // Just for safety. setCodeStylePreferences should be called when the editor the
  // indenter belongs to gets initialized.
  m_cppCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
}

CppQtStyleIndenter::~CppQtStyleIndenter() = default;

auto CppQtStyleIndenter::isElectricCharacter(const QChar &ch) const -> bool
{
  switch (ch.toLatin1()) {
  case '{':
  case '}':
  case ':':
  case '#':
  case '<':
  case '>':
  case ';':
    return true;
  }
  return false;
}

static auto isElectricInLine(const QChar ch, const QString &text) -> bool
{
  switch (ch.toLatin1()) {
  case ';':
    return text.contains(QLatin1String("break"));
  case ':':
    // switch cases and access declarations should be reindented
    if (text.contains(QLatin1String("case")) || text.contains(QLatin1String("default")) || text.contains(QLatin1String("public")) || text.contains(QLatin1String("private")) || text.contains(QLatin1String("protected")) || text.contains(QLatin1String("signals")) || text.contains(QLatin1String("Q_SIGNALS"))) {
      return true;
    }

    Q_FALLTHROUGH();
  // lines that start with : might have a constructor initializer list
  case '<':
  case '>': {
    // Electric if at line beginning (after space indentation)
    for (int i = 0, len = text.count(); i < len; ++i) {
      if (!text.at(i).isSpace())
        return text.at(i) == ch;
    }
    return false;
  }
  }

  return true;
}

auto CppQtStyleIndenter::indentBlock(const QTextBlock &block, const QChar &typedChar, const TextEditor::TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> void
{
  QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

  codeFormatter.updateStateUntil(block);
  if (codeFormatter.isInRawStringLiteral(block))
    return;
  int indent;
  int padding;
  codeFormatter.indentFor(block, &indent, &padding);

  if (isElectricCharacter(typedChar)) {
    // : should not be electric for labels
    if (!isElectricInLine(typedChar, block.text()))
      return;

    // only reindent the current line when typing electric characters if the
    // indent is the same it would be if the line were empty
    int newlineIndent;
    int newlinePadding;
    codeFormatter.indentForNewLineAfter(block.previous(), &newlineIndent, &newlinePadding);
    if (tabSettings.indentationColumn(block.text()) != newlineIndent + newlinePadding)
      return;
  }

  tabSettings.indentLine(block, indent + padding, padding);
}

auto CppQtStyleIndenter::indent(const QTextCursor &cursor, const QChar &typedChar, const TextEditor::TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> void
{
  if (cursor.hasSelection()) {
    auto block = m_doc->findBlock(cursor.selectionStart());
    const auto end = m_doc->findBlock(cursor.selectionEnd()).next();

    QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());
    codeFormatter.updateStateUntil(block);

    auto tc = cursor;
    tc.beginEditBlock();
    do {
      if (!codeFormatter.isInRawStringLiteral(block)) {
        int indent;
        int padding;
        codeFormatter.indentFor(block, &indent, &padding);
        tabSettings.indentLine(block, indent + padding, padding);
      }
      codeFormatter.updateLineStateChange(block);
      block = block.next();
    } while (block.isValid() && block != end);
    tc.endEditBlock();
  } else {
    indentBlock(cursor.block(), typedChar, tabSettings);
  }
}

auto CppQtStyleIndenter::setCodeStylePreferences(TextEditor::ICodeStylePreferences *preferences) -> void
{
  auto cppCodeStylePreferences = qobject_cast<CppCodeStylePreferences*>(preferences);
  if (cppCodeStylePreferences)
    m_cppCodeStylePreferences = cppCodeStylePreferences;
}

auto CppQtStyleIndenter::invalidateCache() -> void
{
  QtStyleCodeFormatter formatter;
  formatter.invalidateCache(m_doc);
}

auto CppQtStyleIndenter::indentFor(const QTextBlock &block, const TextEditor::TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> int
{
  QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

  codeFormatter.updateStateUntil(block);
  int indent;
  int padding;
  codeFormatter.indentFor(block, &indent, &padding);

  return indent;
}

auto CppQtStyleIndenter::codeStyleSettings() const -> CppCodeStyleSettings
{
  if (m_cppCodeStylePreferences)
    return m_cppCodeStylePreferences->currentCodeStyleSettings();
  return {};
}

auto CppQtStyleIndenter::indentationForBlocks(const QVector<QTextBlock> &blocks, const TextEditor::TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> TextEditor::IndentationForBlock
{
  QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

  codeFormatter.updateStateUntil(blocks.last());

  TextEditor::IndentationForBlock ret;
  foreach(QTextBlock block, blocks) {
    int indent;
    int padding;
    codeFormatter.indentFor(block, &indent, &padding);
    ret.insert(block.blockNumber(), indent);
  }
  return ret;
}

} // namespace CppEditor::Internal
