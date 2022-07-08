// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildstep.hpp"

#include "buildconfiguration.hpp"
#include "buildsteplist.hpp"
#include "customparser.hpp"
#include "deployconfiguration.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorerconstants.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileinprojectfinder.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>
#include <utils/variablechooser.hpp>

#include <QFormLayout>
#include <QFutureWatcher>
#include <QPointer>

/*!
    \class ProjectExplorer::BuildStep

    \brief The BuildStep class provides build steps for projects.

    Build steps are the primary way plugin developers can customize
    how their projects (or projects from other plugins) are built.

    Projects are built by taking the list of build steps
    from the project and calling first \c init() and then \c run() on them.

    To change the way your project is built, reimplement
    this class and add your build step to the build step list of the project.

    \note The projects own the build step. Do not delete them yourself.

    \c init() is called in the GUI thread and can be used to query the
    project for any information you need.

    \c run() is run via Utils::runAsync in a separate thread. If you need an
    event loop, you need to create it yourself.
*/

/*!
    \fn bool ProjectExplorer::BuildStep::init()

    This function is run in the GUI thread. Use it to retrieve any information
    that you need in the run() function.
*/

/*!
    \fn void ProjectExplorer::BuildStep::run(QFutureInterface<bool> &fi)

    Reimplement this function. It is called when the target is built.
    By default, this function is NOT run in the GUI thread, but runs in its
    own thread. If you need an event loop, you need to create one.
    This function should block until the task is done

    The absolute minimal implementation is:
    \code
    fi.reportResult(true);
    \endcode

    By returning \c true from runInGuiThread(), this function is called in
    the GUI thread. Then the function should not block and instead the
    finished() signal should be emitted.

    \sa runInGuiThread()
*/

/*!
    \fn BuildStepConfigWidget *ProjectExplorer::BuildStep::createConfigWidget()

    Returns the Widget shown in the target settings dialog for this build step.
    Ownership is transferred to the caller.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::addTask(const ProjectExplorer::Task &task)
    Adds \a task.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::addOutput(const QString &string, ProjectExplorer::BuildStep::OutputFormat format,
              ProjectExplorer::BuildStep::OutputNewlineSetting newlineSetting = DoAppendNewline) const

    The \a string is added to the generated output, usually in the output pane.
    It should be in plain text, with the format in the parameter.
*/

/*!
    \fn  void ProjectExplorer::BuildStep::finished()
    This signal needs to be emitted if the build step runs in the GUI thread.
*/

using namespace Utils;

static const char buildStepEnabledKey[] = "ProjectExplorer.BuildStep.Enabled";

