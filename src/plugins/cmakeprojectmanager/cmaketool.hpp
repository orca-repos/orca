// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <texteditor/codeassist/keywordscompletionassist.hpp>

#include <utils/fileutils.hpp>
#include <utils/id.hpp>
#include <utils/optional.hpp>

namespace Utils {
class QtcProcess;
}

namespace CMakeProjectManager {

namespace Internal {
class IntrospectionData;
}

class CMAKE_EXPORT CMakeTool {
public:
  enum Detection {
    ManualDetection,
    AutoDetection
  };

  enum ReaderType { FileApi };

  struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    QByteArray fullVersion;
  };

  class Generator {
  public:
    Generator(const QString &n, const QStringList &eg, bool pl = true, bool ts = true) : name(n), extraGenerators(eg), supportsPlatform(pl), supportsToolset(ts) { }

    QString name;
    QStringList extraGenerators;
    bool supportsPlatform = true;
    bool supportsToolset = true;

    auto matches(const QString &n, const QString &ex = QString()) const -> bool;
  };

  using PathMapper = std::function<Utils::FilePath (const Utils::FilePath &)>;

  explicit CMakeTool(Detection d, const Utils::Id &id);
  explicit CMakeTool(const QVariantMap &map, bool fromSdk);
  ~CMakeTool();

  static auto createId() -> Utils::Id;
  auto isValid() const -> bool;
  auto id() const -> Utils::Id { return m_id; }
  auto toMap() const -> QVariantMap;
  auto setAutorun(bool autoRun) -> void;
  auto setFilePath(const Utils::FilePath &executable) -> void;
  auto filePath() const -> Utils::FilePath;
  auto cmakeExecutable() const -> Utils::FilePath;
  auto setQchFilePath(const Utils::FilePath &path) -> void;
  auto qchFilePath() const -> Utils::FilePath;
  static auto cmakeExecutable(const Utils::FilePath &path) -> Utils::FilePath;
  auto isAutoRun() const -> bool;
  auto autoCreateBuildDirectory() const -> bool;
  auto supportedGenerators() const -> QList<Generator>;
  auto keywords() -> TextEditor::Keywords;
  auto hasFileApi() const -> bool;
  auto version() const -> Version;
  auto versionDisplay() const -> QString;
  auto isAutoDetected() const -> bool;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &displayName) -> void;
  auto setPathMapper(const PathMapper &includePathMapper) -> void;
  auto pathMapper() const -> PathMapper;
  auto readerType() const -> Utils::optional<ReaderType>;
  static auto searchQchFile(const Utils::FilePath &executable) -> Utils::FilePath;
  auto detectionSource() const -> QString { return m_detectionSource; }
  auto setDetectionSource(const QString &source) -> void { m_detectionSource = source; }
  static auto documentationUrl(const Version &version, bool online) -> QString;
  static auto openCMakeHelpUrl(const CMakeTool *tool, const QString &linkUrl) -> void;

private:
  auto readInformation() const -> void;
  auto runCMake(Utils::QtcProcess &proc, const QStringList &args, int timeoutS = 1) const -> void;
  auto parseFunctionDetailsOutput(const QString &output) -> void;
  auto parseVariableOutput(const QString &output) -> QStringList;
  auto fetchFromCapabilities() const -> void;
  auto parseFromCapabilities(const QString &input) const -> void;

  // Note: New items here need also be handled in CMakeToolItemModel::apply()
  // FIXME: Use a saner approach.
  Utils::Id m_id;
  QString m_displayName;
  Utils::FilePath m_executable;
  Utils::FilePath m_qchFilePath;
  bool m_isAutoRun = true;
  bool m_isAutoDetected = false;
  QString m_detectionSource;
  bool m_autoCreateBuildDirectory = false;
  Utils::optional<ReaderType> m_readerType;
  std::unique_ptr<Internal::IntrospectionData> m_introspection;
  PathMapper m_pathMapper;
};

} // namespace CMakeProjectManager
