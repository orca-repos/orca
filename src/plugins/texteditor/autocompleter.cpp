// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "autocompleter.hpp"
#include "textdocumentlayout.hpp"
#include "tabsettings.hpp"

#include <QDebug>
#include <QTextCursor>

using namespace TextEditor;

AutoCompleter::AutoCompleter() : m_allowSkippingOfBlockEnd(false), m_autoInsertBrackets(true), m_surroundWithBrackets(true), m_autoInsertQuotes(true), m_surroundWithQuotes(true), m_overwriteClosingChars(false) {}

AutoCompleter::~AutoCompleter() = default;

static auto countBracket(QChar open, QChar close, QChar c, int *errors, int *stillopen) -> void
{
  if (c == open)
    ++*stillopen;
  else if (c == close)
    --*stillopen;

  if (*stillopen < 0) {
    *errors += -1 * *stillopen;
    *stillopen = 0;
  }
}

static auto countBrackets(QTextCursor cursor, int from, int end, QChar open, QChar close, int *errors, int *stillopen) -> void
{
  cursor.setPosition(from);
  auto block = cursor.block();
  while (block.isValid() && block.position() < end) {
    auto parenList = TextDocumentLayout::parentheses(block);
    if (!parenList.isEmpty() && !TextDocumentLayout::ifdefedOut(block)) {
      for (auto i = 0; i < parenList.count(); ++i) {
        const auto paren = parenList.at(i);
        const auto position = block.position() + paren.pos;
        if (position < from || position >= end)
          continue;
        countBracket(open, close, paren.chr, errors, stillopen);
      }
    }
    block = block.next();
  }
}

enum class CharType {
  OpenChar,
  CloseChar
};

static auto charType(const QChar &c, CharType type) -> QChar
{
  switch (c.unicode()) {
  case '(':
  case ')':
    return type == CharType::OpenChar ? QLatin1Char('(') : QLatin1Char(')');
  case '[':
  case ']':
    return type == CharType::OpenChar ? QLatin1Char('[') : QLatin1Char(']');
  case '{':
  case '}':
    return type == CharType::OpenChar ? QLatin1Char('{') : QLatin1Char('}');
  }
  return QChar();
}

static auto fixesBracketsError(const QString &textToInsert, const QTextCursor &cursor) -> bool
{
  const auto character = textToInsert.at(0);
  const QString allParentheses = QLatin1String("()[]{}");
  if (!allParentheses.contains(character))
    return false;

  auto tmp = cursor;
  const auto foundBlockStart = TextBlockUserData::findPreviousBlockOpenParenthesis(&tmp);
  const auto blockStart = foundBlockStart ? tmp.position() : 0;
  tmp = cursor;
  const auto foundBlockEnd = TextBlockUserData::findNextBlockClosingParenthesis(&tmp);
  const auto blockEnd = foundBlockEnd ? tmp.position() : cursor.document()->characterCount() - 1;
  const auto openChar = charType(character, CharType::OpenChar);
  const auto closeChar = charType(character, CharType::CloseChar);

  auto errors = 0;
  auto stillopen = 0;
  countBrackets(cursor, blockStart, blockEnd, openChar, closeChar, &errors, &stillopen);
  const auto errorsBeforeInsertion = errors + stillopen;
  errors = 0;
  stillopen = 0;
  countBrackets(cursor, blockStart, cursor.position(), openChar, closeChar, &errors, &stillopen);
  countBracket(openChar, closeChar, character, &errors, &stillopen);
  countBrackets(cursor, cursor.position(), blockEnd, openChar, closeChar, &errors, &stillopen);
  const auto errorsAfterInsertion = errors + stillopen;
  return errorsAfterInsertion < errorsBeforeInsertion;
}

static auto surroundSelectionWithBrackets(const QString &textToInsert, const QString &selection) -> QString
{
  QString replacement;
  if (textToInsert == QLatin1String("(")) {
    replacement = selection + QLatin1Char(')');
  } else if (textToInsert == QLatin1String("[")) {
    replacement = selection + QLatin1Char(']');
  } else if (textToInsert == QLatin1String("{")) {
    //If the text spans multiple lines, insert on different lines
    replacement = selection;
    if (selection.contains(QChar::ParagraphSeparator)) {
      //Also, try to simulate auto-indent
      replacement = (selection.startsWith(QChar::ParagraphSeparator) ? QString() : QString(QChar::ParagraphSeparator)) + selection;
      if (replacement.endsWith(QChar::ParagraphSeparator))
        replacement += QLatin1Char('}') + QString(QChar::ParagraphSeparator);
      else
        replacement += QString(QChar::ParagraphSeparator) + QLatin1Char('}');
    } else {
      replacement += QLatin1Char('}');
    }
  }
  return replacement;
}

