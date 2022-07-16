// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectconfiguration.hpp"
#include "projectexplorer_export.hpp"

#include <memory>

QT_FORWARD_DECLARE_CLASS(QIcon)

namespace Utils {
class MacroExpander;
}

namespace ProjectExplorer {

class BuildConfiguration;
class BuildTargetInfo;
class BuildSystem;
class DeployConfiguration;
class DeploymentData;
class Kit;
class MakeInstallCommand;
class Project;
class ProjectConfigurationModel;
class RunConfiguration;

class TargetPrivate;

class PROJECTEXPLORER_EXPORT Target : public QObject {
  friend class SessionManager; // for setActiveBuild and setActiveDeployConfiguration
  Q_OBJECT

public:
  struct _constructor_tag {
    explicit _constructor_tag() = default;
  };

  Target(Project *parent, Kit *k, _constructor_tag);
  ~Target() override;

  auto isActive() const -> bool;
  auto markAsShuttingDown() -> void;
  auto isShuttingDown() const -> bool;
  auto project() const -> Project*;
  auto kit() const -> Kit*;
  auto buildSystem() const -> BuildSystem*;
  auto id() const -> Utils::Id;
  auto displayName() const -> QString;
  auto toolTip() const -> QString;
  static auto displayNameKey() -> QString;
  static auto deviceTypeKey() -> QString;

  // Build configuration
  auto addBuildConfiguration(BuildConfiguration *bc) -> void;
  auto removeBuildConfiguration(BuildConfiguration *bc) -> bool;
  auto buildConfigurations() const -> const QList<BuildConfiguration*>;
  auto activeBuildConfiguration() const -> BuildConfiguration*;

  // DeployConfiguration
  auto addDeployConfiguration(DeployConfiguration *dc) -> void;
  auto removeDeployConfiguration(DeployConfiguration *dc) -> bool;
  auto deployConfigurations() const -> const QList<DeployConfiguration*>;
  auto activeDeployConfiguration() const -> DeployConfiguration*;

  // Running
  auto runConfigurations() const -> const QList<RunConfiguration*>;
  auto addRunConfiguration(RunConfiguration *rc) -> void;
  auto removeRunConfiguration(RunConfiguration *rc) -> void;
  auto activeRunConfiguration() const -> RunConfiguration*;
  auto setActiveRunConfiguration(RunConfiguration *rc) -> void;
  auto icon() const -> QIcon;
  auto overlayIcon() const -> QIcon;
  auto setOverlayIcon(const QIcon &icon) -> void;
  auto overlayIconToolTip() -> QString;
  auto toMap() const -> QVariantMap;
  auto updateDefaultBuildConfigurations() -> void;
  auto updateDefaultDeployConfigurations() -> void;
  auto updateDefaultRunConfigurations() -> void;
  auto namedSettings(const QString &name) const -> QVariant;
  auto setNamedSettings(const QString &name, const QVariant &value) -> void;
  auto additionalData(Utils::Id id) const -> QVariant;
  auto makeInstallCommand(const QString &installRoot) const -> MakeInstallCommand;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto buildConfigurationModel() const -> ProjectConfigurationModel*;
  auto deployConfigurationModel() const -> ProjectConfigurationModel*;
  auto runConfigurationModel() const -> ProjectConfigurationModel*;
  auto fallbackBuildSystem() const -> BuildSystem*;
  auto deploymentData() const -> DeploymentData;
  auto buildSystemDeploymentData() const -> DeploymentData;
  auto buildTarget(const QString &buildKey) const -> BuildTargetInfo;
  auto activeBuildKey() const -> QString; // Build key of active run configuaration

signals:
  auto targetEnabled(bool) -> void;
  auto iconChanged() -> void;
  auto overlayIconChanged() -> void;
  auto kitChanged() -> void;
  auto parsingStarted() -> void;
  auto parsingFinished(bool) -> void;
  auto buildSystemUpdated(BuildSystem *bs) -> void;

  // TODO clean up signal names
  // might be better to also have aboutToRemove signals
  auto removedRunConfiguration(RunConfiguration *rc) -> void;
  auto addedRunConfiguration(RunConfiguration *rc) -> void;
  auto activeRunConfigurationChanged(RunConfiguration *rc) -> void;
  auto removedBuildConfiguration(BuildConfiguration *bc) -> void;
  auto addedBuildConfiguration(BuildConfiguration *bc) -> void;
  auto activeBuildConfigurationChanged(BuildConfiguration *) -> void;
  auto buildEnvironmentChanged(BuildConfiguration *bc) -> void;
  auto removedDeployConfiguration(DeployConfiguration *dc) -> void;
  auto addedDeployConfiguration(DeployConfiguration *dc) -> void;
  auto activeDeployConfigurationChanged(DeployConfiguration *dc) -> void;
  auto deploymentDataChanged() -> void;

private:
  auto fromMap(const QVariantMap &map) -> bool;
  auto updateDeviceState() -> void;
  auto changeDeployConfigurationEnabled() -> void;
  auto changeRunConfigurationEnabled() -> void;
  auto handleKitUpdates(Kit *k) -> void;
  auto handleKitRemoval(Kit *k) -> void;
  auto setActiveBuildConfiguration(BuildConfiguration *configuration) -> void;
  auto setActiveDeployConfiguration(DeployConfiguration *configuration) -> void;
  const std::unique_ptr<TargetPrivate> d;

  friend class Project;
};

} // namespace ProjectExplorer
