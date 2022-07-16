// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodemodelsettings.hpp"

#include "clangdiagnosticconfigsmodel.hpp"
#include "cppeditorconstants.hpp"
#include "cpptoolsreuse.hpp"

#include <core/core-interface.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/session.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/settingsutils.hpp>

#include <QDateTime>
#include <QHash>
#include <QPair>
#include <QSettings>

using namespace Utils;

namespace CppEditor {

static auto initialClangDiagnosticConfigId() -> Id { return Constants::CPP_CLANG_DIAG_CONFIG_BUILDSYSTEM; }
static auto initialPchUsage() -> CppCodeModelSettings::PCHUsage { return CppCodeModelSettings::PchUse_BuildSystem; }
static auto clangDiagnosticConfigKey() -> QString { return QStringLiteral("ClangDiagnosticConfig"); }
static auto enableLowerClazyLevelsKey() -> QString { return QLatin1String("enableLowerClazyLevels"); }
static auto pchUsageKey() -> QString { return QLatin1String(Constants::CPPEDITOR_MODEL_MANAGER_PCH_USAGE); }
static auto interpretAmbiguousHeadersAsCHeadersKey() -> QString { return QLatin1String(Constants::CPPEDITOR_INTERPRET_AMBIGIUOUS_HEADERS_AS_C_HEADERS); }
static auto skipIndexingBigFilesKey() -> QString { return QLatin1String(Constants::CPPEDITOR_SKIP_INDEXING_BIG_FILES); }
static auto indexerFileSizeLimitKey() -> QString { return QLatin1String(Constants::CPPEDITOR_INDEXER_FILE_SIZE_LIMIT); }
static auto clangdSettingsKey() -> QString { return QLatin1String("ClangdSettings"); }
static auto useClangdKey() -> QString { return QLatin1String("UseClangdV7"); }
static auto clangdPathKey() -> QString { return QLatin1String("ClangdPath"); }
static auto clangdIndexingKey() -> QString { return QLatin1String("ClangdIndexing"); }
static auto clangdHeaderInsertionKey() -> QString { return QLatin1String("ClangdHeaderInsertion"); }
static auto clangdThreadLimitKey() -> QString { return QLatin1String("ClangdThreadLimit"); }
static auto clangdDocumentThresholdKey() -> QString { return QLatin1String("ClangdDocumentThreshold"); }
static auto clangdUseGlobalSettingsKey() -> QString { return QLatin1String("useGlobalSettings"); }
static auto sessionsWithOneClangdKey() -> QString { return QLatin1String("SessionsWithOneClangd"); }

static FilePath g_defaultClangdFilePath;

static auto fallbackClangdFilePath() -> FilePath
{
  if (g_defaultClangdFilePath.exists())
    return g_defaultClangdFilePath;
  return "clangd";
}

static auto clangDiagnosticConfigIdFromSettings(QSettings *s) -> Id
{
  QTC_ASSERT(s->group() == QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP), return Id());

  return Id::fromSetting(s->value(clangDiagnosticConfigKey(), initialClangDiagnosticConfigId().toSetting()));
}

// Removed since Qt Creator 4.11
static auto removedBuiltinConfigs() -> ClangDiagnosticConfigs
{
  ClangDiagnosticConfigs configs;

  // Pedantic
  ClangDiagnosticConfig config;
  config.setId("Builtin.Pedantic");
  config.setDisplayName(QCoreApplication::translate("ClangDiagnosticConfigsModel", "Pedantic checks"));
  config.setIsReadOnly(true);
  config.setClangOptions(QStringList{QStringLiteral("-Wpedantic")});
  config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseCustomChecks);
  config.setClazyMode(ClangDiagnosticConfig::ClazyMode::UseCustomChecks);
  configs << config;

  // Everything with exceptions
  config = ClangDiagnosticConfig();
  config.setId("Builtin.EverythingWithExceptions");
  config.setDisplayName(QCoreApplication::translate("ClangDiagnosticConfigsModel", "Checks for almost everything"));
  config.setIsReadOnly(true);
  config.setClangOptions(QStringList{
    QStringLiteral("-Weverything"),
    QStringLiteral("-Wno-c++98-compat"),
    QStringLiteral("-Wno-c++98-compat-pedantic"),
    QStringLiteral("-Wno-unused-macros"),
    QStringLiteral("-Wno-newline-eof"),
    QStringLiteral("-Wno-exit-time-destructors"),
    QStringLiteral("-Wno-global-constructors"),
    QStringLiteral("-Wno-gnu-zero-variadic-macro-arguments"),
    QStringLiteral("-Wno-documentation"),
    QStringLiteral("-Wno-shadow"),
    QStringLiteral("-Wno-switch-enum"),
    QStringLiteral("-Wno-missing-prototypes"),
    // Not optimal for C projects.
    QStringLiteral("-Wno-used-but-marked-unused"),
    // e.g. QTest::qWait
  });
  config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseCustomChecks);
  config.setClazyMode(ClangDiagnosticConfig::ClazyMode::UseCustomChecks);
  configs << config;

  return configs;
}

