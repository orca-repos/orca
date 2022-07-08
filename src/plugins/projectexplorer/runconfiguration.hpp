// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildtargetinfo.hpp"
#include "devicesupport/idevice.hpp"
#include "projectconfiguration.hpp"
#include "projectexplorerconstants.hpp"
#include "task.hpp"

#include <utils/aspects.hpp>
#include <utils/environment.hpp>
#include <utils/macroexpander.hpp>
#include <utils/port.hpp>

#include <QWidget>

#include <functional>
#include <memory>

namespace Utils {
class OutputFormatter;
}

namespace ProjectExplorer {

class BuildConfiguration;
class BuildSystem;
class GlobalOrProjectAspect;
class ProjectNode;
class Runnable;
class RunConfigurationFactory;
class RunConfiguration;
class RunConfigurationCreationInfo;
class Target;

/**
 * An interface for a hunk of global or per-project
 * configuration data.
 *
 */

class PROJECTEXPLORER_EXPORT ISettingsAspect : public Utils::AspectContainer {
  Q_OBJECT

public:
  ISettingsAspect();

  /// Create a configuration widget for this settings aspect.
  auto createConfigWidget() const -> QWidget*;

protected:
  using ConfigWidgetCreator = std::function<QWidget *()>;
  auto setConfigWidgetCreator(const ConfigWidgetCreator &configWidgetCreator) -> void;

  friend class GlobalOrProjectAspect;

  ConfigWidgetCreator m_configWidgetCreator;
};

/**
 * An interface to facilitate switching between hunks of
 * global and per-project configuration data.
 *
 */

class PROJECTEXPLORER_EXPORT GlobalOrProjectAspect : public Utils::BaseAspect {
  Q_OBJECT

public:
  GlobalOrProjectAspect();
  ~GlobalOrProjectAspect() override;

  auto setProjectSettings(ISettingsAspect *settings) -> void;
  auto setGlobalSettings(ISettingsAspect *settings) -> void;
  auto isUsingGlobalSettings() const -> bool { return m_useGlobalSettings; }
  auto setUsingGlobalSettings(bool value) -> void;
  auto resetProjectToGlobalSettings() -> void;
  auto projectSettings() const -> ISettingsAspect* { return m_projectSettings; }
  auto globalSettings() const -> ISettingsAspect* { return m_globalSettings; }
  auto currentSettings() const -> ISettingsAspect*;

protected:
  friend class RunConfiguration;
  auto fromMap(const QVariantMap &map) -> void override;
  auto toMap(QVariantMap &data) const -> void override;
  auto toActiveMap(QVariantMap &data) const -> void override;

private:
  bool m_useGlobalSettings = false;
  ISettingsAspect *m_projectSettings = nullptr; // Owned if present.
  ISettingsAspect *m_globalSettings = nullptr;  // Not owned.
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT RunConfiguration : public ProjectConfiguration {
  Q_OBJECT

public:
  ~RunConfiguration() override;

  virtual auto disabledReason() const -> QString;
  virtual auto isEnabled() const -> bool;
  auto createConfigurationWidget() -> QWidget*;
  auto isConfigured() const -> bool;
  virtual auto checkForIssues() const -> Tasks { return {}; }
  using CommandLineGetter = std::function<Utils::CommandLine()>;
  auto setCommandLineGetter(const CommandLineGetter &cmdGetter) -> void;
  auto commandLine() const -> Utils::CommandLine;
  using RunnableModifier = std::function<void(Runnable &)>;
  auto setRunnableModifier(const RunnableModifier &extraModifier) -> void;
  virtual auto runnable() const -> Runnable;

  // Return a handle to the build system target that created this run configuration.
  // May return an empty string if no target built the executable!
  auto buildKey() const -> QString { return m_buildKey; }
  // The BuildTargetInfo corresponding to the buildKey.
  auto buildTargetInfo() const -> BuildTargetInfo;
  auto productNode() const -> ProjectNode*;

  template <class T = ISettingsAspect>
  auto currentSettings(Utils::Id id) const -> T*
  {
    if (const auto a = qobject_cast<GlobalOrProjectAspect*>(aspect(id)))
      return qobject_cast<T*>(a->currentSettings());
    return nullptr;
  }

  using AspectFactory = std::function<Utils::BaseAspect *(Target *)>;

  template <class T>
  static auto registerAspect() -> void
  {
    addAspectFactory([](Target *target) { return new T(target); });
  }

  auto aspectData() const -> QMap<Utils::Id, QVariantMap>;
  auto update() -> void;
  auto macroExpander() const -> const Utils::MacroExpander* { return &m_expander; }

signals:
  auto enabledChanged() -> void;

protected:
  RunConfiguration(Target *target, Utils::Id id);

  /// convenience function to get current build system. Try to avoid.
  auto activeBuildSystem() const -> BuildSystem*;
  using Updater = std::function<void()>;
  auto setUpdater(const Updater &updater) -> void;
  auto createConfigurationIssue(const QString &description) const -> Task;

private:
  // Any additional data should be handled by aspects.
  auto fromMap(const QVariantMap &map) -> bool final;
  auto toMap() const -> QVariantMap final;

  static auto addAspectFactory(const AspectFactory &aspectFactory) -> void;

  friend class RunConfigurationCreationInfo;
  friend class RunConfigurationFactory;
  friend class Target;

  QString m_buildKey;
  CommandLineGetter m_commandLineGetter;
  RunnableModifier m_runnableModifier;
  Updater m_updater;
  Utils::MacroExpander m_expander;
};

class RunConfigurationCreationInfo {
public:
  enum CreationMode {
    AlwaysCreate,
    ManualCreationOnly
  };

