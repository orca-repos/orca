// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.hpp>

#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class HighlighterSettings {
public:
  HighlighterSettings() = default;

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto setDefinitionFilesPath(const Utils::FilePath &path) -> void { m_definitionFilesPath = path; }
  auto definitionFilesPath() const -> const Utils::FilePath& { return m_definitionFilesPath; }
  auto setIgnoredFilesPatterns(const QString &patterns) -> void;
  auto ignoredFilesPatterns() const -> QString;
  auto isIgnoredFilePattern(const QString &fileName) const -> bool;
  auto equals(const HighlighterSettings &highlighterSettings) const -> bool;

  friend auto operator==(const HighlighterSettings &a, const HighlighterSettings &b) -> bool { return a.equals(b); }
  friend auto operator!=(const HighlighterSettings &a, const HighlighterSettings &b) -> bool { return !a.equals(b); }

private:
  auto assignDefaultIgnoredPatterns() -> void;
  auto assignDefaultDefinitionsPath() -> void;
  auto setExpressionsFromList(const QStringList &patterns) -> void;
  auto listFromExpressions() const -> QStringList;

  Utils::FilePath m_definitionFilesPath;
  QList<QRegularExpression> m_ignoredFiles;
};

namespace Internal {
auto findFallbackDefinitionsLocation() -> Utils::FilePath;
}

} // namespace TextEditor
