// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectconfigurationmodel.hpp"
#include "target.hpp"

#include "buildconfiguration.hpp"
#include "buildinfo.hpp"
#include "buildmanager.hpp"
#include "buildsystem.hpp"
#include "buildtargetinfo.hpp"
#include "deployconfiguration.hpp"
#include "deploymentdata.hpp"
#include "devicesupport/devicemanager.hpp"
#include "environmentaspect.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "miniprojecttargetselector.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorericons.hpp"
#include "projectexplorersettings.hpp"
#include "runconfiguration.hpp"
#include "runconfigurationaspects.hpp"
#include "session.hpp"

#include <core/coreconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/commandline.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QDebug>
#include <QIcon>
#include <QPainter>

#include <limits>

using namespace Utils;

namespace ProjectExplorer {

constexpr char ACTIVE_BC_KEY[] = "ProjectExplorer.Target.ActiveBuildConfiguration";
constexpr char BC_KEY_PREFIX[] = "ProjectExplorer.Target.BuildConfiguration.";
constexpr char BC_COUNT_KEY[] = "ProjectExplorer.Target.BuildConfigurationCount";
constexpr char ACTIVE_DC_KEY[] = "ProjectExplorer.Target.ActiveDeployConfiguration";
constexpr char DC_KEY_PREFIX[] = "ProjectExplorer.Target.DeployConfiguration.";
constexpr char DC_COUNT_KEY[] = "ProjectExplorer.Target.DeployConfigurationCount";
constexpr char ACTIVE_RC_KEY[] = "ProjectExplorer.Target.ActiveRunConfiguration";
constexpr char RC_KEY_PREFIX[] = "ProjectExplorer.Target.RunConfiguration.";
constexpr char RC_COUNT_KEY[] = "ProjectExplorer.Target.RunConfigurationCount";
constexpr char PLUGIN_SETTINGS_KEY[] = "ProjectExplorer.Target.PluginSettings";

static auto formatDeviceInfo(const IDevice::DeviceInfo &input) -> QString
{
  const auto lines = transform(input, [](const IDevice::DeviceInfoItem &i) {
    return QString::fromLatin1("<b>%1:</b> %2").arg(i.key, i.value);
  });
  return lines.join(QLatin1String("<br>"));
}

// -------------------------------------------------------------------------
// Target
// -------------------------------------------------------------------------

class TargetPrivate {
public:
  TargetPrivate(Target *t, Kit *k) : m_kit(k), m_buildConfigurationModel(t), m_deployConfigurationModel(t), m_runConfigurationModel(t) { }

  ~TargetPrivate()
  {
    delete m_buildSystem;
  }

  QIcon m_overlayIcon;
  QList<BuildConfiguration*> m_buildConfigurations;
  QPointer<BuildConfiguration> m_activeBuildConfiguration;
  QList<DeployConfiguration*> m_deployConfigurations;
  DeployConfiguration *m_activeDeployConfiguration = nullptr;
  QList<RunConfiguration*> m_runConfigurations;
  RunConfiguration *m_activeRunConfiguration = nullptr;
  QVariantMap m_pluginSettings;
  Kit *const m_kit;
  MacroExpander m_macroExpander;
  BuildSystem *m_buildSystem = nullptr;
  ProjectConfigurationModel m_buildConfigurationModel;
  ProjectConfigurationModel m_deployConfigurationModel;
  ProjectConfigurationModel m_runConfigurationModel;