namespace ProjectExplorer {

static QList<BuildStepFactory *> g_buildStepFactories;

BuildStep::BuildStep(BuildStepList *bsl, Id id) : ProjectConfiguration(bsl, id)
{
  QTC_CHECK(bsl->target() && bsl->target() == this->target());
  connect(this, &ProjectConfiguration::displayNameChanged, this, &BuildStep::updateSummary);
  //    m_displayName = step->displayName();
  //    m_summaryText = "<b>" + m_displayName + "</b>";
}

BuildStep::~BuildStep()
{
  emit finished(false);
}

auto BuildStep::run() -> void
{
  m_cancelFlag = false;
  doRun();
}

auto BuildStep::cancel() -> void
{
  m_cancelFlag = true;
  doCancel();
}

auto BuildStep::doCreateConfigWidget() -> QWidget*
{
  const auto widget = createConfigWidget();

  const auto recreateSummary = [this] {
    if (m_summaryUpdater)
      setSummaryText(m_summaryUpdater());
  };

  for (const auto aspect : qAsConst(m_aspects))
    connect(aspect, &BaseAspect::changed, widget, recreateSummary);

  connect(buildConfiguration(), &BuildConfiguration::buildDirectoryChanged, widget, recreateSummary);

  recreateSummary();

  return widget;
}

auto BuildStep::createConfigWidget() -> QWidget*
{
  Layouting::Form builder;
  for (const auto aspect : qAsConst(m_aspects)) {
    if (aspect->isVisible())
      aspect->addToLayout(builder.finishRow());
  }
  const auto widget = builder.emerge(false);

  if (m_addMacroExpander)
    VariableChooser::addSupportForChildWidgets(widget, macroExpander());

  return widget;
}

auto BuildStep::fromMap(const QVariantMap &map) -> bool
{
  m_enabled = map.value(buildStepEnabledKey, true).toBool();
  return ProjectConfiguration::fromMap(map);
}

auto BuildStep::toMap() const -> QVariantMap
{
  auto map = ProjectConfiguration::toMap();
  map.insert(buildStepEnabledKey, m_enabled);
  return map;
}

auto BuildStep::buildConfiguration() const -> BuildConfiguration*
{
  const auto config = qobject_cast<BuildConfiguration*>(parent()->parent());
  if (config)
    return config;

  // step is not part of a build configuration, use active build configuration of step's target
  return target()->activeBuildConfiguration();
}

auto BuildStep::deployConfiguration() const -> DeployConfiguration*
{
  const auto config = qobject_cast<DeployConfiguration*>(parent()->parent());
  if (config)
    return config;
  // See comment in buildConfiguration()
  QTC_CHECK(false);
  // step is not part of a deploy configuration, use active deploy configuration of step's target
  return target()->activeDeployConfiguration();
}

auto BuildStep::projectConfiguration() const -> ProjectConfiguration*
{
  return static_cast<ProjectConfiguration*>(parent()->parent());
}

auto BuildStep::buildSystem() const -> BuildSystem*
{
  if (const auto bc = buildConfiguration())
    return bc->buildSystem();
  return target()->buildSystem();
}

auto BuildStep::buildEnvironment() const -> Environment
{
  if (const auto bc = qobject_cast<BuildConfiguration*>(parent()->parent()))
    return bc->environment();
  if (const auto bc = target()->activeBuildConfiguration())
    return bc->environment();
  return Environment::systemEnvironment();
}

auto BuildStep::buildDirectory() const -> FilePath
{
  if (const auto bc = buildConfiguration())
    return bc->buildDirectory();
  return {};
}

auto BuildStep::buildType() const -> BuildConfiguration::BuildType
{
  if (const auto bc = buildConfiguration())
    return bc->buildType();
  return BuildConfiguration::Unknown;
}

auto BuildStep::macroExpander() const -> MacroExpander*
{
  if (const auto bc = buildConfiguration())
    return bc->macroExpander();
  return globalMacroExpander();
}

auto BuildStep::fallbackWorkingDirectory() const -> QString
{
  if (buildConfiguration())
    return {Constants::DEFAULT_WORKING_DIR};
  return {Constants::DEFAULT_WORKING_DIR_ALTERNATE};
}

auto BuildStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  if (qobject_cast<BuildConfiguration*>(parent()->parent())) {
    for (const auto id : buildConfiguration()->customParsers()) {
      if (const auto parser = Internal::CustomParser::createFromId(id))
        formatter->addLineParser(parser);
    }

    formatter->setForwardStdOutToStdError(buildConfiguration()->parseStdOut());
  }
  FileInProjectFinder fileFinder;
  fileFinder.setProjectDirectory(project()->projectDirectory());
  fileFinder.setProjectFiles(project()->files(Project::AllFiles));
  formatter->setFileFinder(fileFinder);
}

auto BuildStep::reportRunResult(QFutureInterface<bool> &fi, bool success) -> void
{
  fi.reportResult(success);
  fi.reportFinished();
}

auto BuildStep::widgetExpandedByDefault() const -> bool
{
  return m_widgetExpandedByDefault;
}

auto BuildStep::setWidgetExpandedByDefault(bool widgetExpandedByDefault) -> void
{
  m_widgetExpandedByDefault = widgetExpandedByDefault;
}

auto BuildStep::data(Id id) const -> QVariant
{
  Q_UNUSED(id)
  return {};
}

/*!
  \fn BuildStep::isImmutable()

    If this function returns \c true, the user cannot delete this build step for
    this target and the user is prevented from changing the order in which
    immutable steps are run. The default implementation returns \c false.
*/

auto BuildStep::runInThread(const std::function<bool()> &syncImpl) -> void
{
  m_runInGuiThread = false;
  m_cancelFlag = false;
  auto *const watcher = new QFutureWatcher<bool>(this);
  connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher] {
    emit finished(watcher->result());
    watcher->deleteLater();
  });
  watcher->setFuture(runAsync(syncImpl));
}

auto BuildStep::cancelChecker() const -> std::function<bool ()>
{
  return [step = QPointer<const BuildStep>(this)] { return step && step->isCanceled(); };
}

auto BuildStep::isCanceled() const -> bool
{
  return m_cancelFlag;
}