auto AutoCompleter::isQuote(const QString &text) -> bool
{
  return text == QLatin1String("\"") || text == QLatin1String("'");
}

auto AutoCompleter::isNextBlockIndented(const QTextBlock &currentBlock) const -> bool
{
  auto block = currentBlock;
  const auto indentation = m_tabSettings.indentationColumn(block.text());

  if (block.next().isValid()) {
    // not the last block
    block = block.next();
    //skip all empty blocks
    while (block.isValid() && TabSettings::onlySpace(block.text()))
      block = block.next();
    if (block.isValid() && m_tabSettings.indentationColumn(block.text()) > indentation)
      return true;
  }

  return false;
}

auto AutoCompleter::replaceSelection(QTextCursor &cursor, const QString &textToInsert) const -> QString
{
  if (!cursor.hasSelection())
    return QString();
  if (isQuote(textToInsert) && m_surroundWithQuotes)
    return cursor.selectedText() + textToInsert;
  if (m_surroundWithBrackets)
    return surroundSelectionWithBrackets(textToInsert, cursor.selectedText());
  return QString();
}

auto AutoCompleter::autoComplete(QTextCursor &cursor, const QString &textToInsert, bool skipChars) const -> QString
{
  const auto checkBlockEnd = m_allowSkippingOfBlockEnd;
  m_allowSkippingOfBlockEnd = false; // consume blockEnd.

  auto autoText = replaceSelection(cursor, textToInsert);
  if (!autoText.isEmpty())
    return autoText;

  const auto doc = cursor.document();
  const auto lookAhead = doc->characterAt(cursor.selectionEnd());

  if (m_overwriteClosingChars && textToInsert == lookAhead)
    skipChars = true;

  auto skippedChars = 0;

  if (isQuote(textToInsert) && m_autoInsertQuotes && contextAllowsAutoQuotes(cursor, textToInsert)) {
    autoText = insertMatchingQuote(cursor, textToInsert, lookAhead, skipChars, &skippedChars);
  } else if (m_autoInsertBrackets && contextAllowsAutoBrackets(cursor, textToInsert)) {
    if (fixesBracketsError(textToInsert, cursor))
      return QString();

    autoText = insertMatchingBrace(cursor, textToInsert, lookAhead, skipChars, &skippedChars);

    if (checkBlockEnd && textToInsert.at(0) == QLatin1Char('}')) {
      if (textToInsert.length() > 1)
        qWarning() << "*** handle event compression";

      auto startPos = cursor.selectionEnd(), pos = startPos;
      while (doc->characterAt(pos).isSpace())
        ++pos;

      if (doc->characterAt(pos) == QLatin1Char('}') && skipChars)
        skippedChars += pos - startPos + 1;
    }
  } else {
    return QString();
  }

  if (skipChars && skippedChars) {
    const auto pos = cursor.position();
    cursor.setPosition(pos + skippedChars);
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
  }

  return autoText;
}

