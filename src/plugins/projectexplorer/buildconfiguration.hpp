// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "projectconfiguration.hpp"
#include "task.hpp"

#include <utils/environment.hpp>
#include <utils/fileutils.hpp>

namespace Utils {
class MacroExpander;
}

namespace ProjectExplorer {

namespace Internal {
class BuildConfigurationPrivate;
}

class BuildDirectoryAspect;
class BuildInfo;
class BuildSystem;
class BuildStepList;
class Kit;
class NamedWidget;
class Node;
class RunConfiguration;
class Target;

class PROJECTEXPLORER_EXPORT BuildConfiguration : public ProjectConfiguration {
  Q_OBJECT

protected:
  friend class BuildConfigurationFactory;
  explicit BuildConfiguration(Target *target, Utils::Id id);

public:
  ~BuildConfiguration() override;

  auto buildDirectory() const -> Utils::FilePath;
  auto rawBuildDirectory() const -> Utils::FilePath;
  auto setBuildDirectory(const Utils::FilePath &dir) -> void;

  virtual auto buildSystem() const -> BuildSystem*;
  virtual auto createConfigWidget() -> NamedWidget*;
  virtual auto createSubConfigWidgets() -> QList<NamedWidget*>;

  // Maybe the BuildConfiguration is not the best place for the environment
  auto baseEnvironment() const -> Utils::Environment;
  auto baseEnvironmentText() const -> QString;
  auto environment() const -> Utils::Environment;
  auto setUserEnvironmentChanges(const Utils::EnvironmentItems &diff) -> void;
  auto userEnvironmentChanges() const -> Utils::EnvironmentItems;
  auto useSystemEnvironment() const -> bool;
  auto setUseSystemEnvironment(bool b) -> void;

  virtual auto addToEnvironment(Utils::Environment &env) const -> void;

  auto parseStdOut() const -> bool;
  auto setParseStdOut(bool b) -> void;
  auto customParsers() const -> const QList<Utils::Id>;
  auto setCustomParsers(const QList<Utils::Id> &parsers) -> void;
  auto buildSteps() const -> BuildStepList*;
  auto cleanSteps() const -> BuildStepList*;
  auto appendInitialBuildStep(Utils::Id id) -> void;
  auto appendInitialCleanStep(Utils::Id id) -> void;
  auto fromMap(const QVariantMap &map) -> bool override;
  auto toMap() const -> QVariantMap override;
  auto isEnabled() const -> bool;
  auto disabledReason() const -> QString;

  virtual auto regenerateBuildFiles(Node *node) -> bool;
  virtual auto restrictNextBuild(const RunConfiguration *rc) -> void;

  enum BuildType {
    Unknown,
    Debug,
    Profile,
    Release
  };

  virtual auto buildType() const -> BuildType;
  static auto buildTypeName(BuildType type) -> QString;

  enum SpaceHandling {
    KeepSpace,
    ReplaceSpaces
  };

  static auto buildDirectoryFromTemplate(const Utils::FilePath &projectDir, const Utils::FilePath &mainFilePath, const QString &projectName, const Kit *kit, const QString &bcName, BuildType buildType, SpaceHandling spaceHandling = ReplaceSpaces) -> Utils::FilePath;

  auto isActive() const -> bool;
  auto updateCacheAndEmitEnvironmentChanged() -> void;
  auto buildDirectoryAspect() const -> BuildDirectoryAspect*;
  auto setConfigWidgetDisplayName(const QString &display) -> void;
  auto setBuildDirectoryHistoryCompleter(const QString &history) -> void;
  auto setConfigWidgetHasFrame(bool configWidgetHasFrame) -> void;
  auto setBuildDirectorySettingsKey(const QString &key) -> void;
  auto addConfigWidgets(const std::function<void (NamedWidget *)> &adder) -> void;
  auto doInitialize(const BuildInfo &info) -> void;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto createBuildDirectory() -> bool;

signals:
  auto environmentChanged() -> void;
  auto buildDirectoryChanged() -> void;
  auto enabledChanged() -> void;
  auto buildTypeChanged() -> void;

protected:
  auto setInitializer(const std::function<void(const BuildInfo &info)> &initializer) -> void;

private:
  auto emitBuildDirectoryChanged() -> void;
  Internal::BuildConfigurationPrivate *d = nullptr;
};

class PROJECTEXPLORER_EXPORT BuildConfigurationFactory {
protected:
  BuildConfigurationFactory();
  BuildConfigurationFactory(const BuildConfigurationFactory &) = delete;

  auto operator=(const BuildConfigurationFactory &) -> BuildConfigurationFactory& = delete;

  virtual ~BuildConfigurationFactory(); // Needed for dynamic_casts in importers.

public:
  using IssueReporter = std::function<Tasks(Kit *, const QString &, const QString &)>;

  // List of build information that can be used to create a new build configuration via
  // "Add Build Configuration" button.
  auto allAvailableBuilds(const Target *parent) const -> const QList<BuildInfo>;
  // List of build information that can be used to initially set up a new build configuration.
  auto allAvailableSetups(const Kit *k, const Utils::FilePath &projectPath) const -> const QList<BuildInfo>;
  auto create(Target *parent, const BuildInfo &info) const -> BuildConfiguration*;
  static auto restore(Target *parent, const QVariantMap &map) -> BuildConfiguration*;
  static auto clone(Target *parent, const BuildConfiguration *source) -> BuildConfiguration*;
  static auto find(const Kit *k, const Utils::FilePath &projectPath) -> BuildConfigurationFactory*;
  static auto find(Target *parent) -> BuildConfigurationFactory*;
  auto setIssueReporter(const IssueReporter &issueReporter) -> void;
  auto reportIssues(Kit *kit, const QString &projectPath, const QString &buildDir) const -> const Tasks;

protected:
  using BuildGenerator = std::function<QList<BuildInfo>(const Kit *, const Utils::FilePath &, bool)>;
  auto setBuildGenerator(const BuildGenerator &buildGenerator) -> void;
  auto supportsTargetDeviceType(Utils::Id id) const -> bool;
  auto setSupportedProjectType(Utils::Id id) -> void;
  auto setSupportedProjectMimeTypeName(const QString &mimeTypeName) -> void;
  auto addSupportedTargetDeviceType(Utils::Id id) -> void;
  auto setDefaultDisplayName(const QString &defaultDisplayName) -> void;

  using BuildConfigurationCreator = std::function<BuildConfiguration *(Target *)>;

  template <class BuildConfig>
  auto registerBuildConfiguration(Utils::Id buildConfigId) -> void
  {
    m_creator = [buildConfigId](Target *t) { return new BuildConfig(t, buildConfigId); };
    m_buildConfigId = buildConfigId;
  }

private:
  auto canHandle(const Target *t) const -> bool;

  BuildConfigurationCreator m_creator;
  Utils::Id m_buildConfigId;
  Utils::Id m_supportedProjectType;
  QList<Utils::Id> m_supportedTargetDeviceTypes;
  QString m_supportedProjectMimeTypeName;
  IssueReporter m_issueReporter;
  BuildGenerator m_buildGenerator;
};

} // namespace ProjectExplorer