auto BuildStep::doCancel() -> void
{
  QTC_ASSERT(!m_runInGuiThread, qWarning() << "Build step" << displayName() << "neeeds to implement the doCancel() function");
}

auto BuildStep::addMacroExpander() -> void
{
  m_addMacroExpander = true;
}

auto BuildStep::setEnabled(bool b) -> void
{
  if (m_enabled == b)
    return;
  m_enabled = b;
  emit enabledChanged();
}

auto BuildStep::stepList() const -> BuildStepList*
{
  return qobject_cast<BuildStepList*>(parent());
}

auto BuildStep::enabled() const -> bool
{
  return m_enabled;
}

BuildStepFactory::BuildStepFactory()
{
  g_buildStepFactories.append(this);
}

BuildStepFactory::~BuildStepFactory()
{
  g_buildStepFactories.removeOne(this);
}

auto BuildStepFactory::allBuildStepFactories() -> const QList<BuildStepFactory*>
{
  return g_buildStepFactories;
}

auto BuildStepFactory::canHandle(BuildStepList *bsl) const -> bool
{
  if (!m_supportedStepLists.isEmpty() && !m_supportedStepLists.contains(bsl->id()))
    return false;

  const auto config = qobject_cast<ProjectConfiguration*>(bsl->parent());

  if (!m_supportedDeviceTypes.isEmpty()) {
    const auto target = bsl->target();
    QTC_ASSERT(target, return false);
    const auto deviceType = DeviceTypeKitAspect::deviceTypeId(target->kit());
    if (!m_supportedDeviceTypes.contains(deviceType))
      return false;
  }

  if (m_supportedProjectType.isValid()) {
    if (!config)
      return false;
    const auto projectId = config->project()->id();
    if (projectId != m_supportedProjectType)
      return false;
  }

  if (!m_isRepeatable && bsl->contains(m_info.id))
    return false;

  if (m_supportedConfiguration.isValid()) {
    if (!config)
      return false;
    const auto configId = config->id();
    if (configId != m_supportedConfiguration)
      return false;
  }

  return true;
}

auto BuildStepFactory::setDisplayName(const QString &displayName) -> void
{
  m_info.displayName = displayName;
}

auto BuildStepFactory::setFlags(BuildStepInfo::Flags flags) -> void
{
  m_info.flags = flags;
}

auto BuildStepFactory::setSupportedStepList(Id id) -> void
{
  m_supportedStepLists = {id};
}

auto BuildStepFactory::setSupportedStepLists(const QList<Id> &ids) -> void
{
  m_supportedStepLists = ids;
}

auto BuildStepFactory::setSupportedConfiguration(Id id) -> void
{
  m_supportedConfiguration = id;
}

auto BuildStepFactory::setSupportedProjectType(Id id) -> void
{
  m_supportedProjectType = id;
}

auto BuildStepFactory::setSupportedDeviceType(Id id) -> void
{
  m_supportedDeviceTypes = {id};
}

auto BuildStepFactory::setSupportedDeviceTypes(const QList<Id> &ids) -> void
{
  m_supportedDeviceTypes = ids;
}

auto BuildStepFactory::stepInfo() const -> BuildStepInfo
{
  return m_info;
}

auto BuildStepFactory::stepId() const -> Id
{
  return m_info.id;
}

auto BuildStepFactory::create(BuildStepList *parent) -> BuildStep*
{
  const auto step = m_info.creator(parent);
  step->setDefaultDisplayName(m_info.displayName);
  return step;
}

auto BuildStepFactory::restore(BuildStepList *parent, const QVariantMap &map) -> BuildStep*
{
  const auto bs = create(parent);
  if (!bs)
    return nullptr;
  if (!bs->fromMap(map)) {
    QTC_CHECK(false);
    delete bs;
    return nullptr;
  }
  return bs;
}

auto BuildStep::summaryText() const -> QString
{
  if (m_summaryText.isEmpty())
    return QString("<b>%1</b>").arg(displayName());

  return m_summaryText;
}

auto BuildStep::setSummaryText(const QString &summaryText) -> void
{
  if (summaryText != m_summaryText) {
    m_summaryText = summaryText;
    emit updateSummary();
  }
}

auto BuildStep::setSummaryUpdater(const std::function<QString()> &summaryUpdater) -> void
{
  m_summaryUpdater = summaryUpdater;
}

} // ProjectExplorer
