// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deployconfiguration.hpp"

#include "buildsteplist.hpp"
#include "buildconfiguration.hpp"
#include "deploymentdataview.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>

using namespace Utils;

namespace ProjectExplorer {

constexpr char BUILD_STEP_LIST_COUNT[] = "ProjectExplorer.BuildConfiguration.BuildStepListCount";
constexpr char BUILD_STEP_LIST_PREFIX[] = "ProjectExplorer.BuildConfiguration.BuildStepList.";
constexpr char USES_DEPLOYMENT_DATA[] = "ProjectExplorer.DeployConfiguration.CustomDataEnabled";
constexpr char DEPLOYMENT_DATA[] = "ProjectExplorer.DeployConfiguration.CustomData";

DeployConfiguration::DeployConfiguration(Target *target, Id id) : ProjectConfiguration(target, id), m_stepList(this, Constants::BUILDSTEPS_DEPLOY)
{
  QTC_CHECK(target && target == this->target());
  //: Default DeployConfiguration display name
  setDefaultDisplayName(tr("Deploy locally"));
}

auto DeployConfiguration::stepList() -> BuildStepList*
{
  return &m_stepList;
}

auto DeployConfiguration::stepList() const -> const BuildStepList*
{
  return &m_stepList;
}

auto DeployConfiguration::createConfigWidget() -> QWidget*
{
  if (!m_configWidgetCreator)
    return nullptr;
  return m_configWidgetCreator(this);
}

auto DeployConfiguration::toMap() const -> QVariantMap
{
  auto map(ProjectConfiguration::toMap());
  map.insert(QLatin1String(BUILD_STEP_LIST_COUNT), 1);
  map.insert(QLatin1String(BUILD_STEP_LIST_PREFIX) + QLatin1Char('0'), m_stepList.toMap());
  map.insert(USES_DEPLOYMENT_DATA, usesCustomDeploymentData());
  QVariantMap deployData;
  for (auto i = 0; i < m_customDeploymentData.fileCount(); ++i) {
    const auto &f = m_customDeploymentData.fileAt(i);
    deployData.insert(f.localFilePath().toString(), f.remoteDirectory());
  }
  map.insert(DEPLOYMENT_DATA, deployData);
  return map;
}

auto DeployConfiguration::fromMap(const QVariantMap &map) -> bool
{
  if (!ProjectConfiguration::fromMap(map))
    return false;

  const auto maxI = map.value(QLatin1String(BUILD_STEP_LIST_COUNT), 0).toInt();
  if (maxI != 1)
    return false;
  const auto data = map.value(QLatin1String(BUILD_STEP_LIST_PREFIX) + QLatin1Char('0')).toMap();
  if (!data.isEmpty()) {
    m_stepList.clear();
    if (!m_stepList.fromMap(data)) {
      qWarning() << "Failed to restore deploy step list";
      m_stepList.clear();
      return false;
    }
  } else {
    qWarning() << "No data for deploy step list found!";
    return false;
  }

  m_usesCustomDeploymentData = map.value(USES_DEPLOYMENT_DATA, false).toBool();
  const auto deployData = map.value(DEPLOYMENT_DATA).toMap();
  for (auto it = deployData.begin(); it != deployData.end(); ++it)
    m_customDeploymentData.addFile(FilePath::fromString(it.key()), it.value().toString());
  return true;
}

auto DeployConfiguration::isActive() const -> bool
{
  return target()->isActive() && target()->activeDeployConfiguration() == this;
}

///
// DeployConfigurationFactory
///

static QList<DeployConfigurationFactory*> g_deployConfigurationFactories;

DeployConfigurationFactory::DeployConfigurationFactory()
{
  g_deployConfigurationFactories.append(this);
}

DeployConfigurationFactory::~DeployConfigurationFactory()
{
  g_deployConfigurationFactories.removeOne(this);
}

auto DeployConfigurationFactory::creationId() const -> Id
{
  return m_deployConfigBaseId;
}

auto DeployConfigurationFactory::defaultDisplayName() const -> QString
{
  return m_defaultDisplayName;
}

auto DeployConfigurationFactory::canHandle(Target *target) const -> bool
{
  if (m_supportedProjectType.isValid()) {
    if (target->project()->id() != m_supportedProjectType)
      return false;
  }

  if (containsType(target->project()->projectIssues(target->kit()), Task::TaskType::Error))
    return false;

  if (!m_supportedTargetDeviceTypes.isEmpty()) {
    if (!m_supportedTargetDeviceTypes.contains(DeviceTypeKitAspect::deviceTypeId(target->kit())))
      return false;
  }

  return true;
}

auto DeployConfigurationFactory::setConfigWidgetCreator(const DeployConfiguration::WidgetCreator &configWidgetCreator) -> void
{
  m_configWidgetCreator = configWidgetCreator;
}

auto DeployConfigurationFactory::setUseDeploymentDataView() -> void
{
  m_configWidgetCreator = [](DeployConfiguration *dc) {
    return new Internal::DeploymentDataView(dc);
  };
}

auto DeployConfigurationFactory::setConfigBaseId(Id deployConfigBaseId) -> void
{
  m_deployConfigBaseId = deployConfigBaseId;
}

auto DeployConfigurationFactory::createDeployConfiguration(Target *t) -> DeployConfiguration*
{
  const auto dc = new DeployConfiguration(t, m_deployConfigBaseId);
  dc->setDefaultDisplayName(m_defaultDisplayName);
  dc->m_configWidgetCreator = m_configWidgetCreator;
  return dc;
}

auto DeployConfigurationFactory::create(Target *parent) -> DeployConfiguration*
{
  QTC_ASSERT(canHandle(parent), return nullptr);
  const auto dc = createDeployConfiguration(parent);
  QTC_ASSERT(dc, return nullptr);
  const auto stepList = dc->stepList();
  for (const auto &info : qAsConst(m_initialSteps)) {
    if (!info.condition || info.condition(parent))
      stepList->appendStep(info.stepId);
  }
  return dc;
}

auto DeployConfigurationFactory::clone(Target *parent, const DeployConfiguration *source) -> DeployConfiguration*
{
  return restore(parent, source->toMap());
}

auto DeployConfigurationFactory::restore(Target *parent, const QVariantMap &map) -> DeployConfiguration*
{
  const auto id = idFromMap(map);
  const auto factory = findOrDefault(g_deployConfigurationFactories, [parent, id](DeployConfigurationFactory *f) {
    if (!f->canHandle(parent))
      return false;
    return id.name().startsWith(f->m_deployConfigBaseId.name());
  });
  if (!factory)
    return nullptr;
  auto dc = factory->createDeployConfiguration(parent);
  QTC_ASSERT(dc, return nullptr);
  if (!dc->fromMap(map)) {
    delete dc;
    dc = nullptr;
  } else if (factory->postRestore()) {
    factory->postRestore()(dc, map);
  }

  return dc;
}

auto DeployConfigurationFactory::find(Target *parent) -> const QList<DeployConfigurationFactory*>
{
  return filtered(g_deployConfigurationFactories, [&parent](DeployConfigurationFactory *factory) {
    return factory->canHandle(parent);
  });
}

auto DeployConfigurationFactory::addSupportedTargetDeviceType(Id id) -> void
{
  m_supportedTargetDeviceTypes.append(id);
}

auto DeployConfigurationFactory::setDefaultDisplayName(const QString &defaultDisplayName) -> void
{
  m_defaultDisplayName = defaultDisplayName;
}

auto DeployConfigurationFactory::setSupportedProjectType(Id id) -> void
{
  m_supportedProjectType = id;
}

auto DeployConfigurationFactory::addInitialStep(Id stepId, const std::function<bool (Target *)> &condition) -> void
{
  m_initialSteps.append({stepId, condition});
}

///
// DefaultDeployConfigurationFactory
///

DefaultDeployConfigurationFactory::DefaultDeployConfigurationFactory()
{
  setConfigBaseId("ProjectExplorer.DefaultDeployConfiguration");
  addSupportedTargetDeviceType(Constants::DESKTOP_DEVICE_TYPE);
  //: Display name of the default deploy configuration
  setDefaultDisplayName(DeployConfiguration::tr("Deploy Configuration"));
}

} // namespace ProjectExplorer
