// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"
#include "cmakeconfigitem.hpp"
#include "configmodel.hpp"

#include <projectexplorer/buildaspects.hpp>
#include <projectexplorer/buildconfiguration.hpp>

namespace CMakeProjectManager {
class CMakeProject;

namespace Internal {

class CMakeBuildSystem;
class CMakeBuildSettingsWidget;
class CMakeProjectImporter;

} // namespace Internal

class CMAKE_EXPORT CMakeBuildConfiguration : public ProjectExplorer::BuildConfiguration {
  Q_OBJECT

public:
  CMakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id);
  ~CMakeBuildConfiguration() override;

  auto configurationFromCMake() const -> CMakeConfig;
  auto configurationChanges() const -> CMakeConfig;
  auto configurationChangesArguments(bool initialParameters = false) const -> QStringList;
  auto initialCMakeArguments() const -> QStringList;
  auto initialCMakeConfiguration() const -> CMakeConfig;
  auto error() const -> QString;
  auto warning() const -> QString;

  static auto shadowBuildDirectory(const Utils::FilePath &projectFilePath, const ProjectExplorer::Kit *k, const QString &bcName, BuildConfiguration::BuildType buildType) -> Utils::FilePath;

  // Context menu action:
  auto buildTarget(const QString &buildTarget) -> void;
  auto buildSystem() const -> ProjectExplorer::BuildSystem* final;
  auto setSourceDirectory(const Utils::FilePath &path) -> void;
  auto sourceDirectory() const -> Utils::FilePath;
  auto cmakeBuildType() const -> QString;
  auto setCMakeBuildType(const QString &cmakeBuildType, bool quiet = false) -> void;
  auto isMultiConfig() const -> bool;
  auto setIsMultiConfig(bool isMultiConfig) -> void;
  auto additionalCMakeArguments() const -> QStringList;
  auto setAdditionalCMakeArguments(const QStringList &args) -> void;
  auto filterConfigArgumentsFromAdditionalCMakeArguments() -> void;

signals:
  auto errorOccurred(const QString &message) -> void;
  auto warningOccurred(const QString &message) -> void;
  auto signingFlagsChanged() -> void;
  auto configurationChanged(const CMakeConfig &config) -> void;

protected:
  auto fromMap(const QVariantMap &map) -> bool override;

private:
  auto toMap() const -> QVariantMap override;
  auto buildType() const -> BuildType override;
  auto createConfigWidget() -> ProjectExplorer::NamedWidget* override;
  virtual auto signingFlags() const -> CMakeConfig;

  enum ForceEnabledChanged {
    False,
    True
  };

  auto clearError(ForceEnabledChanged fec = ForceEnabledChanged::False) -> void;
  auto setConfigurationFromCMake(const CMakeConfig &config) -> void;
  auto setConfigurationChanges(const CMakeConfig &config) -> void;
  auto setInitialCMakeArguments(const QStringList &args) -> void;
  auto setError(const QString &message) -> void;
  auto setWarning(const QString &message) -> void;

  QString m_error;
  QString m_warning;
  CMakeConfig m_configurationFromCMake;
  CMakeConfig m_configurationChanges;
  Internal::CMakeBuildSystem *m_buildSystem = nullptr;
  bool m_isMultiConfig = false;

  friend class Internal::CMakeBuildSettingsWidget;
  friend class Internal::CMakeBuildSystem;
};

class CMAKE_EXPORT CMakeBuildConfigurationFactory : public ProjectExplorer::BuildConfigurationFactory {
public:
  CMakeBuildConfigurationFactory();

  enum BuildType {
    BuildTypeNone = 0,
    BuildTypeDebug = 1,
    BuildTypeRelease = 2,
    BuildTypeRelWithDebInfo = 3,
    BuildTypeMinSizeRel = 4,
    BuildTypeLast = 5
  };

  static auto buildTypeFromByteArray(const QByteArray &in) -> BuildType;
  static auto cmakeBuildTypeToBuildType(const BuildType &in) -> ProjectExplorer::BuildConfiguration::BuildType;

private:
  static auto createBuildInfo(BuildType buildType) -> ProjectExplorer::BuildInfo;

  friend class Internal::CMakeProjectImporter;
};

namespace Internal {

class InitialCMakeArgumentsAspect final : public Utils::StringAspect {
  Q_OBJECT
  CMakeConfig m_cmakeConfiguration;

public:
  InitialCMakeArgumentsAspect();

  auto cmakeConfiguration() const -> const CMakeConfig&;
  auto allValues() const -> const QStringList;
  auto setAllValues(const QString &values, QStringList &additionalArguments) -> void;
  auto setCMakeConfiguration(const CMakeConfig &config) -> void;
  auto fromMap(const QVariantMap &map) -> void final;
  auto toMap(QVariantMap &map) const -> void final;
};

class AdditionalCMakeOptionsAspect final : public Utils::StringAspect {
  Q_OBJECT

public:
  AdditionalCMakeOptionsAspect();
};

class SourceDirectoryAspect final : public Utils::StringAspect {
  Q_OBJECT

public:
  SourceDirectoryAspect();
};

class BuildTypeAspect final : public Utils::StringAspect {
  Q_OBJECT

public:
  BuildTypeAspect();
  using Utils::StringAspect::update;
};

} // namespace Internal
} // namespace CMakeProjectManager
