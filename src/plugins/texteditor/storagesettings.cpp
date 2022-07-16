// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "storagesettings.hpp"

#include <utils/settingsutils.hpp>

#include <QRegularExpression>
#include <QSettings>
#include <QString>

namespace TextEditor {

static constexpr char cleanWhitespaceKey[] = "cleanWhitespace";
static constexpr char inEntireDocumentKey[] = "inEntireDocument";
static constexpr char addFinalNewLineKey[] = "addFinalNewLine";
static constexpr char cleanIndentationKey[] = "cleanIndentation";
static constexpr char skipTrailingWhitespaceKey[] = "skipTrailingWhitespace";
static constexpr char ignoreFileTypesKey[] = "ignoreFileTypes";
static constexpr char groupPostfix[] = "StorageSettings";
static constexpr char defaultTrailingWhitespaceBlacklist[] = "*.md, *.MD, Makefile";

StorageSettings::StorageSettings() : m_ignoreFileTypes(defaultTrailingWhitespaceBlacklist), m_cleanWhitespace(true), m_inEntireDocument(false), m_addFinalNewLine(true), m_cleanIndentation(true), m_skipTrailingWhitespace(true) {}

auto StorageSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  Utils::toSettings(QLatin1String(groupPostfix), category, s, this);
}

auto StorageSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  *this = StorageSettings();
  Utils::fromSettings(QLatin1String(groupPostfix), category, s, this);
}

auto StorageSettings::toMap() const -> QVariantMap
{
  return {{cleanWhitespaceKey, m_cleanWhitespace}, {inEntireDocumentKey, m_inEntireDocument}, {addFinalNewLineKey, m_addFinalNewLine}, {cleanIndentationKey, m_cleanIndentation}, {skipTrailingWhitespaceKey, m_skipTrailingWhitespace}, {ignoreFileTypesKey, m_ignoreFileTypes}};
}

auto StorageSettings::fromMap(const QVariantMap &map) -> void
{
  m_cleanWhitespace = map.value(cleanWhitespaceKey, m_cleanWhitespace).toBool();
  m_inEntireDocument = map.value(inEntireDocumentKey, m_inEntireDocument).toBool();
  m_addFinalNewLine = map.value(addFinalNewLineKey, m_addFinalNewLine).toBool();
  m_cleanIndentation = map.value(cleanIndentationKey, m_cleanIndentation).toBool();
  m_skipTrailingWhitespace = map.value(skipTrailingWhitespaceKey, m_skipTrailingWhitespace).toBool();
  m_ignoreFileTypes = map.value(ignoreFileTypesKey, m_ignoreFileTypes).toString();
}

auto StorageSettings::removeTrailingWhitespace(const QString &fileName) const -> bool
{
  // if the user has elected not to trim trailing whitespace altogether, then
  // early out here
  if (!m_skipTrailingWhitespace) {
    return true;
  }

  const QString ignoreFileTypesRegExp(R"(\s*((?>\*\.)?[\w\d\.\*]+)[,;]?\s*)");

  // use the ignore-files regex to extract the specified file patterns
  const QRegularExpression re(ignoreFileTypesRegExp);
  auto iter = re.globalMatch(m_ignoreFileTypes);

  while (iter.hasNext()) {
    auto match = iter.next();
    auto pattern = match.captured(1);

    QRegularExpression patternRegExp(QRegularExpression::wildcardToRegularExpression(pattern));
    auto patternMatch = patternRegExp.match(fileName);
    if (patternMatch.hasMatch()) {
      // if the filename has a pattern we want to ignore, then we need to return
      // false ("don't remove trailing whitespace")
      return false;
    }
  }

  // the supplied pattern does not match, so we want to remove trailing whitespace
  return true;
}

auto StorageSettings::equals(const StorageSettings &ts) const -> bool
{
  return m_addFinalNewLine == ts.m_addFinalNewLine && m_cleanWhitespace == ts.m_cleanWhitespace && m_inEntireDocument == ts.m_inEntireDocument && m_cleanIndentation == ts.m_cleanIndentation && m_skipTrailingWhitespace == ts.m_skipTrailingWhitespace && m_ignoreFileTypes == ts.m_ignoreFileTypes;
}

} // namespace TextEditor
