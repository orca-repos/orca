// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectmacro.hpp"

#include <utils/algorithm.hpp>
#include <cctype>

namespace ProjectExplorer {

auto Macro::isValid() const -> bool
{
  return !key.isEmpty() && type != MacroType::Invalid;
}

auto Macro::toByteArray() const -> QByteArray
{
  switch (type) {
  case MacroType::Define: {
    if (value.isEmpty())
      return QByteArray("#define ") + key;
    return QByteArray("#define ") + key + ' ' + value;
  }
  case MacroType::Undefine:
    return QByteArray("#undef ") + key;
  case MacroType::Invalid:
    break;
  }

  return QByteArray();
}

auto Macro::toByteArray(const Macros &macros) -> QByteArray
{
  QByteArray text;

  for (const auto &macro : macros) {
    const auto macroText = macro.toByteArray();
    if (!macroText.isEmpty())
      text += macroText + '\n';
  }

  return text;
}

auto Macro::toByteArray(const QVector<Macros> &macrosVector) -> QByteArray
{
  QByteArray text;

  for (const auto &macros : macrosVector)
    text += toByteArray(macros);

  return text;
}

auto Macro::toMacros(const QByteArray &text) -> Macros
{
  return tokensLinesToMacros(tokenizeLines(splitLines(text)));
}

auto Macro::fromKeyValue(const QString &utf16text) -> Macro
{
  return fromKeyValue(utf16text.toUtf8());
}

auto Macro::fromKeyValue(const QByteArray &text) -> Macro
{
  QByteArray key;
  QByteArray value;
  auto type = MacroType::Invalid;

  if (!text.isEmpty()) {
    type = MacroType::Define;

    const int index = text.indexOf('=');

    if (index != -1) {
      key = text.left(index).trimmed();
      value = text.mid(index + 1).trimmed();
    } else {
      key = text.trimmed();
      value = "1";
    }
  }

  return Macro(key, value, type);
}

auto Macro::toKeyValue(const QByteArray &prefix) const -> QByteArray
{
  QByteArray keyValue;
  if (type != MacroType::Invalid)
    keyValue = prefix;

  if (value.isEmpty())
    keyValue += key + '=';
  else if (value == "1")
    keyValue += key;
  else
    keyValue += key + '=' + value;

  return keyValue;
}

static auto removeCarriageReturn(QByteArray &line) -> void
{
  if (line.endsWith('\r'))
    line.truncate(line.size() - 1);
}

static auto removeCarriageReturns(QList<QByteArray> &lines) -> void
{
  for (auto &line : lines)
    removeCarriageReturn(line);
}

auto Macro::splitLines(const QByteArray &text) -> QList<QByteArray>
{
  auto splitLines = text.split('\n');

  splitLines.removeAll("");
  removeCarriageReturns(splitLines);

  return splitLines;
}

auto Macro::removeNonsemanticSpaces(QByteArray line) -> QByteArray
{
  const auto begin = line.begin();
  const auto end = line.end();
  auto notInString = true;

  const auto newEnd = std::unique(begin, end, [&](char first, char second) {
    notInString = notInString && first != '\"';
    return notInString && (first == '#' || std::isspace(first)) && std::isspace(second);
  });

  line.truncate(line.size() - int(std::distance(newEnd, end)));

  return line.trimmed();
}

auto Macro::tokenizeLine(const QByteArray &line) -> QList<QByteArray>
{
  const auto normalizedLine = removeNonsemanticSpaces(line);

  const auto begin = normalizedLine.begin();
  auto first = std::find(normalizedLine.begin(), normalizedLine.end(), ' ');
  const auto end = normalizedLine.end();

  QList<QByteArray> tokens;

  if (first != end) {
    auto second = std::find(std::next(first), normalizedLine.end(), ' ');
    tokens.append(QByteArray(begin, int(std::distance(begin, first))));

    std::advance(first, 1);
    tokens.append(QByteArray(first, int(std::distance(first, second))));

    if (second != end) {
      std::advance(second, 1);
      tokens.append(QByteArray(second, int(std::distance(second, end))));
    }
  }

  return tokens;
}

auto Macro::tokenizeLines(const QList<QByteArray> &lines) -> QList<QList<QByteArray>>
{
  auto tokensLines = Utils::transform(lines, &Macro::tokenizeLine);

  return tokensLines;
}

auto Macro::tokensToMacro(const QList<QByteArray> &tokens) -> Macro
{
  Macro macro;

  if (tokens.size() >= 2 && tokens[0] == "#define") {
    macro.type = MacroType::Define;
    macro.key = tokens[1];

    if (tokens.size() >= 3)
      macro.value = tokens[2];
  }

  return macro;
}

auto Macro::tokensLinesToMacros(const QList<QList<QByteArray>> &tokensLines) -> Macros
{
  Macros macros;
  macros.reserve(tokensLines.size());

  for (const auto &tokens : tokensLines) {
    auto macro = tokensToMacro(tokens);

    if (macro.type != MacroType::Invalid)
      macros.push_back(std::move(macro));
  }

  return macros;
}

} // namespace ProjectExplorer