static auto convertToCustomConfig(const Id &id) -> ClangDiagnosticConfig
{
  const auto config = findOrDefault(removedBuiltinConfigs(), [id](const ClangDiagnosticConfig &config) {
    return config.id() == id;
  });
  return ClangDiagnosticConfigsModel::createCustomConfig(config, config.displayName());
}

auto CppCodeModelSettings::fromSettings(QSettings *s) -> void
{
  s->beginGroup(QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP));

  setClangCustomDiagnosticConfigs(diagnosticConfigsFromSettings(s));
  setClangDiagnosticConfigId(clangDiagnosticConfigIdFromSettings(s));

  // Qt Creator 4.11 removes some built-in configs.
  auto write = false;
  const auto id = m_clangDiagnosticConfigId;
  if (id == "Builtin.Pedantic" || id == "Builtin.EverythingWithExceptions") {
    // If one of them was used, continue to use it, but convert it to a custom config.
    const auto customConfig = convertToCustomConfig(id);
    m_clangCustomDiagnosticConfigs.append(customConfig);
    m_clangDiagnosticConfigId = customConfig.id();
    write = true;
  }

  // Before Qt Creator 4.8, inconsistent settings might have been written.
  const auto model = diagnosticConfigsModel(m_clangCustomDiagnosticConfigs);
  if (!model.hasConfigWithId(m_clangDiagnosticConfigId))
    setClangDiagnosticConfigId(initialClangDiagnosticConfigId());

  setEnableLowerClazyLevels(s->value(enableLowerClazyLevelsKey(), true).toBool());

  const auto pchUsageVariant = s->value(pchUsageKey(), initialPchUsage());
  setPCHUsage(static_cast<PCHUsage>(pchUsageVariant.toInt()));

  const auto interpretAmbiguousHeadersAsCHeaders = s->value(interpretAmbiguousHeadersAsCHeadersKey(), false);
  setInterpretAmbigiousHeadersAsCHeaders(interpretAmbiguousHeadersAsCHeaders.toBool());

  const auto skipIndexingBigFiles = s->value(skipIndexingBigFilesKey(), true);
  setSkipIndexingBigFiles(skipIndexingBigFiles.toBool());

  const auto indexerFileSizeLimit = s->value(indexerFileSizeLimitKey(), 5);
  setIndexerFileSizeLimitInMb(indexerFileSizeLimit.toInt());

  s->endGroup();

  if (write)
    toSettings(s);

  emit changed();
}

auto CppCodeModelSettings::toSettings(QSettings *s) -> void
{
  s->beginGroup(QLatin1String(Constants::CPPEDITOR_SETTINGSGROUP));
  const auto previousConfigs = diagnosticConfigsFromSettings(s);
  const auto previousConfigId = clangDiagnosticConfigIdFromSettings(s);

  diagnosticConfigsToSettings(s, m_clangCustomDiagnosticConfigs);

  s->setValue(clangDiagnosticConfigKey(), clangDiagnosticConfigId().toSetting());
  s->setValue(enableLowerClazyLevelsKey(), enableLowerClazyLevels());
  s->setValue(pchUsageKey(), pchUsage());

  s->setValue(interpretAmbiguousHeadersAsCHeadersKey(), interpretAmbigiousHeadersAsCHeaders());
  s->setValue(skipIndexingBigFilesKey(), skipIndexingBigFiles());
  s->setValue(indexerFileSizeLimitKey(), indexerFileSizeLimitInMb());

  s->endGroup();

  auto invalidated = ClangDiagnosticConfigsModel::changedOrRemovedConfigs(previousConfigs, m_clangCustomDiagnosticConfigs);

  if (previousConfigId != clangDiagnosticConfigId() && !invalidated.contains(previousConfigId))
    invalidated.append(previousConfigId);

  if (!invalidated.isEmpty()) emit clangDiagnosticConfigsInvalidated(invalidated);
  emit changed();
}

auto CppCodeModelSettings::clangDiagnosticConfigId() const -> Id
{
  if (!diagnosticConfigsModel().hasConfigWithId(m_clangDiagnosticConfigId))
    return defaultClangDiagnosticConfigId();
  return m_clangDiagnosticConfigId;
}

