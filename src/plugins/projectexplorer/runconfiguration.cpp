// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runconfiguration.hpp"

#include "abi.hpp"
#include "buildconfiguration.hpp"
#include "buildsystem.hpp"
#include "environmentaspect.hpp"
#include "kitinformation.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectnodes.hpp"
#include "runconfigurationaspects.hpp"
#include "runcontrol.hpp"
#include "session.hpp"
#include "target.hpp"
#include "toolchain.hpp"

#include <utils/algorithm.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/detailswidget.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <core/icontext.hpp>
#include <core/icore.hpp>

#include <QDir>
#include <QFormLayout>
#include <QHash>
#include <QPushButton>
#include <QTimer>
#include <QLoggingCategory>
#include <QSettings>

using namespace Utils;
using namespace ProjectExplorer::Internal;

namespace ProjectExplorer {

constexpr char BUILD_KEY[] = "ProjectExplorer.RunConfiguration.BuildKey";

///////////////////////////////////////////////////////////////////////
//
// ISettingsAspect
//
///////////////////////////////////////////////////////////////////////

ISettingsAspect::ISettingsAspect() = default;

auto ISettingsAspect::createConfigWidget() const -> QWidget*
{
  QTC_ASSERT(m_configWidgetCreator, return nullptr);
  return m_configWidgetCreator();
}

auto ISettingsAspect::setConfigWidgetCreator(const ConfigWidgetCreator &configWidgetCreator) -> void
{
  m_configWidgetCreator = configWidgetCreator;
}

///////////////////////////////////////////////////////////////////////
//
// IRunConfigurationAspect
//
///////////////////////////////////////////////////////////////////////

GlobalOrProjectAspect::GlobalOrProjectAspect() = default;

GlobalOrProjectAspect::~GlobalOrProjectAspect()
{
  delete m_projectSettings;
}

auto GlobalOrProjectAspect::setProjectSettings(ISettingsAspect *settings) -> void
{
  m_projectSettings = settings;
}

auto GlobalOrProjectAspect::setGlobalSettings(ISettingsAspect *settings) -> void
{
  m_globalSettings = settings;
}

auto GlobalOrProjectAspect::setUsingGlobalSettings(bool value) -> void
{
  m_useGlobalSettings = value;
}

auto GlobalOrProjectAspect::currentSettings() const -> ISettingsAspect*
{
  return m_useGlobalSettings ? m_globalSettings : m_projectSettings;
}

auto GlobalOrProjectAspect::fromMap(const QVariantMap &map) -> void
{
  if (m_projectSettings)
    m_projectSettings->fromMap(map);
  m_useGlobalSettings = map.value(id().toString() + ".UseGlobalSettings", true).toBool();
}

auto GlobalOrProjectAspect::toMap(QVariantMap &map) const -> void
{
  if (m_projectSettings)
    m_projectSettings->toMap(map);
  map.insert(id().toString() + ".UseGlobalSettings", m_useGlobalSettings);
}

auto GlobalOrProjectAspect::toActiveMap(QVariantMap &data) const -> void
{
  if (m_useGlobalSettings)
    m_globalSettings->toMap(data);
  else if (m_projectSettings)
    m_projectSettings->toMap(data);
  // The debugger accesses the data directly, so this can actually happen.
  //else
  //    QTC_CHECK(false);
}

auto GlobalOrProjectAspect::resetProjectToGlobalSettings() -> void
{
  QTC_ASSERT(m_globalSettings, return);
  QVariantMap map;
  m_globalSettings->toMap(map);
  if (m_projectSettings)
    m_projectSettings->fromMap(map);
}

/*!
    \class ProjectExplorer::RunConfiguration
    \inmodule QtCreator
    \inheaderfile projectexplorer/runconfiguration.h

    \brief The RunConfiguration class is the base class for a run configuration.

    A run configuration specifies how a target should be run, while a runner
    does the actual running.

    The target owns the RunConfigurations and a RunControl will need to copy all
    necessary data as the RunControl may continue to exist after the RunConfiguration
    has been destroyed.

    A RunConfiguration disables itself if the project has no parsing
    data available. The disabledReason() method can be used to get a user-facing string
    describing why the RunConfiguration considers itself unfit for use.
*/

static std::vector<RunConfiguration::AspectFactory> theAspectFactories;

RunConfiguration::RunConfiguration(Target *target, Id id) : ProjectConfiguration(target, id)
{
  QTC_CHECK(target && target == this->target());
  connect(target, &Target::parsingFinished, this, &RunConfiguration::update);

  m_expander.setDisplayName(tr("Run Settings"));
  m_expander.setAccumulating(true);
  m_expander.registerSubProvider([target] {
    const auto bc = target->activeBuildConfiguration();
    return bc ? bc->macroExpander() : target->macroExpander();
  });
  m_expander.registerPrefix("RunConfig:Env", tr("Variables in the run environment."), [this](const QString &var) {
    const auto envAspect = aspect<EnvironmentAspect>();
    return envAspect ? envAspect->environment().expandedValueForKey(var) : QString();
  });
  m_expander.registerVariable("RunConfig:WorkingDir", tr("The run configuration's working directory."), [this] {
    const auto wdAspect = aspect<WorkingDirectoryAspect>();
    return wdAspect ? wdAspect->workingDirectory().toString() : QString();
  });
  m_expander.registerVariable("RunConfig:Name", tr("The run configuration's name."), [this] { return displayName(); });
  m_expander.registerFileVariables("RunConfig:Executable", tr("The run configuration's executable."), [this] { return commandLine().executable(); });

  m_commandLineGetter = [this] {
    FilePath executable;
    if (const auto executableAspect = aspect<ExecutableAspect>())
      executable = executableAspect->executable();
    QString arguments;
    if (const auto argumentsAspect = aspect<ArgumentsAspect>())
      arguments = argumentsAspect->arguments(macroExpander());
    return CommandLine{executable, arguments, CommandLine::Raw};
  };

  addPostInit([this] {
    if (const auto wdAspect = aspect<WorkingDirectoryAspect>())
      wdAspect->setMacroExpander(&m_expander);
  });
}

RunConfiguration::~RunConfiguration() = default;

auto RunConfiguration::disabledReason() const -> QString
{
  const auto bs = activeBuildSystem();
  return bs ? bs->disabledReason(m_buildKey) : tr("No build system active");
}

auto RunConfiguration::isEnabled() const -> bool
{
  const auto bs = activeBuildSystem();
  return bs && bs->hasParsingData();
}

auto RunConfiguration::createConfigurationWidget() -> QWidget*
{
  Layouting::Form builder;
  for (const auto aspect : qAsConst(m_aspects)) {
    if (aspect->isVisible())
      aspect->addToLayout(builder.finishRow());
  }

  const auto widget = builder.emerge(false);

  VariableChooser::addSupportForChildWidgets(widget, &m_expander);

  const auto detailsWidget = new DetailsWidget;
  detailsWidget->setState(DetailsWidget::NoSummary);
  detailsWidget->setWidget(widget);
  return detailsWidget;
}

auto RunConfiguration::isConfigured() const -> bool
{
  return !anyOf(checkForIssues(), [](const Task &t) { return t.type == Task::Error; });
}

auto RunConfiguration::addAspectFactory(const AspectFactory &aspectFactory) -> void
{
  theAspectFactories.push_back(aspectFactory);
}

auto RunConfiguration::aspectData() const -> QMap<Id, QVariantMap>
{
  QMap<Id, QVariantMap> data;
  for (const auto aspect : qAsConst(m_aspects))
    aspect->toActiveMap(data[aspect->id()]);
  return data;
}

auto RunConfiguration::activeBuildSystem() const -> BuildSystem*
{
  return target()->buildSystem();
}

auto RunConfiguration::setUpdater(const Updater &updater) -> void
{
  m_updater = updater;
}

auto RunConfiguration::createConfigurationIssue(const QString &description) const -> Task
{
  return BuildSystemTask(Task::Error, description);
}

auto RunConfiguration::toMap() const -> QVariantMap
{
  auto map = ProjectConfiguration::toMap();

  map.insert(BUILD_KEY, m_buildKey);

  // FIXME: Remove this id mangling, e.g. by using a separate entry for the build key.
  if (!m_buildKey.isEmpty()) {
    const auto mangled = id().withSuffix(m_buildKey);
    map.insert(settingsIdKey(), mangled.toSetting());
  }

  return map;
}

auto RunConfiguration::setCommandLineGetter(const CommandLineGetter &cmdGetter) -> void
{
  m_commandLineGetter = cmdGetter;
}

auto RunConfiguration::commandLine() const -> CommandLine
{
  return m_commandLineGetter();
}

auto RunConfiguration::setRunnableModifier(const RunnableModifier &runnableModifier) -> void
{
  m_runnableModifier = runnableModifier;
}

auto RunConfiguration::update() -> void
{
  if (m_updater)
    m_updater();

  emit enabledChanged();

  const auto isActive = target()->isActive() && target()->activeRunConfiguration() == this;

  if (isActive && project() == SessionManager::startupProject())
    ProjectExplorerPlugin::updateRunActions();
}

auto RunConfiguration::buildTargetInfo() const -> BuildTargetInfo
{
  const auto bs = target()->buildSystem();
  QTC_ASSERT(bs, return {});
  return bs->buildTarget(m_buildKey);
}

auto RunConfiguration::productNode() const -> ProjectNode*
{
  return project()->rootProjectNode()->findProjectNode([this](const ProjectNode *candidate) {
    return candidate->buildKey() == buildKey();
  });
}

auto RunConfiguration::fromMap(const QVariantMap &map) -> bool
{
  if (!ProjectConfiguration::fromMap(map))
    return false;

  m_buildKey = map.value(BUILD_KEY).toString();

  if (m_buildKey.isEmpty()) {
    const auto mangledId = Id::fromSetting(map.value(settingsIdKey()));
    m_buildKey = mangledId.suffixAfter(id());

    // Hack for cmake projects 4.10 -> 4.11.
    const QString magicSeparator = "///::///";
    const int magicIndex = m_buildKey.indexOf(magicSeparator);
    if (magicIndex != -1)
      m_buildKey = m_buildKey.mid(magicIndex + magicSeparator.length());
  }

  return true;
}

/*!
    \class ProjectExplorer::IRunConfigurationAspect

    \brief The IRunConfigurationAspect class provides an additional
    configuration aspect.

    Aspects are a mechanism to add RunControl-specific options to a run
    configuration without subclassing the run configuration for every addition.
    This prevents a combinatorial explosion of subclasses and eliminates
    the need to add all options to the base class.
*/

/*!
    \internal

    \class ProjectExplorer::Runnable

    \brief The ProjectExplorer::Runnable class wraps information needed
    to execute a process on a target device.

    A target specific \l RunConfiguration implementation can specify
    what information it considers necessary to execute a process
    on the target. Target specific) \n RunWorker implementation
    can use that information either unmodified or tweak it or ignore
    it when setting up a RunControl.

    From Qt Creator's core perspective a Runnable object is opaque.
*/

/*!
    \internal

    \brief Returns a \l Runnable described by this RunConfiguration.
*/

auto RunConfiguration::runnable() const -> Runnable
{
  Runnable r;
  r.command = commandLine();
  if (const auto workingDirectoryAspect = aspect<WorkingDirectoryAspect>())
    r.workingDirectory = workingDirectoryAspect->workingDirectory();
  if (const auto environmentAspect = aspect<EnvironmentAspect>())
    r.environment = environmentAspect->environment();
  if (m_runnableModifier)
    m_runnableModifier(r);
  return r;
}

/*!
    \class ProjectExplorer::RunConfigurationFactory
    \inmodule QtCreator
    \inheaderfile projectexplorer/runconfiguration.h

    \brief The RunConfigurationFactory class is used to create and persist
    run configurations.

    The run configuration factory is used for restoring run configurations from
    settings and for creating new run configurations in the \gui {Run Settings}
    dialog.

    A RunConfigurationFactory instance is responsible for handling one type of
    run configurations. This can be restricted to certain project and device
    types.

    RunConfigurationFactory instances register themselves into a global list on
    construction and deregister on destruction. It is recommended to make them
    a plain data member of a structure that is allocated in your plugin's
    ExtensionSystem::IPlugin::initialize() method.
*/

static QList<RunConfigurationFactory*> g_runConfigurationFactories;

/*!
    Constructs a RunConfigurationFactory instance and registers it into a global
    list.

    Derived classes should set suitably properties to specify the type of
    run configurations they can handle.
*/

RunConfigurationFactory::RunConfigurationFactory()
{
  g_runConfigurationFactories.append(this);
}

/*!
    De-registers the instance from the global list of factories and destructs it.
*/

RunConfigurationFactory::~RunConfigurationFactory()
{
  g_runConfigurationFactories.removeOne(this);
}

auto RunConfigurationFactory::decoratedTargetName(const QString &targetName, Target *target) -> QString
{
  auto displayName = targetName;
  const auto devType = DeviceTypeKitAspect::deviceTypeId(target->kit());
  if (devType != Constants::DESKTOP_DEVICE_TYPE) {
    if (auto dev = DeviceKitAspect::device(target->kit())) {
      if (displayName.isEmpty()) {
        //: Shown in Run configuration if no executable is given, %1 is device name
        displayName = RunConfiguration::tr("Run on %{Device:Name}");
      } else {
        //: Shown in Run configuration, Add menu: "name of runnable (on device name)"
        displayName = RunConfiguration::tr("%1 (on %{Device:Name})").arg(displayName);
      }
    }
  }
  return displayName;
}

auto RunConfigurationFactory::availableCreators(Target *target) const -> QList<RunConfigurationCreationInfo>
{
  const auto buildTargets = target->buildSystem()->applicationTargets();
  const auto hasAnyQtcRunnable = anyOf(buildTargets, equal(&BuildTargetInfo::isQtcRunnable, true));
  return transform(buildTargets, [&](const BuildTargetInfo &ti) {
    auto displayName = ti.displayName;
    if (displayName.isEmpty())
      displayName = decoratedTargetName(ti.buildKey, target);
    else if (m_decorateDisplayNames)
      displayName = decoratedTargetName(displayName, target);
    RunConfigurationCreationInfo rci;
    rci.factory = this;
    rci.buildKey = ti.buildKey;
    rci.projectFilePath = ti.projectFilePath;
    rci.displayName = displayName;
    rci.displayNameUniquifier = ti.displayNameUniquifier;
    rci.creationMode = ti.isQtcRunnable || !hasAnyQtcRunnable ? RunConfigurationCreationInfo::AlwaysCreate : RunConfigurationCreationInfo::ManualCreationOnly;
    rci.useTerminal = ti.usesTerminal;
    rci.buildKey = ti.buildKey;
    return rci;
  });
}

/*!
    Adds a device type for which this RunConfigurationFactory
    can create RunConfigurations.

    If this function is never called for a RunConfigurationFactory,
    the factory will create RunConfiguration objects for all device types.

    This function should be used in the constructor of derived classes.

    \sa addSupportedProjectType()
*/

auto RunConfigurationFactory::addSupportedTargetDeviceType(Id id) -> void
{
  m_supportedTargetDeviceTypes.append(id);
}

auto RunConfigurationFactory::setDecorateDisplayNames(bool on) -> void
{
  m_decorateDisplayNames = on;
}

/*!
    Adds a project type for which this RunConfigurationFactory
    can create RunConfigurations.

    If this function is never called for a RunConfigurationFactory,
    the factory will create RunConfigurations for all project types.

    This function should be used in the constructor of derived classes.

    \sa addSupportedTargetDeviceType()
*/

auto RunConfigurationFactory::addSupportedProjectType(Id id) -> void
{
  m_supportedProjectTypes.append(id);
}

auto RunConfigurationFactory::canHandle(Target *target) const -> bool
{
  const Project *project = target->project();
  const auto kit = target->kit();

  if (containsType(target->project()->projectIssues(kit), Task::TaskType::Error))
    return false;

  if (!m_supportedProjectTypes.isEmpty())
    if (!m_supportedProjectTypes.contains(project->id()))
      return false;

  if (!m_supportedTargetDeviceTypes.isEmpty())
    if (!m_supportedTargetDeviceTypes.contains(DeviceTypeKitAspect::deviceTypeId(kit)))
      return false;

  return true;
}

auto RunConfigurationFactory::create(Target *target) const -> RunConfiguration*
{
  QTC_ASSERT(m_creator, return nullptr);
  const auto rc = m_creator(target);
  QTC_ASSERT(rc, return nullptr);

  // Add the universal aspects.
  for (const auto &factory : theAspectFactories)
    rc->m_aspects.registerAspect(factory(target));

  rc->acquaintAspects();
  rc->doPostInit();
  return rc;
}

auto RunConfigurationCreationInfo::create(Target *target) const -> RunConfiguration*
{
  QTC_ASSERT(factory->canHandle(target), return nullptr);

  const auto rc = factory->create(target);
  if (!rc)
    return nullptr;

  rc->m_buildKey = buildKey;
  rc->update();
  rc->setDisplayName(displayName);

  return rc;
}

auto RunConfigurationFactory::restore(Target *parent, const QVariantMap &map) -> RunConfiguration*
{
  for (const auto factory : qAsConst(g_runConfigurationFactories)) {
    if (factory->canHandle(parent)) {
      const auto id = idFromMap(map);
      if (id.name().startsWith(factory->m_runConfigurationId.name())) {
        const auto rc = factory->create(parent);
        if (rc->fromMap(map)) {
          rc->update();
          return rc;
        }
        delete rc;
        return nullptr;
      }
    }
  }
  return nullptr;
}

auto RunConfigurationFactory::clone(Target *parent, RunConfiguration *source) -> RunConfiguration*
{
  return restore(parent, source->toMap());
}

auto RunConfigurationFactory::creatorsForTarget(Target *parent) -> const QList<RunConfigurationCreationInfo>
{
  QList<RunConfigurationCreationInfo> items;
  for (const auto factory : qAsConst(g_runConfigurationFactories)) {
    if (factory->canHandle(parent))
      items.append(factory->availableCreators(parent));
  }
  QHash<QString, QList<RunConfigurationCreationInfo*>> itemsPerDisplayName;
  for (auto &item : items)
    itemsPerDisplayName[item.displayName] << &item;
  for (auto it = itemsPerDisplayName.cbegin(); it != itemsPerDisplayName.cend(); ++it) {
    if (it.value().size() == 1)
      continue;
    for (const auto rci : it.value())
      rci->displayName += rci->displayNameUniquifier;
  }
  return items;
}

FixedRunConfigurationFactory::FixedRunConfigurationFactory(const QString &displayName, bool addDeviceName) : m_fixedBuildTarget(displayName), m_decorateTargetName(addDeviceName) { }

auto FixedRunConfigurationFactory::availableCreators(Target *parent) const -> QList<RunConfigurationCreationInfo>
{
  const auto displayName = m_decorateTargetName ? decoratedTargetName(m_fixedBuildTarget, parent) : m_fixedBuildTarget;
  RunConfigurationCreationInfo rci;
  rci.factory = this;
  rci.displayName = displayName;
  return {rci};
}

} // namespace ProjectExplorer
