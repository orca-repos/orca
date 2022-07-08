// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippet.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/templateengine.hpp>

#include <QTextDocument>

using namespace TextEditor;

const char UCMANGLER_ID[] = "TextEditor::UppercaseMangler";
const char LCMANGLER_ID[] = "TextEditor::LowercaseMangler";
const char TCMANGLER_ID[] = "TextEditor::TitlecaseMangler";

NameMangler::~NameMangler() = default;

class UppercaseMangler : public NameMangler {
public:
  auto id() const -> Utils::Id final { return UCMANGLER_ID; }
  auto mangle(const QString &unmangled) const -> QString final { return unmangled.toUpper(); }
};

class LowercaseMangler : public NameMangler {
public:
  auto id() const -> Utils::Id final { return LCMANGLER_ID; }
  auto mangle(const QString &unmangled) const -> QString final { return unmangled.toLower(); }
};

class TitlecaseMangler : public NameMangler {
public:
  auto id() const -> Utils::Id final { return TCMANGLER_ID; }

  auto mangle(const QString &unmangled) const -> QString final
  {
    auto result = unmangled;
    if (!result.isEmpty())
      result[0] = unmangled.at(0).toTitleCase();
    return result;
  }
};

const QChar Snippet::kVariableDelimiter(QLatin1Char('$'));
const QChar Snippet::kEscapeChar(QLatin1Char('\\'));

Snippet::Snippet(const QString &groupId, const QString &id) : m_groupId(groupId), m_id(id) {}

Snippet::~Snippet() = default;

auto Snippet::id() const -> const QString&
{
  return m_id;
}

auto Snippet::groupId() const -> const QString&
{
  return m_groupId;
}

auto Snippet::isBuiltIn() const -> bool
{
  return !m_id.isEmpty();
}

auto Snippet::setTrigger(const QString &trigger) -> void
{
  m_trigger = trigger;
}

auto Snippet::trigger() const -> const QString&
{
  return m_trigger;
}

auto Snippet::isValidTrigger(const QString &trigger) -> bool
{
  if (trigger.isEmpty() || trigger.at(0).isNumber())
    return false;
  return Utils::allOf(trigger, [](const QChar &c) { return c.isLetterOrNumber() || c == '_'; });
}

auto Snippet::setContent(const QString &content) -> void
{
  m_content = content;
}

auto Snippet::content() const -> const QString&
{
  return m_content;
}

auto Snippet::setComplement(const QString &complement) -> void
{
  m_complement = complement;
}

auto Snippet::complement() const -> const QString&
{
  return m_complement;
}

auto Snippet::setIsRemoved(bool removed) -> void
{
  m_isRemoved = removed;
}

auto Snippet::isRemoved() const -> bool
{
  return m_isRemoved;
}

auto Snippet::setIsModified(bool modified) -> void
{
  m_isModified = modified;
}

auto Snippet::isModified() const -> bool
{
  return m_isModified;
}

static auto tipPart(const ParsedSnippet::Part &part) -> QString
{
  static const char kOpenBold[] = "<b>";
  static const char kCloseBold[] = "</b>";
  static const QHash<QChar, QString> replacements = {{'\n', "<br>"}, {' ', "&nbsp;"}, {'"', "&quot;"}, {'&', "&amp;"}, {'<', "&lt;"}, {'>', "&gt;"}};

  QString text;
  text.reserve(part.text.size());

  for (const auto &c : part.text)
    text.append(replacements.value(c, c));

  if (part.variableIndex >= 0)
    text = kOpenBold + (text.isEmpty() ? QString("...") : part.text) + kCloseBold;

  return text;
}

auto Snippet::generateTip() const -> QString
{
  const auto result = parse(m_content);

  if (Utils::holds_alternative<SnippetParseError>(result))
    return Utils::get<SnippetParseError>(result).htmlMessage();
  QTC_ASSERT(Utils::holds_alternative<ParsedSnippet>(result), return {});
  const auto parsedSnippet = Utils::get<ParsedSnippet>(result);

  QString tip("<nobr>");
  for (const auto &part : parsedSnippet.parts)
    tip.append(tipPart(part));
  return tip;
}

auto Snippet::parse(const QString &snippet) -> SnippetParseResult
{
  static UppercaseMangler ucMangler;
  static LowercaseMangler lcMangler;
  static TitlecaseMangler tcMangler;

  ParsedSnippet result;

  QString errorMessage;
  const auto preprocessedSnippet = Utils::TemplateEngine::processText(Utils::globalMacroExpander(), snippet, &errorMessage);

  if (!errorMessage.isEmpty())
    return {SnippetParseError{errorMessage, {}, -1}};

  const int count = preprocessedSnippet.count();
  NameMangler *mangler = nullptr;

  QMap<QString, int> variableIndexes;
  auto inVar = false;

  ParsedSnippet::Part currentPart;

  for (auto i = 0; i < count; ++i) {
    const auto current = preprocessedSnippet.at(i);

    if (current == kVariableDelimiter) {
      if (inVar) {
        const auto variable = currentPart.text;
        const auto index = variableIndexes.value(currentPart.text, result.variables.size());
        if (index == result.variables.size()) {
          variableIndexes[variable] = index;
          result.variables.append(QList<int>());
        }
        currentPart.variableIndex = index;
        currentPart.mangler = mangler;
        mangler = nullptr;
        result.variables[index] << result.parts.size() - 1;
      } else if (currentPart.text.isEmpty()) {
        inVar = !inVar;
        continue;
      }
      result.parts << currentPart;
      currentPart = ParsedSnippet::Part();
      inVar = !inVar;
      continue;
    }

    if (mangler) {
      return SnippetParseResult{SnippetParseError{tr("Expected delimiter after mangler ID."), preprocessedSnippet, i}};
    }

    if (current == ':' && inVar) {
      const auto next = i + 1 < count ? preprocessedSnippet.at(i + 1) : QChar();
      if (next == 'l') {
        mangler = &lcMangler;
      } else if (next == 'u') {
        mangler = &ucMangler;
      } else if (next == 'c') {
        mangler = &tcMangler;
      } else {
        return SnippetParseResult{SnippetParseError{tr("Expected mangler ID \"l\" (lowercase), \"u\" (uppercase), " "or \"c\" (titlecase) after colon."), preprocessedSnippet, i}};
      }
      ++i;
      continue;
    }

    if (current == kEscapeChar) {
      const auto next = i + 1 < count ? preprocessedSnippet.at(i + 1) : QChar();
      if (next == kEscapeChar || next == kVariableDelimiter) {
        currentPart.text.append(next);
        ++i;
        continue;
      }
    }

    currentPart.text.append(current);
  }

  if (inVar) {
    return SnippetParseResult{SnippetParseError{tr("Missing closing variable delimiter for:"), currentPart.text, 0}};
  }

  if (!currentPart.text.isEmpty())
    result.parts << currentPart;

  return SnippetParseResult(result);
}

