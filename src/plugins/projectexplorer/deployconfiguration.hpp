// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "buildsteplist.hpp"
#include "deploymentdata.hpp"
#include "projectconfiguration.hpp"

namespace ProjectExplorer {

class BuildStepList;
class Target;
class DeployConfigurationFactory;

class PROJECTEXPLORER_EXPORT DeployConfiguration final : public ProjectConfiguration {
  Q_OBJECT
  friend class DeployConfigurationFactory;
  explicit DeployConfiguration(Target *target, Utils::Id id);

public:
  ~DeployConfiguration() override = default;

  auto stepList() -> BuildStepList*;
  auto stepList() const -> const BuildStepList*;
  auto createConfigWidget() -> QWidget*;
  auto fromMap(const QVariantMap &map) -> bool override;
  auto toMap() const -> QVariantMap override;
  auto isActive() const -> bool;
  auto usesCustomDeploymentData() const -> bool { return m_usesCustomDeploymentData; }
  auto setUseCustomDeploymentData(bool enabled) -> void { m_usesCustomDeploymentData = enabled; }
  auto customDeploymentData() const -> DeploymentData { return m_customDeploymentData; }
  auto setCustomDeploymentData(const DeploymentData &data) -> void { m_customDeploymentData = data; }

private:
  BuildStepList m_stepList;
  using WidgetCreator = std::function<QWidget *(DeployConfiguration *)>;
  WidgetCreator m_configWidgetCreator;
  DeploymentData m_customDeploymentData;
  bool m_usesCustomDeploymentData = false;
};

class PROJECTEXPLORER_EXPORT DeployConfigurationFactory {
public:
  using PostRestore = std::function<void(DeployConfiguration *dc, const QVariantMap &)>;

  DeployConfigurationFactory();
  DeployConfigurationFactory(const DeployConfigurationFactory &) = delete;

  auto operator=(const DeployConfigurationFactory &) -> DeployConfigurationFactory = delete;
  virtual ~DeployConfigurationFactory();

  // return possible addition to a target, invalid if there is none
  auto creationId() const -> Utils::Id;
  // the name to display to the user
  auto defaultDisplayName() const -> QString;
  auto create(Target *parent) -> DeployConfiguration*;
  static auto find(Target *parent) -> const QList<DeployConfigurationFactory*>;
  static auto restore(Target *parent, const QVariantMap &map) -> DeployConfiguration*;
  static auto clone(Target *parent, const DeployConfiguration *dc) -> DeployConfiguration*;
  auto addSupportedTargetDeviceType(Utils::Id id) -> void;
  auto setDefaultDisplayName(const QString &defaultDisplayName) -> void;
  auto setSupportedProjectType(Utils::Id id) -> void;

  // Step is only added if condition is not set, or returns true when called.
  auto addInitialStep(Utils::Id stepId, const std::function<bool(Target *)> &condition = {}) -> void;
  auto canHandle(Target *target) const -> bool;
  auto setConfigWidgetCreator(const DeployConfiguration::WidgetCreator &configWidgetCreator) -> void;
  auto setUseDeploymentDataView() -> void;
  auto setPostRestore(const PostRestore &postRestore) -> void { m_postRestore = postRestore; }
  auto postRestore() const -> PostRestore { return m_postRestore; }

protected:
  using DeployConfigurationCreator = std::function<DeployConfiguration *(Target *)>;
  auto setConfigBaseId(Utils::Id deployConfigBaseId) -> void;

private:
  auto createDeployConfiguration(Target *target) -> DeployConfiguration*;
  Utils::Id m_deployConfigBaseId;
  Utils::Id m_supportedProjectType;
  QList<Utils::Id> m_supportedTargetDeviceTypes;
  QList<BuildStepList::StepCreationInfo> m_initialSteps;
  QString m_defaultDisplayName;
  DeployConfiguration::WidgetCreator m_configWidgetCreator;
  PostRestore m_postRestore;
};

class DefaultDeployConfigurationFactory : public DeployConfigurationFactory {
public:
  DefaultDeployConfigurationFactory();
};

} // namespace ProjectExplorer
