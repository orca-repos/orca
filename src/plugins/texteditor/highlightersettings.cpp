// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "highlightersettings.hpp"

#include "texteditorconstants.hpp"

#include <core/core-interface.hpp>

#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/stringutils.hpp>

#include <QSettings>

using namespace Utils;

namespace TextEditor {
namespace Internal {

auto findFallbackDefinitionsLocation() -> FilePath
{
  if (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost()) {
    static const QLatin1String kateSyntaxPaths[] = {QLatin1String("/share/apps/katepart/syntax"), QLatin1String("/share/kde4/apps/katepart/syntax")};

    // Some wild guesses.
    for (const auto &kateSyntaxPath : kateSyntaxPaths) {
      const FilePath paths[] = {FilePath("/usr") / kateSyntaxPath, FilePath("/usr/local") / kateSyntaxPath, FilePath("/opt") / kateSyntaxPath};
      for (const auto &path : paths) {
        if (path.exists() && !path.dirEntries({{"*.xml"}}).isEmpty())
          return path;
      }
    }

    // Try kde-config.
    const FilePath programs[] = {"kde-config", "kde4-config"};
    for (const auto &program : programs) {
      QtcProcess process;
      process.setTimeoutS(5);
      process.setCommand({program, {"--prefix"}});
      process.runBlocking();
      if (process.result() == QtcProcess::FinishedWithSuccess) {
        auto output = process.stdOut();
        output.remove('\n');
        const auto dir = FilePath::fromString(output);
        for (auto &kateSyntaxPath : kateSyntaxPaths) {
          const auto path = dir / kateSyntaxPath;
          if (path.exists() && !path.dirEntries({{"*.xml"}}).isEmpty())
            return path;
        }
      }
    }
  }

  const FilePath dir = Orca::Plugin::Core::ICore::resourcePath("generic-highlighter");
  if (dir.exists() && !dir.dirEntries({{"*.xml"}}).isEmpty())
    return dir;

  return {};
}

} // namespace Internal

const QLatin1String kDefinitionFilesPath("UserDefinitionFilesPath");
const QLatin1String kIgnoredFilesPatterns("IgnoredFilesPatterns");

static auto groupSpecifier(const QString &postFix, const QString &category) -> QString
{
  if (category.isEmpty())
    return postFix;
  return QString(category + postFix);
}

auto HighlighterSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  const auto &group = groupSpecifier(Constants::HIGHLIGHTER_SETTINGS_CATEGORY, category);
  s->beginGroup(group);
  s->setValue(kDefinitionFilesPath, m_definitionFilesPath.toVariant());
  s->setValue(kIgnoredFilesPatterns, ignoredFilesPatterns());
  s->endGroup();
}

auto HighlighterSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  const auto &group = groupSpecifier(Constants::HIGHLIGHTER_SETTINGS_CATEGORY, category);
  s->beginGroup(group);
  m_definitionFilesPath = FilePath::fromVariant(s->value(kDefinitionFilesPath));
  if (!s->contains(kDefinitionFilesPath))
    assignDefaultDefinitionsPath();
  else
    m_definitionFilesPath = FilePath::fromVariant(s->value(kDefinitionFilesPath));
  if (!s->contains(kIgnoredFilesPatterns))
    assignDefaultIgnoredPatterns();
  else
    setIgnoredFilesPatterns(s->value(kIgnoredFilesPatterns, QString()).toString());
  s->endGroup();
}

auto HighlighterSettings::setIgnoredFilesPatterns(const QString &patterns) -> void
{
  setExpressionsFromList(patterns.split(',', Qt::SkipEmptyParts));
}

auto HighlighterSettings::ignoredFilesPatterns() const -> QString
{
  return listFromExpressions().join(',');
}

auto HighlighterSettings::assignDefaultIgnoredPatterns() -> void
{
  setExpressionsFromList({"*.txt", "LICENSE*", "README", "INSTALL", "COPYING", "NEWS", "qmldir"});
}

auto HighlighterSettings::assignDefaultDefinitionsPath() -> void
{
  const FilePath path = Orca::Plugin::Core::ICore::userResourcePath("generic-highlighter");
  if (path.exists() || path.ensureWritableDir())
    m_definitionFilesPath = path;
}

auto HighlighterSettings::isIgnoredFilePattern(const QString &fileName) const -> bool
{
  for (const auto &regExp : m_ignoredFiles)
    if (fileName.indexOf(regExp) != -1)
      return true;

  return false;
}

auto HighlighterSettings::equals(const HighlighterSettings &highlighterSettings) const -> bool
{
  return m_definitionFilesPath == highlighterSettings.m_definitionFilesPath && m_ignoredFiles == highlighterSettings.m_ignoredFiles;
}

auto HighlighterSettings::setExpressionsFromList(const QStringList &patterns) -> void
{
  m_ignoredFiles.clear();
  QRegularExpression regExp;
  regExp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
  for (const auto &pattern : patterns) {
    regExp.setPattern(QRegularExpression::wildcardToRegularExpression(pattern));
    m_ignoredFiles.append(regExp);
  }
}

auto HighlighterSettings::listFromExpressions() const -> QStringList
{
  QStringList patterns;
  for (const auto &regExp : m_ignoredFiles)
    patterns.append(regExp.pattern());
  return patterns;
}

} // TextEditor
