// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeautocompleter.hpp"

#include <QRegularExpression>

namespace CMakeProjectManager {
namespace Internal {

CMakeAutoCompleter::CMakeAutoCompleter()
{
  setAutoInsertBracketsEnabled(true);
}

auto CMakeAutoCompleter::isInComment(const QTextCursor &cursor) const -> bool
{
  // NOTE: This doesn't handle '#' inside quotes, nor multi-line comments
  auto moved = cursor;
  moved.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
  return moved.selectedText().contains(QLatin1Char('#'));
}

auto CMakeAutoCompleter::isInString(const QTextCursor &cursor) const -> bool
{
  // NOTE: multiline strings are currently not supported, since they rarely, if ever, seem to be used
  auto moved = cursor;
  moved.movePosition(QTextCursor::StartOfLine);
  const auto positionInLine = cursor.position() - moved.position();
  moved.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  const auto line = moved.selectedText();

  auto isEscaped = false;
  auto inString = false;
  for (auto i = 0; i < positionInLine; ++i) {
    const auto c = line.at(i);
    if (c == QLatin1Char('\\') && !isEscaped)
      isEscaped = true;
    else if (c == QLatin1Char('"') && !isEscaped)
      inString = !inString;
    else
      isEscaped = false;
  }
  return inString;
}

auto CMakeAutoCompleter::insertMatchingBrace(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString
{
  Q_UNUSED(cursor)
  if (text.isEmpty())
    return QString();
  const auto current = text.at(0);
  switch (current.unicode()) {
  case '(':
    return QStringLiteral(")");

  case ')':
    if (current == lookAhead && skipChars)
      ++*skippedChars;
    break;

  default:
    break;
  }

  return QString();
}

auto CMakeAutoCompleter::insertMatchingQuote(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString
{
  Q_UNUSED(cursor)
  static const QChar quote(QLatin1Char('"'));
  if (text.isEmpty() || text != quote)
    return QString();
  if (lookAhead == quote && skipChars) {
    ++*skippedChars;
    return QString();
  }
  return quote;
}

auto CMakeAutoCompleter::paragraphSeparatorAboutToBeInserted(QTextCursor &cursor) -> int
{
  const auto line = cursor.block().text().trimmed();
  if (line.contains(QRegularExpression(QStringLiteral("^(endfunction|endmacro|endif|endforeach|endwhile)\\w*\\("))))
    tabSettings().indentLine(cursor.block(), tabSettings().indentationColumn(cursor.block().text()));
  return 0;
}

auto CMakeAutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor, const QString &textToInsert) const -> bool
{
  if (textToInsert.isEmpty())
    return false;

  const auto c = textToInsert.at(0);
  if (c == QLatin1Char('(') || c == QLatin1Char(')'))
    return !isInComment(cursor);
  return false;
}

auto CMakeAutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor, const QString &textToInsert) const -> bool
{
  if (textToInsert.isEmpty())
    return false;

  const auto c = textToInsert.at(0);
  if (c == QLatin1Char('"'))
    return !isInComment(cursor);
  return false;
}

auto CMakeAutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const -> bool
{
  return !isInComment(cursor) && !isInString(cursor);
}

} // namespace Internal
} // namespace CMakeProjectManager