  auto create(Target *target) const -> RunConfiguration*;

  const RunConfigurationFactory *factory = nullptr;
  QString buildKey;
  QString displayName;
  QString displayNameUniquifier;
  Utils::FilePath projectFilePath;
  CreationMode creationMode = AlwaysCreate;
  bool useTerminal = false;
};

class PROJECTEXPLORER_EXPORT RunConfigurationFactory {
public:
  RunConfigurationFactory();
  RunConfigurationFactory(const RunConfigurationFactory &) = delete;

  auto operator=(const RunConfigurationFactory &) -> RunConfigurationFactory = delete;
  virtual ~RunConfigurationFactory();
  static auto restore(Target *parent, const QVariantMap &map) -> RunConfiguration*;
  static auto clone(Target *parent, RunConfiguration *source) -> RunConfiguration*;
  static auto creatorsForTarget(Target *parent) -> const QList<RunConfigurationCreationInfo>;
  auto runConfigurationId() const -> Utils::Id { return m_runConfigurationId; }
  static auto decoratedTargetName(const QString &targetName, Target *kit) -> QString;

protected:
  virtual auto availableCreators(Target *target) const -> QList<RunConfigurationCreationInfo>;
  using RunConfigurationCreator = std::function<RunConfiguration *(Target *)>;

  template <class RunConfig>
  auto registerRunConfiguration(Utils::Id runConfigurationId) -> void
  {
    m_creator = [runConfigurationId](Target *t) -> RunConfiguration* {
      return new RunConfig(t, runConfigurationId);
    };
    m_runConfigurationId = runConfigurationId;
  }

  auto addSupportedProjectType(Utils::Id projectTypeId) -> void;
  auto addSupportedTargetDeviceType(Utils::Id deviceTypeId) -> void;
  auto setDecorateDisplayNames(bool on) -> void;

private:
  auto canHandle(Target *target) const -> bool;
  auto create(Target *target) const -> RunConfiguration*;

  friend class RunConfigurationCreationInfo;
  RunConfigurationCreator m_creator;
  Utils::Id m_runConfigurationId;
  QList<Utils::Id> m_supportedProjectTypes;
  QList<Utils::Id> m_supportedTargetDeviceTypes;
  bool m_decorateDisplayNames = false;
};

class PROJECTEXPLORER_EXPORT FixedRunConfigurationFactory : public RunConfigurationFactory {
public:
  explicit FixedRunConfigurationFactory(const QString &displayName, bool addDeviceName = false);
  auto availableCreators(Target *parent) const -> QList<RunConfigurationCreationInfo> override;

private:
  const QString m_fixedBuildTarget;
  const bool m_decorateTargetName;
};

} // namespace ProjectExplorer
