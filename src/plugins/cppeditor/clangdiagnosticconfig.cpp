// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangdiagnosticconfig.hpp"
#include "cpptoolsreuse.hpp"

#include <utils/qtcassert.hpp>

#include <QSettings>

namespace CppEditor {

auto ClangDiagnosticConfig::id() const -> Utils::Id
{
  return m_id;
}

auto ClangDiagnosticConfig::setId(const Utils::Id &id) -> void
{
  m_id = id;
}

auto ClangDiagnosticConfig::displayName() const -> QString
{
  return m_displayName;
}

auto ClangDiagnosticConfig::setDisplayName(const QString &displayName) -> void
{
  m_displayName = displayName;
}

auto ClangDiagnosticConfig::clangOptions() const -> QStringList
{
  return m_clangOptions;
}

auto ClangDiagnosticConfig::setClangOptions(const QStringList &options) -> void
{
  m_clangOptions = options;
}

auto ClangDiagnosticConfig::isReadOnly() const -> bool
{
  return m_isReadOnly;
}

auto ClangDiagnosticConfig::setIsReadOnly(bool isReadOnly) -> void
{
  m_isReadOnly = isReadOnly;
}

auto ClangDiagnosticConfig::operator==(const ClangDiagnosticConfig &other) const -> bool
{
  return m_id == other.m_id && m_displayName == other.m_displayName && m_clangOptions == other.m_clangOptions && m_clangTidyMode == other.m_clangTidyMode && m_clangTidyChecks == other.m_clangTidyChecks && m_tidyChecksOptions == other.m_tidyChecksOptions && m_clazyMode == other.m_clazyMode && m_clazyChecks == other.m_clazyChecks && m_isReadOnly == other.m_isReadOnly && m_useBuildSystemWarnings == other.m_useBuildSystemWarnings;
}

auto ClangDiagnosticConfig::operator!=(const ClangDiagnosticConfig &other) const -> bool
{
  return !(*this == other);
}

auto ClangDiagnosticConfig::clazyMode() const -> ClangDiagnosticConfig::ClazyMode
{
  return m_clazyMode;
}

auto ClangDiagnosticConfig::setClazyMode(const ClazyMode &clazyMode) -> void
{
  m_clazyMode = clazyMode;
}

auto ClangDiagnosticConfig::useBuildSystemWarnings() const -> bool
{
  return m_useBuildSystemWarnings;
}

auto ClangDiagnosticConfig::setUseBuildSystemWarnings(bool useBuildSystemWarnings) -> void
{
  m_useBuildSystemWarnings = useBuildSystemWarnings;
}

auto ClangDiagnosticConfig::clangTidyMode() const -> ClangDiagnosticConfig::TidyMode
{
  return m_clangTidyMode;
}

auto ClangDiagnosticConfig::setClangTidyMode(TidyMode mode) -> void
{
  m_clangTidyMode = mode;
}

auto ClangDiagnosticConfig::clangTidyChecks() const -> QString
{
  return m_clangTidyChecks;
}

auto ClangDiagnosticConfig::clangTidyChecksAsJson() const -> QString
{
  QString jsonString = "{Checks: '" + clangTidyChecks() + ",-clang-diagnostic-*', CheckOptions: [";

  // The check is either listed verbatim or covered by the "<prefix>-*" pattern.
  const auto checkIsEnabled = [this](const QString &check) {
    for (auto subString = check; !subString.isEmpty(); subString.chop(subString.length() - subString.lastIndexOf('-'))) {
      const int idx = m_clangTidyChecks.indexOf(subString);
      if (idx == -1)
        continue;
      if (idx > 0 && m_clangTidyChecks.at(idx - 1) == '-')
        continue;
      if (subString == check || QStringView(m_clangTidyChecks).mid(idx + subString.length()).startsWith(QLatin1String("-*"))) {
        return true;
      }
    }
    return false;
  };

  QString optionString;
  for (auto it = m_tidyChecksOptions.cbegin(); it != m_tidyChecksOptions.cend(); ++it) {
    if (!checkIsEnabled(it.key()))
      continue;
    for (auto optIt = it.value().begin(); optIt != it.value().end(); ++optIt) {
      if (!optionString.isEmpty())
        optionString += ',';
      optionString += "{key: '" + it.key() + '.' + optIt.key() + "', value: '" + optIt.value() + "'}";
    }
  }
  jsonString += optionString;
  return jsonString += "]}";
}

auto ClangDiagnosticConfig::setClangTidyChecks(const QString &checks) -> void
{
  m_clangTidyChecks = checks;
}

auto ClangDiagnosticConfig::isClangTidyEnabled() const -> bool
{
  return m_clangTidyMode != TidyMode::UseCustomChecks || clangTidyChecks() != "-*";
}

auto ClangDiagnosticConfig::setTidyCheckOptions(const QString &check, const TidyCheckOptions &options) -> void
{
  m_tidyChecksOptions[check] = options;
}

auto ClangDiagnosticConfig::tidyCheckOptions(const QString &check) const -> ClangDiagnosticConfig::TidyCheckOptions
{
  return m_tidyChecksOptions.value(check);
}

auto ClangDiagnosticConfig::setTidyChecksOptionsFromSettings(const QVariant &options) -> void
{
  const auto topLevelMap = options.toMap();
  for (auto it = topLevelMap.begin(); it != topLevelMap.end(); ++it) {
    const auto optionsMap = it.value().toMap();
    TidyCheckOptions options;
    for (auto optIt = optionsMap.begin(); optIt != optionsMap.end(); ++optIt)
      options.insert(optIt.key(), optIt.value().toString());
    m_tidyChecksOptions.insert(it.key(), options);
  }
}

auto ClangDiagnosticConfig::tidyChecksOptionsForSettings() const -> QVariant
{
  QVariantMap topLevelMap;
  for (auto it = m_tidyChecksOptions.cbegin(); it != m_tidyChecksOptions.cend(); ++it) {
    QVariantMap optionsMap;
    for (auto optIt = it.value().begin(); optIt != it.value().end(); ++optIt)
      optionsMap.insert(optIt.key(), optIt.value());
    topLevelMap.insert(it.key(), optionsMap);
  }
  return topLevelMap;
}

auto ClangDiagnosticConfig::clazyChecks() const -> QString
{
  return m_clazyChecks;
}

auto ClangDiagnosticConfig::setClazyChecks(const QString &checks) -> void
{
  m_clazyChecks = checks;
}

auto ClangDiagnosticConfig::isClazyEnabled() const -> bool
{
  return m_clazyMode != ClazyMode::UseCustomChecks || !m_clazyChecks.isEmpty();
}

static auto convertToNewClazyChecksFormat(const QString &checks) -> QString
{
  // Before Qt Creator 4.9 valid values for checks were: "", "levelN".
  // Starting with Qt Creator 4.9, checks are a comma-separated string of checks: "x,y,z".

  if (checks.isEmpty())
    return {};
  if (checks.size() == 6 && checks.startsWith("level"))
    return {};
  return checks;
}

static const char diagnosticConfigsArrayKey[] = "ClangDiagnosticConfigs";
static const char diagnosticConfigIdKey[] = "id";
static const char diagnosticConfigDisplayNameKey[] = "displayName";
static const char diagnosticConfigWarningsKey[] = "diagnosticOptions";
static const char useBuildSystemFlagsKey[] = "useBuildSystemFlags";
static const char diagnosticConfigsTidyChecksKey[] = "clangTidyChecks";
static const char diagnosticConfigsTidyChecksOptionsKey[] = "clangTidyChecksOptions";
static const char diagnosticConfigsTidyModeKey[] = "clangTidyMode";
static const char diagnosticConfigsClazyModeKey[] = "clazyMode";
static const char diagnosticConfigsClazyChecksKey[] = "clazyChecks";

auto diagnosticConfigsToSettings(QSettings *s, const ClangDiagnosticConfigs &configs) -> void
{
  s->beginWriteArray(diagnosticConfigsArrayKey);
  for (int i = 0, size = configs.size(); i < size; ++i) {
    const auto &config = configs.at(i);
    s->setArrayIndex(i);
    s->setValue(diagnosticConfigIdKey, config.id().toSetting());
    s->setValue(diagnosticConfigDisplayNameKey, config.displayName());
    s->setValue(diagnosticConfigWarningsKey, config.clangOptions());
    s->setValue(useBuildSystemFlagsKey, config.useBuildSystemWarnings());
    s->setValue(diagnosticConfigsTidyModeKey, int(config.clangTidyMode()));
    s->setValue(diagnosticConfigsTidyChecksKey, config.clangTidyChecks());
    s->setValue(diagnosticConfigsTidyChecksOptionsKey, config.tidyChecksOptionsForSettings());
    s->setValue(diagnosticConfigsClazyModeKey, int(config.clazyMode()));
    s->setValue(diagnosticConfigsClazyChecksKey, config.clazyChecks());
  }
  s->endArray();
}

auto diagnosticConfigsFromSettings(QSettings *s) -> ClangDiagnosticConfigs
{
  ClangDiagnosticConfigs configs;

  const auto size = s->beginReadArray(diagnosticConfigsArrayKey);
  for (auto i = 0; i < size; ++i) {
    s->setArrayIndex(i);

    ClangDiagnosticConfig config;
    config.setId(Utils::Id::fromSetting(s->value(diagnosticConfigIdKey)));
    config.setDisplayName(s->value(diagnosticConfigDisplayNameKey).toString());
    config.setClangOptions(s->value(diagnosticConfigWarningsKey).toStringList());
    config.setUseBuildSystemWarnings(s->value(useBuildSystemFlagsKey, false).toBool());
    const auto tidyModeValue = s->value(diagnosticConfigsTidyModeKey).toInt();
    if (tidyModeValue == 0) {
      // Convert from settings of <= Qt Creator 4.10
      config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseCustomChecks);
      config.setClangTidyChecks("-*");
    } else {
      config.setClangTidyMode(static_cast<ClangDiagnosticConfig::TidyMode>(tidyModeValue));
      config.setClangTidyChecks(s->value(diagnosticConfigsTidyChecksKey).toString());
      config.setTidyChecksOptionsFromSettings(s->value(diagnosticConfigsTidyChecksOptionsKey));
    }

    config.setClazyMode(static_cast<ClangDiagnosticConfig::ClazyMode>(s->value(diagnosticConfigsClazyModeKey).toInt()));
    const auto clazyChecks = s->value(diagnosticConfigsClazyChecksKey).toString();
    config.setClazyChecks(convertToNewClazyChecksFormat(clazyChecks));
    configs.append(config);
  }
  s->endArray();

  return configs;
}

} // namespace CppEditor