#ifdef WITH_TESTS
#include <QTest>

#include "../texteditorplugin.hpp"

const char NOMANGLER_ID[] = "TextEditor::NoMangler";

struct SnippetPart {
  SnippetPart() = default;
  explicit SnippetPart(const QString &text, int index = -1, const Utils::Id &manglerId = NOMANGLER_ID) : text(text), variableIndex(index), manglerId(manglerId) {}
  QString text;
  int variableIndex = -1; // if variable index is >= 0 the text is interpreted as a variable
  Utils::Id manglerId;
};

Q_DECLARE_METATYPE(SnippetPart);

using Parts = QList<SnippetPart>;

void Internal::TextEditorPlugin::testSnippetParsing_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<bool>("success");
  QTest::addColumn<Parts>("parts");

  QTest::newRow("no input") << QString() << true << Parts();
  QTest::newRow("empty input") << QString("") << true << Parts();
  QTest::newRow("newline only") << QString("\n") << true << Parts{SnippetPart("\n")};

  QTest::newRow("simple identifier") << QString("$tESt$") << true << Parts{SnippetPart("tESt", 0)};
  QTest::newRow("simple identifier with lc") << QString("$tESt:l$") << true << Parts{SnippetPart("tESt", 0, LCMANGLER_ID)};
  QTest::newRow("simple identifier with uc") << QString("$tESt:u$") << true << Parts{SnippetPart("tESt", 0, UCMANGLER_ID)};
  QTest::newRow("simple identifier with tc") << QString("$tESt:c$") << true << Parts{SnippetPart("tESt", 0, TCMANGLER_ID)};

  QTest::newRow("escaped string") << QString("\\\\$test\\\\$") << true << Parts{SnippetPart("$test$")};
  QTest::newRow("escaped escape") << QString("\\\\\\\\$test$\\\\\\\\") << true << Parts{SnippetPart("\\"), SnippetPart("test", 0), SnippetPart("\\"),};
  QTest::newRow("broken escape") << QString::fromLatin1("\\\\$test\\\\\\\\$\\\\") << false << Parts();

  QTest::newRow("Q_PROPERTY") << QString("Q_PROPERTY($type$ $name$ READ $name$ WRITE set$name:c$ NOTIFY $name$Changed)") << true << Parts{SnippetPart("Q_PROPERTY("), SnippetPart("type", 0), SnippetPart(" "), SnippetPart("name", 1), SnippetPart(" READ "), SnippetPart("name", 1), SnippetPart(" WRITE set"), SnippetPart("name", 1, TCMANGLER_ID), SnippetPart(" NOTIFY "), SnippetPart("name", 1), SnippetPart("Changed)")};

  QTest::newRow("open identifier") << QString("$test") << false << Parts();
  QTest::newRow("wrong mangler") << QString("$test:X$") << false << Parts();

  QTest::newRow("multiline with :") << QString("class $name$\n" "{\n" "public:\n" "    $name$() {}\n" "};") << true << Parts{SnippetPart("class "), SnippetPart("name", 0), SnippetPart("\n" "{\n" "public:\n" "    "), SnippetPart("name", 0), SnippetPart("() {}\n" "};"),};

  QTest::newRow("escape sequences") << QString("class $name$\\n" "{\\n" "public\\\\:\\n" "\\t$name$() {}\\n" "};") << true << Parts{SnippetPart("class "), SnippetPart("name", 0), SnippetPart("\n" "{\n" "public\\:\n" "\t"), SnippetPart("name", 0), SnippetPart("() {}\n" "};"),};
}

void Internal::TextEditorPlugin::testSnippetParsing()
{
  QFETCH(QString, input);
  QFETCH(bool, success);
  QFETCH(Parts, parts);

  SnippetParseResult result = Snippet::parse(input);
  QCOMPARE(Utils::holds_alternative<ParsedSnippet>(result), success);
  if (!success)
    return;

  ParsedSnippet snippet = Utils::get<ParsedSnippet>(result);

  auto rangesCompare = [&](const ParsedSnippet::Part &actual, const SnippetPart &expected) {
    QCOMPARE(actual.text, expected.text);
    QCOMPARE(actual.variableIndex, expected.variableIndex);
    auto manglerId = actual.mangler ? actual.mangler->id() : NOMANGLER_ID;
    QCOMPARE(manglerId, expected.manglerId);
  };

  for (int i = 0; i < parts.count(); ++i)
    rangesCompare(snippet.parts.at(i), parts.at(i));
}
#endif