auto AutoCompleter::autoBackspace(QTextCursor &cursor) -> bool
{
  m_allowSkippingOfBlockEnd = false;

  if (!m_autoInsertBrackets)
    return false;

  const auto pos = cursor.position();
  if (pos == 0)
    return false;
  auto c = cursor;
  c.setPosition(pos - 1);

  const auto doc = cursor.document();
  const auto lookAhead = doc->characterAt(pos);
  const auto lookBehind = doc->characterAt(pos - 1);
  const auto lookFurtherBehind = doc->characterAt(pos - 2);

  const auto character = lookBehind;
  if (character == QLatin1Char('(') || character == QLatin1Char('[') || character == QLatin1Char('{')) {
    auto tmp = cursor;
    TextBlockUserData::findPreviousBlockOpenParenthesis(&tmp);
    const auto blockStart = tmp.isNull() ? 0 : tmp.position();
    tmp = cursor;
    TextBlockUserData::findNextBlockClosingParenthesis(&tmp);
    const auto blockEnd = tmp.isNull() ? cursor.document()->characterCount() - 1 : tmp.position();
    const auto openChar = character;
    const auto closeChar = charType(character, CharType::CloseChar);

    auto errors = 0;
    auto stillopen = 0;
    countBrackets(cursor, blockStart, blockEnd, openChar, closeChar, &errors, &stillopen);
    const auto errorsBeforeDeletion = errors + stillopen;
    errors = 0;
    stillopen = 0;
    countBrackets(cursor, blockStart, pos - 1, openChar, closeChar, &errors, &stillopen);
    countBrackets(cursor, pos, blockEnd, openChar, closeChar, &errors, &stillopen);
    const auto errorsAfterDeletion = errors + stillopen;

    if (errorsAfterDeletion < errorsBeforeDeletion)
      return false; // insertion fixes parentheses or bracket errors, do not auto complete
  }

  // ### this code needs to be generalized
  if (lookBehind == QLatin1Char('(') && lookAhead == QLatin1Char(')') || lookBehind == QLatin1Char('[') && lookAhead == QLatin1Char(']') || lookBehind == QLatin1Char('{') && lookAhead == QLatin1Char('}') || lookBehind == QLatin1Char('"') && lookAhead == QLatin1Char('"') && lookFurtherBehind != QLatin1Char('\\') || lookBehind == QLatin1Char('\'') && lookAhead == QLatin1Char('\'') && lookFurtherBehind != QLatin1Char('\\')) {
    if (! isInComment(c)) {
      cursor.beginEditBlock();
      cursor.deleteChar();
      cursor.deletePreviousChar();
      cursor.endEditBlock();
      return true;
    }
  }
  return false;
}

auto AutoCompleter::paragraphSeparatorAboutToBeInserted(QTextCursor &cursor) -> int
{
  if (!m_autoInsertBrackets)
    return 0;

  const auto doc = cursor.document();
  if (doc->characterAt(cursor.position() - 1) != QLatin1Char('{'))
    return 0;

  if (!contextAllowsAutoBrackets(cursor))
    return 0;

  // verify that we indeed do have an extra opening brace in the document
  const auto block = cursor.block();
  const auto textFromCusror = block.text().mid(cursor.positionInBlock()).trimmed();
  const auto braceDepth = TextDocumentLayout::braceDepth(doc->lastBlock());

  if (braceDepth <= 0 && (textFromCusror.isEmpty() || textFromCusror.at(0) != QLatin1Char('}')))
    return 0; // braces are all balanced or worse, no need to do anything and separator inserted not between '{' and '}'

  // we have an extra brace , let's see if we should close it

  /* verify that the next block is not further intended compared to the current block.
     This covers the following case:

          if (condition) {|
              statement;
  */
  if (isNextBlockIndented(block))
    return 0;

  const auto &textToInsert = insertParagraphSeparator(cursor);
  const auto pos = cursor.position();
  cursor.insertBlock();
  cursor.insertText(textToInsert);
  cursor.setPosition(pos);

  // if we actually insert a separator, allow it to be overwritten if
  // user types it
  if (!textToInsert.isEmpty())
    m_allowSkippingOfBlockEnd = true;

  return 1;
}

auto AutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor, const QString &textToInsert) const -> bool
{
  Q_UNUSED(cursor)
  Q_UNUSED(textToInsert)
  return false;
}

auto AutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor, const QString &textToInsert) const -> bool
{
  Q_UNUSED(cursor)
  Q_UNUSED(textToInsert)
  return false;
}

auto AutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const -> bool
{
  return contextAllowsAutoBrackets(cursor);
}

auto AutoCompleter::isInComment(const QTextCursor &cursor) const -> bool
{
  Q_UNUSED(cursor)
  return false;
}

auto AutoCompleter::isInString(const QTextCursor &cursor) const -> bool
{
  Q_UNUSED(cursor)
  return false;
}

auto AutoCompleter::insertMatchingBrace(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString
{
  Q_UNUSED(cursor)
  Q_UNUSED(text)
  Q_UNUSED(lookAhead)
  Q_UNUSED(skipChars)
  Q_UNUSED(skippedChars)
  return QString();
}

auto AutoCompleter::insertMatchingQuote(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString
{
  Q_UNUSED(cursor)
  Q_UNUSED(text)
  Q_UNUSED(lookAhead)
  Q_UNUSED(skipChars)
  Q_UNUSED(skippedChars)
  return QString();
}

auto AutoCompleter::insertParagraphSeparator(const QTextCursor &cursor) const -> QString
{
  Q_UNUSED(cursor)
  return QString();
}