auto CppCodeModelSettings::setClangDiagnosticConfigId(const Id &configId) -> void
{
  m_clangDiagnosticConfigId = configId;
}

auto CppCodeModelSettings::defaultClangDiagnosticConfigId() -> Id
{
  return initialClangDiagnosticConfigId();
}

auto CppCodeModelSettings::clangDiagnosticConfig() const -> const ClangDiagnosticConfig
{
  const auto configsModel = diagnosticConfigsModel(m_clangCustomDiagnosticConfigs);

  return configsModel.configWithId(clangDiagnosticConfigId());
}

auto CppCodeModelSettings::clangCustomDiagnosticConfigs() const -> ClangDiagnosticConfigs
{
  return m_clangCustomDiagnosticConfigs;
}

auto CppCodeModelSettings::setClangCustomDiagnosticConfigs(const ClangDiagnosticConfigs &configs) -> void
{
  m_clangCustomDiagnosticConfigs = configs;
}

auto CppCodeModelSettings::pchUsage() const -> CppCodeModelSettings::PCHUsage
{
  return m_pchUsage;
}

auto CppCodeModelSettings::setPCHUsage(CppCodeModelSettings::PCHUsage pchUsage) -> void
{
  m_pchUsage = pchUsage;
}

auto CppCodeModelSettings::interpretAmbigiousHeadersAsCHeaders() const -> bool
{
  return m_interpretAmbigiousHeadersAsCHeaders;
}

auto CppCodeModelSettings::setInterpretAmbigiousHeadersAsCHeaders(bool yesno) -> void
{
  m_interpretAmbigiousHeadersAsCHeaders = yesno;
}

auto CppCodeModelSettings::skipIndexingBigFiles() const -> bool
{
  return m_skipIndexingBigFiles;
}

auto CppCodeModelSettings::setSkipIndexingBigFiles(bool yesno) -> void
{
  m_skipIndexingBigFiles = yesno;
}

auto CppCodeModelSettings::indexerFileSizeLimitInMb() const -> int
{
  return m_indexerFileSizeLimitInMB;
}

auto CppCodeModelSettings::setIndexerFileSizeLimitInMb(int sizeInMB) -> void
{
  m_indexerFileSizeLimitInMB = sizeInMB;
}

auto CppCodeModelSettings::enableLowerClazyLevels() const -> bool
{
  return m_enableLowerClazyLevels;
}

auto CppCodeModelSettings::setEnableLowerClazyLevels(bool yesno) -> void
{
  m_enableLowerClazyLevels = yesno;
}

auto ClangdSettings::instance() -> ClangdSettings&
{
  static ClangdSettings settings;
  return settings;
}

ClangdSettings::ClangdSettings()
{
  loadSettings();
  const auto sessionMgr = ProjectExplorer::SessionManager::instance();
  connect(sessionMgr, &ProjectExplorer::SessionManager::sessionRemoved, this, [this](const QString &name) { m_data.sessionsWithOneClangd.removeOne(name); });
  connect(sessionMgr, &ProjectExplorer::SessionManager::sessionRenamed, this, [this](const QString &oldName, const QString &newName) {
    const auto index = m_data.sessionsWithOneClangd.indexOf(oldName);
    if (index != -1)
      m_data.sessionsWithOneClangd[index] = newName;
  });
}

auto ClangdSettings::useClangd() const -> bool
{
  return m_data.useClangd && clangdVersion() >= QVersionNumber(13);
}

auto ClangdSettings::setDefaultClangdPath(const FilePath &filePath) -> void
{
  g_defaultClangdFilePath = filePath;
}

auto ClangdSettings::clangdFilePath() const -> FilePath
{
  if (!m_data.executableFilePath.isEmpty())
    return m_data.executableFilePath;
  return fallbackClangdFilePath();
}

auto ClangdSettings::granularity() const -> ClangdSettings::Granularity
{
  if (m_data.sessionsWithOneClangd.contains(ProjectExplorer::SessionManager::activeSession()))
    return Granularity::Session;
  return Granularity::Project;
}

auto ClangdSettings::setData(const Data &data) -> void
{
  if (this == &instance() && data != m_data) {
    m_data = data;
    saveSettings();
    emit changed();
  }
}

static auto getClangdVersion(const FilePath &clangdFilePath) -> QVersionNumber
{
  Utils::QtcProcess clangdProc;
  clangdProc.setCommand({clangdFilePath, {"--version"}});
  clangdProc.start();
  if (!clangdProc.waitForStarted() || !clangdProc.waitForFinished())
    return {};
  const auto output = clangdProc.allOutput();
  static const QString versionPrefix = "clangd version ";
  const int prefixOffset = output.indexOf(versionPrefix);
  if (prefixOffset == -1)
    return {};
  return QVersionNumber::fromString(output.mid(prefixOffset + versionPrefix.length()));
}