  bool m_shuttingDown = false;
};

Target::Target(Project *project, Kit *k, _constructor_tag) : QObject(project), d(std::make_unique<TargetPrivate>(this, k))
{
  // Note: nullptr is a valid state for the per-buildConfig systems.
  d->m_buildSystem = project->createBuildSystem(this);

  QTC_CHECK(d->m_kit);
  connect(DeviceManager::instance(), &DeviceManager::updated, this, &Target::updateDeviceState);

  connect(this, &Target::parsingStarted, this, [this, project] {
    emit project->anyParsingStarted(this);
  });

  connect(this, &Target::parsingFinished, this, [this, project](bool success) {
    if (success && this == SessionManager::startupTarget())
      updateDefaultRunConfigurations();
    // For testing.
    emit SessionManager::instance()->projectFinishedParsing(project);
    emit project->anyParsingFinished(this, success);
  }, Qt::QueuedConnection); // Must wait for run configs to change their enabled state.

  const auto km = KitManager::instance();
  connect(km, &KitManager::kitUpdated, this, &Target::handleKitUpdates);
  connect(km, &KitManager::kitRemoved, this, &Target::handleKitRemoval);

  d->m_macroExpander.setDisplayName(tr("Target Settings"));
  d->m_macroExpander.setAccumulating(true);

  d->m_macroExpander.registerSubProvider([this] { return kit()->macroExpander(); });

  d->m_macroExpander.registerVariable("sourceDir", tr("Source directory"), [project] { return project->projectDirectory().toUserOutput(); });
  d->m_macroExpander.registerVariable("BuildSystem:Name", tr("Build system"), [this] {
    if (const BuildSystem *const bs = buildSystem())
      return bs->name();
    return QString();
  });

  // TODO: Remove in ~4.16.
  d->m_macroExpander.registerVariable(Constants::VAR_CURRENTPROJECT_NAME, QCoreApplication::translate("ProjectExplorer", "Name of current project"), [project] { return project->displayName(); }, false);
  d->m_macroExpander.registerVariable("Project:Name", QCoreApplication::translate("ProjectExplorer", "Name of current project"), [project] { return project->displayName(); });

  d->m_macroExpander.registerVariable("CurrentRun:Name", tr("The currently active run configuration's name."), [this]() -> QString {
    if (const auto rc = activeRunConfiguration())
      return rc->displayName();
    return QString();
  }, false);
  d->m_macroExpander.registerFileVariables("CurrentRun:Executable", tr("The currently active run configuration's executable (if applicable)."), [this]() -> FilePath {
    if (const auto rc = activeRunConfiguration())
      return rc->commandLine().executable();
    return FilePath();
  }, false);
  d->m_macroExpander.registerPrefix("CurrentRun:Env", tr("Variables in the current run environment."), [this](const QString &var) {
    if (const auto rc = activeRunConfiguration()) {
      if (const auto envAspect = rc->aspect<EnvironmentAspect>())
        return envAspect->environment().expandedValueForKey(var);
    }
    return QString();
  }, false);
  d->m_macroExpander.registerVariable("CurrentRun:WorkingDir", tr("The currently active run configuration's working directory."), [this] {
    if (const auto rc = activeRunConfiguration()) {
      if (const auto wdAspect = rc->aspect<WorkingDirectoryAspect>())
        return wdAspect->workingDirectory().toString();
    }
    return QString();
  }, false);
}

Target::~Target()
{
  qDeleteAll(d->m_buildConfigurations);
  qDeleteAll(d->m_deployConfigurations);
  qDeleteAll(d->m_runConfigurations);
}

auto Target::handleKitUpdates(Kit *k) -> void
{
  if (k != d->m_kit)
    return;

  updateDefaultDeployConfigurations();
  updateDeviceState(); // in case the device changed...

  emit iconChanged();
  emit kitChanged();
}

auto Target::handleKitRemoval(Kit *k) -> void
{
  if (k != d->m_kit)
    return;
  project()->removeTarget(this);
}

auto Target::isActive() const -> bool
{
  return project()->activeTarget() == this;
}

auto Target::markAsShuttingDown() -> void
{
  d->m_shuttingDown = true;
}

auto Target::isShuttingDown() const -> bool
{
  return d->m_shuttingDown;
}

auto Target::project() const -> Project*
{
  return static_cast<Project*>(parent());
}

auto Target::kit() const -> Kit*
{
  return d->m_kit;
}

auto Target::buildSystem() const -> BuildSystem*
{
  if (d->m_activeBuildConfiguration)
    return d->m_activeBuildConfiguration->buildSystem();

  return d->m_buildSystem;
}

auto Target::fallbackBuildSystem() const -> BuildSystem*
{
  return d->m_buildSystem;
}

auto Target::deploymentData() const -> DeploymentData
{
  const DeployConfiguration *const dc = activeDeployConfiguration();
  if (dc && dc->usesCustomDeploymentData())
    return dc->customDeploymentData();
  return buildSystemDeploymentData();
}

auto Target::buildSystemDeploymentData() const -> DeploymentData
{
  QTC_ASSERT(buildSystem(), return {});
  return buildSystem()->deploymentData();
}

auto Target::buildTarget(const QString &buildKey) const -> BuildTargetInfo
{
  QTC_ASSERT(buildSystem(), return {});
  return buildSystem()->buildTarget(buildKey);
}

auto Target::activeBuildKey() const -> QString
{
  // Should not happen. If it does, return a buildKey that wont be found in
  // the project tree, so that the project()->findNodeForBuildKey(buildKey)
  // returns null.
  QTC_ASSERT(d->m_activeRunConfiguration, return QString(QChar(0)));
  return d->m_activeRunConfiguration->buildKey();
}

auto Target::id() const -> Id
{
  return d->m_kit->id();
}

auto Target::displayName() const -> QString
{
  return d->m_kit->displayName();
}

auto Target::toolTip() const -> QString
{
  return d->m_kit->toHtml();
}

auto Target::displayNameKey() -> QString
{
  return QString("ProjectExplorer.ProjectConfiguration.DisplayName");
}

auto Target::deviceTypeKey() -> QString
{
  return QString("DeviceType");
}

auto Target::addBuildConfiguration(BuildConfiguration *bc) -> void
{
  QTC_ASSERT(bc && !d->m_buildConfigurations.contains(bc), return);
  Q_ASSERT(bc->target() == this);

  // Check that we don't have a configuration with the same displayName
  auto configurationDisplayName = bc->displayName();
  const auto displayNames = transform(d->m_buildConfigurations, &BuildConfiguration::displayName);
  configurationDisplayName = makeUniquelyNumbered(configurationDisplayName, displayNames);
  if (configurationDisplayName != bc->displayName()) {
    if (bc->usesDefaultDisplayName())
      bc->setDefaultDisplayName(configurationDisplayName);
    else
      bc->setDisplayName(configurationDisplayName);
  }

  // add it
  d->m_buildConfigurations.push_back(bc);

  ProjectExplorerPlugin::targetSelector()->addedBuildConfiguration(bc);
  emit addedBuildConfiguration(bc);
  d->m_buildConfigurationModel.addProjectConfiguration(bc);

  if (!activeBuildConfiguration())
    setActiveBuildConfiguration(bc);
}

auto Target::removeBuildConfiguration(BuildConfiguration *bc) -> bool
{
  //todo: this might be error prone
  if (!d->m_buildConfigurations.contains(bc))
    return false;

  if (BuildManager::isBuilding(bc))
    return false;

  d->m_buildConfigurations.removeOne(bc);

  if (activeBuildConfiguration() == bc) {
    if (d->m_buildConfigurations.isEmpty())
      SessionManager::setActiveBuildConfiguration(this, nullptr, SetActive::Cascade);
    else
      SessionManager::setActiveBuildConfiguration(this, d->m_buildConfigurations.at(0), SetActive::Cascade);
  }

  emit removedBuildConfiguration(bc);
  ProjectExplorerPlugin::targetSelector()->removedBuildConfiguration(bc);
  d->m_buildConfigurationModel.removeProjectConfiguration(bc);

  delete bc;
  return true;
}

auto Target::buildConfigurations() const -> const QList<BuildConfiguration*>
{
  return d->m_buildConfigurations;
}

auto Target::activeBuildConfiguration() const -> BuildConfiguration*
{
  return d->m_activeBuildConfiguration;
}

auto Target::setActiveBuildConfiguration(BuildConfiguration *bc) -> void
{
  if ((!bc && d->m_buildConfigurations.isEmpty()) || (bc && d->m_buildConfigurations.contains(bc) && bc != d->m_activeBuildConfiguration)) {
    d->m_activeBuildConfiguration = bc;
    emit activeBuildConfigurationChanged(d->m_activeBuildConfiguration);
    ProjectExplorerPlugin::updateActions();
  }
}

auto Target::addDeployConfiguration(DeployConfiguration *dc) -> void
{
  QTC_ASSERT(dc && !d->m_deployConfigurations.contains(dc), return);
  Q_ASSERT(dc->target() == this);

  // Check that we don't have a configuration with the same displayName
  auto configurationDisplayName = dc->displayName();
  const auto displayNames = transform(d->m_deployConfigurations, &DeployConfiguration::displayName);
  configurationDisplayName = makeUniquelyNumbered(configurationDisplayName, displayNames);
  dc->setDisplayName(configurationDisplayName);

  // add it
  d->m_deployConfigurations.push_back(dc);

  ProjectExplorerPlugin::targetSelector()->addedDeployConfiguration(dc);
  d->m_deployConfigurationModel.addProjectConfiguration(dc);
  emit addedDeployConfiguration(dc);

  if (!d->m_activeDeployConfiguration)
    setActiveDeployConfiguration(dc);
  Q_ASSERT(activeDeployConfiguration());
}

auto Target::removeDeployConfiguration(DeployConfiguration *dc) -> bool
{
  //todo: this might be error prone
  if (!d->m_deployConfigurations.contains(dc))
    return false;

  if (BuildManager::isBuilding(dc))
    return false;

  d->m_deployConfigurations.removeOne(dc);

  if (activeDeployConfiguration() == dc) {
    if (d->m_deployConfigurations.isEmpty())
      SessionManager::setActiveDeployConfiguration(this, nullptr, SetActive::Cascade);
    else
      SessionManager::setActiveDeployConfiguration(this, d->m_deployConfigurations.at(0), SetActive::Cascade);
  }

  ProjectExplorerPlugin::targetSelector()->removedDeployConfiguration(dc);
  d->m_deployConfigurationModel.removeProjectConfiguration(dc);
  emit removedDeployConfiguration(dc);

  delete dc;
  return true;
}

auto Target::deployConfigurations() const -> const QList<DeployConfiguration*>
{
  return d->m_deployConfigurations;
}

auto Target::activeDeployConfiguration() const -> DeployConfiguration*
{
  return d->m_activeDeployConfiguration;
}

auto Target::setActiveDeployConfiguration(DeployConfiguration *dc) -> void
{
  if ((!dc && d->m_deployConfigurations.isEmpty()) || (dc && d->m_deployConfigurations.contains(dc) && dc != d->m_activeDeployConfiguration)) {
    d->m_activeDeployConfiguration = dc;
    emit activeDeployConfigurationChanged(d->m_activeDeployConfiguration);
  }
  updateDeviceState();
}

auto Target::runConfigurations() const -> const QList<RunConfiguration*>
{
  return d->m_runConfigurations;
}

auto Target::addRunConfiguration(RunConfiguration *rc) -> void
{
  QTC_ASSERT(rc && !d->m_runConfigurations.contains(rc), return);
  Q_ASSERT(rc->target() == this);

  // Check that we don't have a configuration with the same displayName
  auto configurationDisplayName = rc->displayName();
  if (!configurationDisplayName.isEmpty()) {
    const auto displayNames = transform(d->m_runConfigurations, &RunConfiguration::displayName);
    configurationDisplayName = makeUniquelyNumbered(configurationDisplayName, displayNames);
    rc->setDisplayName(configurationDisplayName);
  }

  d->m_runConfigurations.push_back(rc);

  ProjectExplorerPlugin::targetSelector()->addedRunConfiguration(rc);
  d->m_runConfigurationModel.addProjectConfiguration(rc);
  emit addedRunConfiguration(rc);

  if (!activeRunConfiguration())
    setActiveRunConfiguration(rc);
}

auto Target::removeRunConfiguration(RunConfiguration *rc) -> void
{
  QTC_ASSERT(rc && d->m_runConfigurations.contains(rc), return);

  d->m_runConfigurations.removeOne(rc);

  if (activeRunConfiguration() == rc) {
    if (d->m_runConfigurations.isEmpty())
      setActiveRunConfiguration(nullptr);
    else
      setActiveRunConfiguration(d->m_runConfigurations.at(0));
  }

  emit removedRunConfiguration(rc);
  ProjectExplorerPlugin::targetSelector()->removedRunConfiguration(rc);
  d->m_runConfigurationModel.removeProjectConfiguration(rc);

  delete rc;
}

auto Target::activeRunConfiguration() const -> RunConfiguration*
{
  return d->m_activeRunConfiguration;
}

auto Target::setActiveRunConfiguration(RunConfiguration *rc) -> void
{
  if (isShuttingDown())
    return;

  if ((!rc && d->m_runConfigurations.isEmpty()) || (rc && d->m_runConfigurations.contains(rc) && rc != d->m_activeRunConfiguration)) {
    d->m_activeRunConfiguration = rc;
    emit activeRunConfigurationChanged(d->m_activeRunConfiguration);
    ProjectExplorerPlugin::updateActions();
  }
  updateDeviceState();
}

auto Target::icon() const -> QIcon
{
  return d->m_kit->icon();
}

auto Target::overlayIcon() const -> QIcon
{
  return d->m_overlayIcon;
}

auto Target::setOverlayIcon(const QIcon &icon) -> void
{
  d->m_overlayIcon = icon;
  emit overlayIconChanged();
}

auto Target::overlayIconToolTip() -> QString
{
  const auto current = DeviceKitAspect::device(kit());
  return current.isNull() ? QString() : formatDeviceInfo(current->deviceInformation());
}

auto Target::toMap() const -> QVariantMap
{
  if (!d->m_kit) // Kit was deleted, target is only around to be copied.
    return QVariantMap();

  QVariantMap map;
  map.insert(displayNameKey(), displayName());
  map.insert(deviceTypeKey(), DeviceTypeKitAspect::deviceTypeId(kit()).toSetting());

  {
    // FIXME: For compatibility within the 4.11 cycle, remove this block later.
    // This is only read by older versions of Creator, but even there not actively used.
    const char CONFIGURATION_ID_KEY[] = "ProjectExplorer.ProjectConfiguration.Id";
    const char DEFAULT_DISPLAY_NAME_KEY[] = "ProjectExplorer.ProjectConfiguration.DefaultDisplayName";
    map.insert(QLatin1String(CONFIGURATION_ID_KEY), id().toSetting());
    map.insert(QLatin1String(DEFAULT_DISPLAY_NAME_KEY), displayName());
  }

  const auto bcs = buildConfigurations();
  map.insert(QLatin1String(ACTIVE_BC_KEY), bcs.indexOf(d->m_activeBuildConfiguration));
  map.insert(QLatin1String(BC_COUNT_KEY), bcs.size());
  for (auto i = 0; i < bcs.size(); ++i)
    map.insert(QString::fromLatin1(BC_KEY_PREFIX) + QString::number(i), bcs.at(i)->toMap());

  const auto dcs = deployConfigurations();
  map.insert(QLatin1String(ACTIVE_DC_KEY), dcs.indexOf(d->m_activeDeployConfiguration));
  map.insert(QLatin1String(DC_COUNT_KEY), dcs.size());
  for (auto i = 0; i < dcs.size(); ++i)
    map.insert(QString::fromLatin1(DC_KEY_PREFIX) + QString::number(i), dcs.at(i)->toMap());

  const auto rcs = runConfigurations();
  map.insert(QLatin1String(ACTIVE_RC_KEY), rcs.indexOf(d->m_activeRunConfiguration));
  map.insert(QLatin1String(RC_COUNT_KEY), rcs.size());
  for (auto i = 0; i < rcs.size(); ++i)
    map.insert(QString::fromLatin1(RC_KEY_PREFIX) + QString::number(i), rcs.at(i)->toMap());

  if (!d->m_pluginSettings.isEmpty())
    map.insert(QLatin1String(PLUGIN_SETTINGS_KEY), d->m_pluginSettings);

  return map;
}

auto Target::updateDefaultBuildConfigurations() -> void
{
  const auto bcFactory = BuildConfigurationFactory::find(this);
  if (!bcFactory) {
    qWarning("No build configuration factory found for target id '%s'.", qPrintable(id().toString()));
    return;
  }
  for (const auto &info : bcFactory->allAvailableSetups(kit(), project()->projectFilePath())) {
    if (const auto bc = bcFactory->create(this, info))
      addBuildConfiguration(bc);
  }
}

auto Target::updateDefaultDeployConfigurations() -> void
{
  auto dcFactories = DeployConfigurationFactory::find(this);
  if (dcFactories.isEmpty()) {
    qWarning("No deployment configuration factory found for target id '%s'.", qPrintable(id().toString()));
    return;
  }

  QList<Id> dcIds;
  foreach(DeployConfigurationFactory *dcFactory, dcFactories)
    dcIds.append(dcFactory->creationId());

  auto dcList = deployConfigurations();
  auto toCreate = dcIds;

  foreach(DeployConfiguration *dc, dcList) {
    if (dcIds.contains(dc->id()))
      toCreate.removeOne(dc->id());
    else
      removeDeployConfiguration(dc);
  }

  foreach(Utils::Id id, toCreate) {
    foreach(DeployConfigurationFactory *dcFactory, dcFactories) {
      if (dcFactory->creationId() == id) {
        const auto dc = dcFactory->create(this);
        if (dc) {
          QTC_CHECK(dc->id() == id);
          addDeployConfiguration(dc);
        }
      }
    }
  }
}

auto Target::updateDefaultRunConfigurations() -> void
{
  // Manual and Auto
  const auto creators = RunConfigurationFactory::creatorsForTarget(this);

  if (creators.isEmpty()) {
    qWarning("No run configuration factory found for target id '%s'.", qPrintable(id().toString()));
    return;
  }

  QList<RunConfiguration*> existingConfigured;   // Existing configured RCs
  QList<RunConfiguration*> existingUnconfigured; // Existing unconfigured RCs
  QList<RunConfiguration*> newConfigured;        // NEW configured Rcs
  QList<RunConfiguration*> newUnconfigured;      // NEW unconfigured RCs

  // sort existing RCs into configured/unconfigured.
  std::tie(existingConfigured, existingUnconfigured) = partition(runConfigurations(), [](const RunConfiguration *rc) { return rc->isConfigured(); });
  int configuredCount = existingConfigured.count();

  // Put outdated RCs into toRemove, do not bother with factories
  // that produce already existing RCs
  QList<RunConfiguration*> toRemove;
  QList<RunConfigurationCreationInfo> existing;
  foreach(RunConfiguration *rc, existingConfigured) {
    auto present = false;
    for (const auto &item : creators) {
      auto buildKey = rc->buildKey();
      if (item.factory->runConfigurationId() == rc->id() && item.buildKey == buildKey) {
        existing.append(item);
        present = true;
      }
    }
    if (!present)
      toRemove.append(rc);
  }
  configuredCount -= toRemove.count();

  auto removeExistingUnconfigured = false;
  if (ProjectExplorerPlugin::projectExplorerSettings().automaticallyCreateRunConfigurations) {
    // Create new "automatic" RCs and put them into newConfigured/newUnconfigured
    foreach(const RunConfigurationCreationInfo &item, creators) {
      if (item.creationMode == RunConfigurationCreationInfo::ManualCreationOnly)
        continue;
      auto exists = false;
      for (const auto &ex : existing) {
        if (ex.factory == item.factory && ex.buildKey == item.buildKey)
          exists = true;
      }
      if (exists)
        continue;

      auto rc = item.create(this);
      if (!rc)
        continue;
      QTC_CHECK(rc->id() == item.factory->runConfigurationId());
      if (!rc->isConfigured())
        newUnconfigured << rc;
      else
        newConfigured << rc;
    }
    configuredCount += newConfigured.count();

    // Decide what to do with the different categories:
    if (configuredCount > 0) {
      // new non-Custom Executable RCs were added
      removeExistingUnconfigured = true;
      qDeleteAll(newUnconfigured);
      newUnconfigured.clear();
    } else {
      // no new RCs, use old or new CERCs?
      if (!existingUnconfigured.isEmpty()) {
        qDeleteAll(newUnconfigured);
        newUnconfigured.clear();
      }
    }
  }

  // Do actual changes:
  foreach(RunConfiguration *rc, newConfigured)
    addRunConfiguration(rc);
  foreach(RunConfiguration *rc, newUnconfigured)
    addRunConfiguration(rc);

  // Generate complete list of RCs to remove later:
  QList<RunConfiguration*> removalList;
  foreach(RunConfiguration *rc, toRemove) {
    removalList << rc;
    existingConfigured.removeOne(rc); // make sure to also remove them from existingConfigured!
  }

  if (removeExistingUnconfigured) {
    removalList.append(existingUnconfigured);
    existingUnconfigured.clear();
  }

  // Make sure a configured RC will be active after we delete the RCs:
  auto active = activeRunConfiguration();
  if (active && (removalList.contains(active) || !active->isEnabled())) {
    auto newConfiguredDefault = newConfigured.isEmpty() ? nullptr : newConfigured.at(0);

    auto rc = findOrDefault(existingConfigured, [](RunConfiguration *rc) { return rc->isEnabled(); });
    if (!rc) {
      rc = findOr(newConfigured, newConfiguredDefault, equal(&RunConfiguration::displayName, project()->displayName()));
    }
    if (!rc)
      rc = newUnconfigured.isEmpty() ? nullptr : newUnconfigured.at(0);
    if (!rc) {
      // No RCs will be deleted, so use the one that will emit the minimum number of signals.
      // One signal will be emitted from the next setActiveRunConfiguration, another one
      // when the RC gets removed (and the activeRunConfiguration turns into a nullptr).
      rc = removalList.isEmpty() ? nullptr : removalList.last();
    }

    if (rc)
      setActiveRunConfiguration(rc);
  }

  // Remove the RCs that are no longer needed:
  foreach(RunConfiguration *rc, removalList)
    removeRunConfiguration(rc);
}

auto Target::namedSettings(const QString &name) const -> QVariant
{
  return d->m_pluginSettings.value(name);
}

auto Target::setNamedSettings(const QString &name, const QVariant &value) -> void
{
  if (value.isNull())
    d->m_pluginSettings.remove(name);
  else
    d->m_pluginSettings.insert(name, value);
}

auto Target::additionalData(Id id) const -> QVariant
{
  if (const BuildSystem *bs = buildSystem())
    return bs->additionalData(id);

  return {};
}

auto Target::makeInstallCommand(const QString &installRoot) const -> MakeInstallCommand
{
  return project()->makeInstallCommand(this, installRoot);
}

auto Target::macroExpander() const -> MacroExpander*
{
  return &d->m_macroExpander;
}

auto Target::buildConfigurationModel() const -> ProjectConfigurationModel*
{
  return &d->m_buildConfigurationModel;
}

auto Target::deployConfigurationModel() const -> ProjectConfigurationModel*
{
  return &d->m_deployConfigurationModel;
}

auto Target::runConfigurationModel() const -> ProjectConfigurationModel*
{
  return &d->m_runConfigurationModel;
}

auto Target::updateDeviceState() -> void
{
  const auto current = DeviceKitAspect::device(kit());

  QIcon overlay;
  static const auto disconnected = Icons::DEVICE_DISCONNECTED_INDICATOR_OVERLAY.icon();
  if (current.isNull()) {
    overlay = disconnected;
  } else {
    switch (current->deviceState()) {
    case IDevice::DeviceStateUnknown:
      overlay = QIcon();
      return;
    case IDevice::DeviceReadyToUse: {
      static const auto ready = Icons::DEVICE_READY_INDICATOR_OVERLAY.icon();
      overlay = ready;
      break;
    }
    case IDevice::DeviceConnected: {
      static const auto connected = Icons::DEVICE_CONNECTED_INDICATOR_OVERLAY.icon();
      overlay = connected;
      break;
    }
    case IDevice::DeviceDisconnected:
      overlay = disconnected;
      break;
    default:
      break;
    }
  }

  setOverlayIcon(overlay);
}

auto Target::fromMap(const QVariantMap &map) -> bool
{
  QTC_ASSERT(d->m_kit == KitManager::kit(id()), return false);

  bool ok;
  auto bcCount = map.value(QLatin1String(BC_COUNT_KEY), 0).toInt(&ok);
  if (!ok || bcCount < 0)
    bcCount = 0;
  auto activeConfiguration = map.value(QLatin1String(ACTIVE_BC_KEY), 0).toInt(&ok);
  if (!ok || activeConfiguration < 0)
    activeConfiguration = 0;
  if (0 > activeConfiguration || bcCount < activeConfiguration)
    activeConfiguration = 0;

  for (auto i = 0; i < bcCount; ++i) {
    const QString key = QString::fromLatin1(BC_KEY_PREFIX) + QString::number(i);
    if (!map.contains(key))
      return false;
    const auto valueMap = map.value(key).toMap();
    const auto bc = BuildConfigurationFactory::restore(this, valueMap);
    if (!bc) {
      qWarning("No factory found to restore build configuration!");
      continue;
    }
    QTC_CHECK(bc->id() == ProjectExplorer::idFromMap(valueMap));
    addBuildConfiguration(bc);
    if (i == activeConfiguration)
      setActiveBuildConfiguration(bc);
  }
  if (buildConfigurations().isEmpty() && BuildConfigurationFactory::find(this))
    return false;

  auto dcCount = map.value(QLatin1String(DC_COUNT_KEY), 0).toInt(&ok);
  if (!ok || dcCount < 0)
    dcCount = 0;
  activeConfiguration = map.value(QLatin1String(ACTIVE_DC_KEY), 0).toInt(&ok);
  if (!ok || activeConfiguration < 0)
    activeConfiguration = 0;
  if (0 > activeConfiguration || dcCount < activeConfiguration)
    activeConfiguration = 0;

  for (auto i = 0; i < dcCount; ++i) {
    const QString key = QString::fromLatin1(DC_KEY_PREFIX) + QString::number(i);
    if (!map.contains(key))
      return false;
    auto valueMap = map.value(key).toMap();
    const auto dc = DeployConfigurationFactory::restore(this, valueMap);
    if (!dc) {
      auto id = idFromMap(valueMap);
      qWarning("No factory found to restore deployment configuration of id '%s'!", id.isValid() ? qPrintable(id.toString()) : "UNKNOWN");
      continue;
    }
    QTC_CHECK(dc->id() == ProjectExplorer::idFromMap(valueMap));
    addDeployConfiguration(dc);
    if (i == activeConfiguration)
      setActiveDeployConfiguration(dc);
  }

  auto rcCount = map.value(QLatin1String(RC_COUNT_KEY), 0).toInt(&ok);
  if (!ok || rcCount < 0)
    rcCount = 0;
  activeConfiguration = map.value(QLatin1String(ACTIVE_RC_KEY), 0).toInt(&ok);
  if (!ok || activeConfiguration < 0)
    activeConfiguration = 0;
  if (0 > activeConfiguration || rcCount < activeConfiguration)
    activeConfiguration = 0;

  for (auto i = 0; i < rcCount; ++i) {
    const QString key = QString::fromLatin1(RC_KEY_PREFIX) + QString::number(i);
    if (!map.contains(key))
      return false;

    // Ignore missing RCs: We will just populate them using the default ones.
    auto valueMap = map.value(key).toMap();
    const auto rc = RunConfigurationFactory::restore(this, valueMap);
    if (!rc)
      continue;
    const auto theIdFromMap = idFromMap(valueMap);
    if (!theIdFromMap.toString().contains("///::///")) {
      // Hack for cmake 4.10 -> 4.11
      QTC_CHECK(rc->id().withSuffix(rc->buildKey()) == theIdFromMap);
    }
    addRunConfiguration(rc);
    if (i == activeConfiguration)
      setActiveRunConfiguration(rc);
  }

  if (map.contains(QLatin1String(PLUGIN_SETTINGS_KEY)))
    d->m_pluginSettings = map.value(QLatin1String(PLUGIN_SETTINGS_KEY)).toMap();

  return true;
}

} // namespace ProjectExplorer
