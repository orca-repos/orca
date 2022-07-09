// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeindenter.hpp"

namespace CMakeProjectManager {
namespace Internal {

CMakeIndenter::CMakeIndenter(QTextDocument *doc)
    : TextEditor::TextIndenter(doc)
{}

auto CMakeIndenter::isElectricCharacter(const QChar &ch) const -> bool
{
    return ch == QLatin1Char('(') || ch == QLatin1Char(')');
}

static auto startsWithChar(const QString &line, char character) -> int
{
  auto occurrences = 0;
  for (auto i = 0; i < line.size(); ++i) {
    if (line.at(i) == character) {
      occurrences++;
    } else if (!line.at(i).isSpace()) {
      break;
    }
  }
  return occurrences;
}

static auto lineContainsFunction(const QString &line, const QString &function) -> bool
{
  const int indexOfFunction = line.indexOf(function);
  if (indexOfFunction == -1)
    return false;
  for (auto i = 0; i < indexOfFunction; ++i) {
    if (!line.at(i).isSpace())
      return false;
  }
  for (int i = indexOfFunction + function.size(); i < line.size(); ++i) {
    if (line.at(i) == QLatin1Char('('))
      return true;
    else if (!line.at(i).isSpace())
      return false;
  }
  return false;
}

static auto lineStartsBlock(const QString &line) -> bool
{
  return lineContainsFunction(line, QStringLiteral("function")) || lineContainsFunction(line, QStringLiteral("macro")) || lineContainsFunction(line, QStringLiteral("foreach")) || lineContainsFunction(line, QStringLiteral("while")) || lineContainsFunction(line, QStringLiteral("if")) || lineContainsFunction(line, QStringLiteral("elseif")) || lineContainsFunction(line, QStringLiteral("else"));
}

static auto lineEndsBlock(const QString &line) -> bool
{
  return lineContainsFunction(line, QStringLiteral("endfunction")) || lineContainsFunction(line, QStringLiteral("endmacro")) || lineContainsFunction(line, QStringLiteral("endforeach")) || lineContainsFunction(line, QStringLiteral("endwhile")) || lineContainsFunction(line, QStringLiteral("endif")) || lineContainsFunction(line, QStringLiteral("elseif")) || lineContainsFunction(line, QStringLiteral("else"));
}

static auto lineIsEmpty(const QString &line) -> bool
{
  for (const auto &c : line) {
    if (!c.isSpace())
      return false;
  }
  return true;
}

static auto paranthesesLevel(const QString &line) -> int
{
  const auto beforeComment = line.mid(0, line.indexOf(QLatin1Char('#')));
  const int opening = beforeComment.count(QLatin1Char('('));
  const int closing = beforeComment.count(QLatin1Char(')'));

  return opening - closing;
}

auto CMakeIndenter::indentFor(const QTextBlock &block, const TextEditor::TabSettings &tabSettings, int /*cursorPositionInEditor*/) -> int
{
  auto previousBlock = block.previous();
  // find the next previous block that is non-empty (contains non-whitespace characters)
  while (previousBlock.isValid() && lineIsEmpty(previousBlock.text()))
    previousBlock = previousBlock.previous();
  if (!previousBlock.isValid())
    return 0;

  const auto previousLine = previousBlock.text();
  const auto currentLine = block.text();
  auto indentation = tabSettings.indentationColumn(previousLine);

  if (lineStartsBlock(previousLine))
    indentation += tabSettings.m_indentSize;
  if (lineEndsBlock(currentLine))
    indentation -= tabSettings.m_indentSize;

  // de-dent lines that start with closing parantheses immediately
  indentation -= tabSettings.m_indentSize * startsWithChar(currentLine, ')');

  if (auto paranthesesCount = paranthesesLevel(previousLine) - startsWithChar(previousLine, ')'))
    indentation += tabSettings.m_indentSize * (paranthesesCount > 0 ? 1 : -1);
  return qMax(0, indentation);
}

} // namespace Internal
} // namespace CMakeProjectManager