auto ClangdSettings::clangdVersion(const FilePath &clangdFilePath) -> QVersionNumber
{
  static QHash<Utils::FilePath, QPair<QDateTime, QVersionNumber>> versionCache;
  const auto timeStamp = clangdFilePath.lastModified();
  const auto it = versionCache.find(clangdFilePath);
  if (it == versionCache.end()) {
    const auto version = getClangdVersion(clangdFilePath);
    versionCache.insert(clangdFilePath, qMakePair(timeStamp, version));
    return version;
  }
  if (it->first != timeStamp) {
    it->first = timeStamp;
    it->second = getClangdVersion(clangdFilePath);
  }
  return it->second;
}

auto ClangdSettings::loadSettings() -> void
{
  Utils::fromSettings(clangdSettingsKey(), {}, Orca::Plugin::Core::ICore::settings(), &m_data);
}

auto ClangdSettings::saveSettings() -> void
{
  Utils::toSettings(clangdSettingsKey(), {}, Orca::Plugin::Core::ICore::settings(), &m_data);
}

#ifdef WITH_TESTS
void ClangdSettings::setUseClangd(bool use) { instance().m_data.useClangd = use; }

void ClangdSettings::setClangdFilePath(const FilePath &filePath)
{
    instance().m_data.executableFilePath = filePath;
}
#endif

ClangdProjectSettings::ClangdProjectSettings(ProjectExplorer::Project *project) : m_project(project)
{
  loadSettings();
}

auto ClangdProjectSettings::settings() const -> ClangdSettings::Data
{
  if (m_useGlobalSettings)
    return ClangdSettings::instance().data();
  auto data = m_customSettings;

  // This property is global by definition.
  data.sessionsWithOneClangd = ClangdSettings::instance().data().sessionsWithOneClangd;

  return data;
}

auto ClangdProjectSettings::setSettings(const ClangdSettings::Data &data) -> void
{
  m_customSettings = data;
  saveSettings();
  emit ClangdSettings::instance().changed();
}

auto ClangdProjectSettings::setUseGlobalSettings(bool useGlobal) -> void
{
  m_useGlobalSettings = useGlobal;
  saveSettings();
  emit ClangdSettings::instance().changed();
}

auto ClangdProjectSettings::loadSettings() -> void
{
  if (!m_project)
    return;
  const auto data = m_project->namedSettings(clangdSettingsKey()).toMap();
  m_useGlobalSettings = data.value(clangdUseGlobalSettingsKey(), true).toBool();
  if (!m_useGlobalSettings)
    m_customSettings.fromMap(data);
}

auto ClangdProjectSettings::saveSettings() -> void
{
  if (!m_project)
    return;
  QVariantMap data;
  if (!m_useGlobalSettings)
    data = m_customSettings.toMap();
  data.insert(clangdUseGlobalSettingsKey(), m_useGlobalSettings);
  m_project->setNamedSettings(clangdSettingsKey(), data);
}

auto ClangdSettings::Data::toMap() const -> QVariantMap
{
  QVariantMap map;
  map.insert(useClangdKey(), useClangd);
  if (executableFilePath != fallbackClangdFilePath())
    map.insert(clangdPathKey(), executableFilePath.toString());
  map.insert(clangdIndexingKey(), enableIndexing);
  map.insert(clangdHeaderInsertionKey(), autoIncludeHeaders);
  map.insert(clangdThreadLimitKey(), workerThreadLimit);
  map.insert(clangdDocumentThresholdKey(), documentUpdateThreshold);
  map.insert(sessionsWithOneClangdKey(), sessionsWithOneClangd);
  return map;
}

auto ClangdSettings::Data::fromMap(const QVariantMap &map) -> void
{
  useClangd = map.value(useClangdKey(), true).toBool();
  executableFilePath = FilePath::fromString(map.value(clangdPathKey()).toString());
  enableIndexing = map.value(clangdIndexingKey(), true).toBool();
  autoIncludeHeaders = map.value(clangdHeaderInsertionKey(), false).toBool();
  workerThreadLimit = map.value(clangdThreadLimitKey(), 0).toInt();
  documentUpdateThreshold = map.value(clangdDocumentThresholdKey(), 500).toInt();
  sessionsWithOneClangd = map.value(sessionsWithOneClangdKey()).toStringList();
}

} // namespace CppEditor
