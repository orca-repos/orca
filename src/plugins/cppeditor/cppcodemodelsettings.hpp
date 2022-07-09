// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "clangdiagnosticconfigsmodel.hpp"

#include <utils/fileutils.hpp>

#include <QObject>
#include <QStringList>
#include <QVersionNumber>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Project;
}

namespace CppEditor {

class CPPEDITOR_EXPORT CppCodeModelSettings : public QObject {
  Q_OBJECT

public:
  enum PCHUsage {
    PchUse_None = 1,
    PchUse_BuildSystem = 2
  };

  auto fromSettings(QSettings *s) -> void;
  auto toSettings(QSettings *s) -> void;
  auto clangDiagnosticConfigId() const -> Utils::Id;
  auto setClangDiagnosticConfigId(const Utils::Id &configId) -> void;
  static auto defaultClangDiagnosticConfigId() -> Utils::Id;
  auto clangDiagnosticConfig() const -> const ClangDiagnosticConfig;
  auto clangCustomDiagnosticConfigs() const -> ClangDiagnosticConfigs;
  auto setClangCustomDiagnosticConfigs(const ClangDiagnosticConfigs &configs) -> void;
  auto enableLowerClazyLevels() const -> bool;
  auto setEnableLowerClazyLevels(bool yesno) -> void;
  auto pchUsage() const -> PCHUsage;
  auto setPCHUsage(PCHUsage pchUsage) -> void;
  auto interpretAmbigiousHeadersAsCHeaders() const -> bool;
  auto setInterpretAmbigiousHeadersAsCHeaders(bool yesno) -> void;
  auto skipIndexingBigFiles() const -> bool;
  auto setSkipIndexingBigFiles(bool yesno) -> void;
  auto indexerFileSizeLimitInMb() const -> int;
  auto setIndexerFileSizeLimitInMb(int sizeInMB) -> void;
  auto setCategorizeFindReferences(bool categorize) -> void { m_categorizeFindReferences = categorize; }
  auto categorizeFindReferences() const -> bool { return m_categorizeFindReferences; }

signals:
  auto clangDiagnosticConfigsInvalidated(const QVector<Utils::Id> &configId) -> void;
  auto changed() -> void;

private:
  PCHUsage m_pchUsage = PchUse_BuildSystem;
  bool m_interpretAmbigiousHeadersAsCHeaders = false;
  bool m_skipIndexingBigFiles = true;
  int m_indexerFileSizeLimitInMB = 5;
  ClangDiagnosticConfigs m_clangCustomDiagnosticConfigs;
  Utils::Id m_clangDiagnosticConfigId;
  bool m_enableLowerClazyLevels = true;    // For UI behavior only
  bool m_categorizeFindReferences = false; // Ephemeral!
};

class CPPEDITOR_EXPORT ClangdSettings : public QObject {
  Q_OBJECT public:
  class CPPEDITOR_EXPORT Data {
  public:
    auto toMap() const -> QVariantMap;
    auto fromMap(const QVariantMap &map) -> void;

    friend auto operator==(const Data &s1, const Data &s2) -> bool
    {
      return s1.useClangd == s2.useClangd && s1.executableFilePath == s2.executableFilePath && s1.sessionsWithOneClangd == s2.sessionsWithOneClangd && s1.workerThreadLimit == s2.workerThreadLimit && s1.enableIndexing == s2.enableIndexing && s1.autoIncludeHeaders == s2.autoIncludeHeaders && s1.documentUpdateThreshold == s2.documentUpdateThreshold;
    }

    friend auto operator!=(const Data &s1, const Data &s2) -> bool { return !(s1 == s2); }

    Utils::FilePath executableFilePath;
    QStringList sessionsWithOneClangd;
    int workerThreadLimit = 0;
    bool useClangd = true;
    bool enableIndexing = true;
    bool autoIncludeHeaders = false;
    int documentUpdateThreshold = 500;
  };

  ClangdSettings(const Data &data) : m_data(data) {}

  static auto instance() -> ClangdSettings&;
  auto useClangd() const -> bool;
  static auto setDefaultClangdPath(const Utils::FilePath &filePath) -> void;
  auto clangdFilePath() const -> Utils::FilePath;
  auto indexingEnabled() const -> bool { return m_data.enableIndexing; }
  auto autoIncludeHeaders() const -> bool { return m_data.autoIncludeHeaders; }
  auto workerThreadLimit() const -> int { return m_data.workerThreadLimit; }
  auto documentUpdateThreshold() const -> int { return m_data.documentUpdateThreshold; }

  enum class Granularity {
    Project,
    Session
  };

  auto granularity() const -> Granularity;
  auto setData(const Data &data) -> void;
  auto data() const -> Data { return m_data; }
  static auto clangdVersion(const Utils::FilePath &clangdFilePath) -> QVersionNumber;
  auto clangdVersion() const -> QVersionNumber { return clangdVersion(clangdFilePath()); }

  #ifdef WITH_TESTS
    static void setUseClangd(bool use);
    static void setClangdFilePath(const Utils::FilePath &filePath);
  #endif

signals:
  auto changed() -> void;

private:
  ClangdSettings();

  auto loadSettings() -> void;
  auto saveSettings() -> void;

  Data m_data;
};

class CPPEDITOR_EXPORT ClangdProjectSettings {
public:
  ClangdProjectSettings(ProjectExplorer::Project *project);

  auto settings() const -> ClangdSettings::Data;
  auto setSettings(const ClangdSettings::Data &data) -> void;
  auto useGlobalSettings() const -> bool { return m_useGlobalSettings; }
  auto setUseGlobalSettings(bool useGlobal) -> void;

private:
  auto loadSettings() -> void;
  auto saveSettings() -> void;

  ProjectExplorer::Project *const m_project;
  ClangdSettings::Data m_customSettings;
  bool m_useGlobalSettings = true;
};

} // namespace CppEditor
