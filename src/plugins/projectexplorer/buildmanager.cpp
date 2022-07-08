// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildmanager.hpp"

#include "buildprogress.hpp"
#include "buildsteplist.hpp"
#include "buildsystem.hpp"
#include "compileoutputwindow.hpp"
#include "deployconfiguration.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorersettings.hpp"
#include "runcontrol.hpp"
#include "session.hpp"
#include "target.hpp"
#include "task.hpp"
#include "taskhub.hpp"
#include "taskwindow.hpp"
#include "waitforstopdialog.hpp"

#include <core/icore.hpp>
#include <core/progressmanager/futureprogress.hpp>
#include <core/progressmanager/progressmanager.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <utils/algorithm.hpp>
#include <utils/outputformatter.hpp>
#include <utils/runextensions.hpp>
#include <utils/stringutils.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QTime>
#include <QTimer>

using namespace Core;
using namespace Utils;

namespace ProjectExplorer {
using namespace Internal;

static auto msgProgress(int progress, int total) -> QString
{
  return BuildManager::tr("Finished %1 of %n steps", nullptr, total).arg(progress);
}

static auto targetsForSelection(const Project *project, ConfigSelection targetSelection) -> const QList<Target*>
{
  if (targetSelection == ConfigSelection::All)
    return project->targets();
  return {project->activeTarget()};
}

static auto buildConfigsForSelection(const Target *target, ConfigSelection configSelection) -> const QList<BuildConfiguration*>
{
  if (configSelection == ConfigSelection::All)
    return target->buildConfigurations();
  else if (target->activeBuildConfiguration())
    return {target->activeBuildConfiguration()};
  return {};
}

static auto queue(const QList<Project*> &projects, const QList<Id> &stepIds, ConfigSelection configSelection, const RunConfiguration *forRunConfig = nullptr) -> int
{
  if (!ProjectExplorerPlugin::saveModifiedFiles())
    return -1;

  const auto &settings = ProjectExplorerPlugin::projectExplorerSettings();
  if (settings.stopBeforeBuild != StopBeforeBuild::None && stepIds.contains(Constants::BUILDSTEPS_BUILD)) {
    auto stopCondition = settings.stopBeforeBuild;
    if (stopCondition == StopBeforeBuild::SameApp && !forRunConfig)
      stopCondition = StopBeforeBuild::SameBuildDir;
    const auto isStoppableRc = [&projects, stopCondition, configSelection, forRunConfig](RunControl *rc) {
      if (!rc->isRunning())
        return false;

      switch (stopCondition) {
      case StopBeforeBuild::None:
        return false;
      case StopBeforeBuild::All:
        return true;
      case StopBeforeBuild::SameProject:
        return projects.contains(rc->project());
      case StopBeforeBuild::SameBuildDir:
        return contains(projects, [rc, configSelection](Project *p) {
          auto device = rc->runnable().device;
          for (const Target *const t : targetsForSelection(p, configSelection)) {
            if (device.isNull())
              device = DeviceKitAspect::device(t->kit());
            if (device.isNull() || device->type() != Constants::DESKTOP_DEVICE_TYPE)
              continue;
            for (const BuildConfiguration *const bc : buildConfigsForSelection(t, configSelection)) {
              if (rc->runnable().command.executable().isChildOf(bc->buildDirectory()))
                return true;
            }
          }
          return false;
        });
      case StopBeforeBuild::SameApp: QTC_ASSERT(forRunConfig, return false);
        return forRunConfig->buildTargetInfo().targetFilePath == rc->targetFilePath();
      }
      return false; // Can't get here!
    };
    const auto toStop = filtered(ProjectExplorerPlugin::allRunControls(), isStoppableRc);

    if (!toStop.isEmpty()) {
      auto stopThem = true;
      if (settings.prompToStopRunControl) {
        auto names = transform(toStop, &RunControl::displayName);
        if (QMessageBox::question(ICore::dialogParent(), BuildManager::tr("Stop Applications"), BuildManager::tr("Stop these applications before building?") + "\n\n" + names.join('\n')) == QMessageBox::No) {
          stopThem = false;
        }
      }

      if (stopThem) {
        foreach(RunControl *rc, toStop)
          rc->initiateStop();

        WaitForStopDialog dialog(toStop);
        dialog.exec();

        if (dialog.canceled())
          return -1;
      }
    }
  }

  QList<BuildStepList*> stepLists;
  QStringList preambleMessage;

  foreach(Project *pro, projects) {
    if (pro && pro->needsConfiguration()) {
      preambleMessage.append(BuildManager::tr("The project %1 is not configured, skipping it.").arg(pro->displayName()) + QLatin1Char('\n'));
    }
  }
  foreach(Id id, stepIds) {
    const auto isBuild = id == Constants::BUILDSTEPS_BUILD;
    const auto isClean = id == Constants::BUILDSTEPS_CLEAN;
    const auto isDeploy = id == Constants::BUILDSTEPS_DEPLOY;
    foreach(Project *pro, projects) {
      if (!pro || pro->needsConfiguration())
        continue;
      BuildStepList *bsl = nullptr;
      for (const Target *target : targetsForSelection(pro, configSelection)) {
        if (isBuild || isClean) {
          for (const BuildConfiguration *const bc : buildConfigsForSelection(target, configSelection)) {
            bsl = isBuild ? bc->buildSteps() : bc->cleanSteps();
            if (bsl && !bsl->isEmpty())
              stepLists << bsl;
          }
          continue;
        }
        if (isDeploy && target->activeDeployConfiguration())
          bsl = target->activeDeployConfiguration()->stepList();
        if (bsl && !bsl->isEmpty())
          stepLists << bsl;
      }
    }
  }

  if (stepLists.isEmpty())
    return 0;

  if (!BuildManager::buildLists(stepLists, preambleMessage))
    return -1;
  return stepLists.count();
}

class BuildManagerPrivate {
public:
  CompileOutputWindow *m_outputWindow = nullptr;
  TaskWindow *m_taskWindow = nullptr;

  QMetaObject::Connection m_scheduledBuild;
  QList<BuildStep*> m_buildQueue;
  QList<bool> m_enabledState;
  QStringList m_stepNames;
  int m_progress = 0;
  int m_maxProgress = 0;
  bool m_running = false;
  bool m_isDeploying = false;
  // is set to true while canceling, so that nextBuildStep knows that the BuildStep finished because of canceling
  bool m_skipDisabled = false;
  bool m_canceling = false;
  bool m_lastStepSucceeded = true;
  bool m_allStepsSucceeded = true;
  BuildStep *m_currentBuildStep = nullptr;
  QString m_currentConfiguration;
  // used to decide if we are building a project to decide when to emit buildStateChanged(Project *)
  QHash<Project*, int> m_activeBuildSteps;
  QHash<Target*, int> m_activeBuildStepsPerTarget;
  QHash<ProjectConfiguration*, int> m_activeBuildStepsPerProjectConfiguration;
  Project *m_previousBuildStepProject = nullptr;

  // Progress reporting to the progress manager
  QFutureInterface<void> *m_progressFutureInterface = nullptr;
  QFutureWatcher<void> m_progressWatcher;
  QPointer<FutureProgress> m_futureProgress;

  QElapsedTimer m_elapsed;
};

static BuildManagerPrivate *d = nullptr;
static BuildManager *m_instance = nullptr;

BuildManager::BuildManager(QObject *parent, QAction *cancelBuildAction) : QObject(parent)
{
  QTC_CHECK(!m_instance);
  m_instance = this;
  d = new BuildManagerPrivate;

  connect(SessionManager::instance(), &SessionManager::aboutToRemoveProject, this, &BuildManager::aboutToRemoveProject);

  d->m_outputWindow = new CompileOutputWindow(cancelBuildAction);
  ExtensionSystem::PluginManager::addObject(d->m_outputWindow);

  d->m_taskWindow = new TaskWindow;
  ExtensionSystem::PluginManager::addObject(d->m_taskWindow);

  qRegisterMetaType<BuildStep::OutputFormat>();
  qRegisterMetaType<BuildStep::OutputNewlineSetting>();

  connect(d->m_taskWindow, &TaskWindow::tasksChanged, this, &BuildManager::updateTaskCount);

  connect(&d->m_progressWatcher, &QFutureWatcherBase::canceled, this, &BuildManager::cancel);
  connect(&d->m_progressWatcher, &QFutureWatcherBase::finished, this, &BuildManager::finish);
}

auto BuildManager::instance() -> BuildManager*
{
  return m_instance;
}

auto BuildManager::extensionsInitialized() -> void
{
  TaskHub::addCategory(Constants::TASK_CATEGORY_COMPILE, tr("Compile", "Category for compiler issues listed under 'Issues'"), true, 100);
  TaskHub::addCategory(Constants::TASK_CATEGORY_BUILDSYSTEM, tr("Build System", "Category for build system issues listed under 'Issues'"), true, 100);
  TaskHub::addCategory(Constants::TASK_CATEGORY_DEPLOYMENT, tr("Deployment", "Category for deployment issues listed under 'Issues'"), true, 100);
  TaskHub::addCategory(Constants::TASK_CATEGORY_AUTOTEST, tr("Autotests", "Category for autotest issues listed under 'Issues'"), true, 100);
}

auto BuildManager::buildProjectWithoutDependencies(Project *project) -> void
{
  queue({project}, {Id(Constants::BUILDSTEPS_BUILD)}, ConfigSelection::Active);
}

auto BuildManager::cleanProjectWithoutDependencies(Project *project) -> void
{
  queue({project}, {Id(Constants::BUILDSTEPS_CLEAN)}, ConfigSelection::Active);
}

auto BuildManager::rebuildProjectWithoutDependencies(Project *project) -> void
{
  queue({project}, {Id(Constants::BUILDSTEPS_CLEAN), Id(Constants::BUILDSTEPS_BUILD)}, ConfigSelection::Active);
}

auto BuildManager::buildProjectWithDependencies(Project *project, ConfigSelection configSelection) -> void
{
  queue(SessionManager::projectOrder(project), {Id(Constants::BUILDSTEPS_BUILD)}, configSelection);
}

auto BuildManager::cleanProjectWithDependencies(Project *project, ConfigSelection configSelection) -> void
{
  queue(SessionManager::projectOrder(project), {Id(Constants::BUILDSTEPS_CLEAN)}, configSelection);
}

auto BuildManager::rebuildProjectWithDependencies(Project *project, ConfigSelection configSelection) -> void
{
  queue(SessionManager::projectOrder(project), {Id(Constants::BUILDSTEPS_CLEAN), Id(Constants::BUILDSTEPS_BUILD)}, configSelection);
}

auto BuildManager::buildProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void
{
  queue(projects, {Id(Constants::BUILDSTEPS_BUILD)}, configSelection);
}

auto BuildManager::cleanProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void
{
  queue(projects, {Id(Constants::BUILDSTEPS_CLEAN)}, configSelection);
}

auto BuildManager::rebuildProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void
{
  queue(projects, {Id(Constants::BUILDSTEPS_CLEAN), Id(Constants::BUILDSTEPS_BUILD)}, configSelection);
}

auto BuildManager::deployProjects(const QList<Project*> &projects) -> void
{
  QList<Id> steps;
  if (ProjectExplorerPlugin::projectExplorerSettings().buildBeforeDeploy != BuildBeforeRunMode::Off)
    steps << Id(Constants::BUILDSTEPS_BUILD);
  steps << Id(Constants::BUILDSTEPS_DEPLOY);
  queue(projects, steps, ConfigSelection::Active);
}

auto BuildManager::potentiallyBuildForRunConfig(RunConfiguration *rc) -> BuildForRunConfigStatus
{
  QList<Id> stepIds;
  const auto &settings = ProjectExplorerPlugin::projectExplorerSettings();
  if (settings.deployBeforeRun) {
    if (!isBuilding()) {
      switch (settings.buildBeforeDeploy) {
      case BuildBeforeRunMode::AppOnly:
        if (rc->target()->activeBuildConfiguration())
          rc->target()->activeBuildConfiguration()->restrictNextBuild(rc);
        Q_FALLTHROUGH();
      case BuildBeforeRunMode::WholeProject:
        stepIds << Id(Constants::BUILDSTEPS_BUILD);
        break;
      case BuildBeforeRunMode::Off:
        break;
      }
    }
    if (!isDeploying())
      stepIds << Id(Constants::BUILDSTEPS_DEPLOY);
  }

  const auto pro = rc->target()->project();
  const auto queueCount = queue(SessionManager::projectOrder(pro), stepIds, ConfigSelection::Active, rc);
  if (rc->target()->activeBuildConfiguration())
    rc->target()->activeBuildConfiguration()->restrictNextBuild(nullptr);

  if (queueCount < 0)
    return BuildForRunConfigStatus::BuildFailed;
  if (queueCount > 0 || isBuilding(rc->project()))
    return BuildForRunConfigStatus::Building;
  return BuildForRunConfigStatus::NotBuilding;
}

BuildManager::~BuildManager()
{
  cancel();
  m_instance = nullptr;
  ExtensionSystem::PluginManager::removeObject(d->m_taskWindow);
  delete d->m_taskWindow;

  ExtensionSystem::PluginManager::removeObject(d->m_outputWindow);
  delete d->m_outputWindow;

  delete d;
  d = nullptr;
}

auto BuildManager::aboutToRemoveProject(Project *p) -> void
{
  const auto it = d->m_activeBuildSteps.find(p);
  const auto end = d->m_activeBuildSteps.end();
  if (it != end && *it > 0) {
    // We are building the project that's about to be removed.
    // We cancel the whole queue, which isn't the nicest thing to do
    // but a safe thing.
    cancel();
  }
}

auto BuildManager::isBuilding() -> bool
{
  // we are building even if we are not running yet
  return !d->m_buildQueue.isEmpty() || d->m_running;
}

auto BuildManager::isDeploying() -> bool
{
  return d->m_isDeploying;
}

auto BuildManager::getErrorTaskCount() -> int
{
  const auto errors = d->m_taskWindow->errorTaskCount(Constants::TASK_CATEGORY_BUILDSYSTEM) + d->m_taskWindow->errorTaskCount(Constants::TASK_CATEGORY_COMPILE) + d->m_taskWindow->errorTaskCount(Constants::TASK_CATEGORY_DEPLOYMENT);
  return errors;
}

auto BuildManager::setCompileOutputSettings(const CompileOutputSettings &settings) -> void
{
  d->m_outputWindow->setSettings(settings);
}

auto BuildManager::compileOutputSettings() -> const CompileOutputSettings&
{
  return d->m_outputWindow->settings();
}

auto BuildManager::displayNameForStepId(Id stepId) -> QString
{
  if (stepId == Constants::BUILDSTEPS_CLEAN) {
    //: Displayed name for a "cleaning" build step
    return tr("Clean");
  }
  if (stepId == Constants::BUILDSTEPS_DEPLOY) {
    //: Displayed name for a deploy step
    return tr("Deploy");
  }
  //: Displayed name for a normal build step
  return tr("Build");
}

auto BuildManager::cancel() -> void
{
  if (d->m_scheduledBuild) {
    disconnect(d->m_scheduledBuild);
    d->m_scheduledBuild = {};
    clearBuildQueue();
    return;
  }
  if (d->m_running) {
    if (d->m_canceling)
      return;
    d->m_canceling = true;
    d->m_currentBuildStep->cancel();
  }
}

auto BuildManager::updateTaskCount() -> void
{
  const auto errors = getErrorTaskCount();
  ProgressManager::setApplicationLabel(errors > 0 ? QString::number(errors) : QString());
}

auto BuildManager::finish() -> void
{
  const auto elapsedTime = formatElapsedTime(d->m_elapsed.elapsed());
  addToOutputWindow(elapsedTime, BuildStep::OutputFormat::NormalMessage);
  d->m_outputWindow->flush();

  QApplication::alert(ICore::dialogParent(), 3000);
}

auto BuildManager::emitCancelMessage() -> void
{
  addToOutputWindow(tr("Canceled build/deployment."), BuildStep::OutputFormat::ErrorMessage);
}

auto BuildManager::clearBuildQueue() -> void
{
  for (const auto bs : qAsConst(d->m_buildQueue)) {
    decrementActiveBuildSteps(bs);
    disconnectOutput(bs);
  }

  d->m_stepNames.clear();
  d->m_buildQueue.clear();
  d->m_enabledState.clear();
  d->m_running = false;
  d->m_isDeploying = false;
  d->m_previousBuildStepProject = nullptr;
  d->m_currentBuildStep = nullptr;

  if (d->m_progressFutureInterface) {
    d->m_progressFutureInterface->reportCanceled();
    d->m_progressFutureInterface->reportFinished();
    d->m_progressWatcher.setFuture(QFuture<void>());
    delete d->m_progressFutureInterface;
    d->m_progressFutureInterface = nullptr;
  }
  d->m_futureProgress = nullptr;
  d->m_maxProgress = 0;

  emit m_instance->buildQueueFinished(false);
}

auto BuildManager::toggleOutputWindow() -> void
{
  d->m_outputWindow->toggle(IOutputPane::ModeSwitch | IOutputPane::WithFocus);
}

auto BuildManager::showTaskWindow() -> void
{
  d->m_taskWindow->popup(IOutputPane::NoModeSwitch);
}

auto BuildManager::toggleTaskWindow() -> void
{
  d->m_taskWindow->toggle(IOutputPane::ModeSwitch | IOutputPane::WithFocus);
}

auto BuildManager::tasksAvailable() -> bool
{
  const auto count = d->m_taskWindow->taskCount(Constants::TASK_CATEGORY_BUILDSYSTEM) + d->m_taskWindow->taskCount(Constants::TASK_CATEGORY_COMPILE) + d->m_taskWindow->taskCount(Constants::TASK_CATEGORY_DEPLOYMENT);
  return count > 0;
}

auto BuildManager::startBuildQueue() -> void
{
  if (d->m_buildQueue.isEmpty()) {
    emit m_instance->buildQueueFinished(true);
    return;
  }

  // Delay if any of the involved build systems are currently parsing.
  const auto buildSystems = transform<QSet<BuildSystem*>>(d->m_buildQueue, [](const BuildStep *bs) { return bs->buildSystem(); });
  for (const BuildSystem *const bs : buildSystems) {
    if (!bs || !bs->isParsing())
      continue;
    d->m_scheduledBuild = connect(bs, &BuildSystem::parsingFinished, instance(), [](bool parsingSuccess) {
      if (!d->m_scheduledBuild)
        return;
      disconnect(d->m_scheduledBuild);
      d->m_scheduledBuild = {};
      if (parsingSuccess)
        startBuildQueue();
      else
        clearBuildQueue();
    }, Qt::QueuedConnection);
    return;
  }

  if (!d->m_running) {
    d->m_elapsed.start();
    // Progress Reporting
    d->m_progressFutureInterface = new QFutureInterface<void>;
    d->m_progressWatcher.setFuture(d->m_progressFutureInterface->future());
    ProgressManager::setApplicationLabel(QString());
    d->m_futureProgress = ProgressManager::addTask(d->m_progressFutureInterface->future(), QString(), "ProjectExplorer.Task.Build", ProgressManager::KeepOnFinish | ProgressManager::ShowInApplicationIcon);
    connect(d->m_futureProgress.data(), &FutureProgress::clicked, m_instance, &BuildManager::showBuildResults);
    d->m_futureProgress.data()->setWidget(new BuildProgress(d->m_taskWindow));
    d->m_futureProgress.data()->setStatusBarWidget(new BuildProgress(d->m_taskWindow, Qt::Horizontal));
    d->m_progress = 0;
    d->m_progressFutureInterface->setProgressRange(0, d->m_maxProgress * 100);

    d->m_running = true;
    d->m_allStepsSucceeded = true;
    d->m_progressFutureInterface->reportStarted();
    nextStep();
  } else {
    // Already running
    d->m_progressFutureInterface->setProgressRange(0, d->m_maxProgress * 100);
    d->m_progressFutureInterface->setProgressValueAndText(d->m_progress * 100, msgProgress(d->m_progress, d->m_maxProgress));
  }
}

auto BuildManager::showBuildResults() -> void
{
  if (tasksAvailable())
    toggleTaskWindow();
  else
    toggleOutputWindow();
  //toggleTaskWindow();
}

auto BuildManager::addToTaskWindow(const Task &task, int linkedOutputLines, int skipLines) -> void
{
  // Distribute to all others
  d->m_outputWindow->registerPositionOf(task, linkedOutputLines, skipLines);
  TaskHub::addTask(task);
}

auto BuildManager::addToOutputWindow(const QString &string, BuildStep::OutputFormat format, BuildStep::OutputNewlineSetting newlineSettings) -> void
{
  QString stringToWrite;
  if (format == BuildStep::OutputFormat::NormalMessage || format == BuildStep::OutputFormat::ErrorMessage) {
    stringToWrite = QTime::currentTime().toString();
    stringToWrite += ": ";
  }
  stringToWrite += string;
  if (newlineSettings == BuildStep::DoAppendNewline)
    stringToWrite += '\n';
  d->m_outputWindow->appendText(stringToWrite, format);
}

auto BuildManager::nextBuildQueue() -> void
{
  d->m_outputWindow->flush();
  if (d->m_canceling) {
    d->m_canceling = false;
    QTimer::singleShot(0, m_instance, &BuildManager::emitCancelMessage);

    disconnectOutput(d->m_currentBuildStep);
    decrementActiveBuildSteps(d->m_currentBuildStep);

    //TODO NBS fix in qtconcurrent
    d->m_progressFutureInterface->setProgressValueAndText(d->m_progress * 100, tr("Build/Deployment canceled"));
    clearBuildQueue();
    return;
  }

  disconnectOutput(d->m_currentBuildStep);
  if (!d->m_skipDisabled)
    ++d->m_progress;
  d->m_progressFutureInterface->setProgressValueAndText(d->m_progress * 100, msgProgress(d->m_progress, d->m_maxProgress));
  decrementActiveBuildSteps(d->m_currentBuildStep);

  const auto success = d->m_skipDisabled || d->m_lastStepSucceeded;
  if (success) {
    nextStep();
  } else {
    // Build Failure
    d->m_allStepsSucceeded = false;
    const auto t = d->m_currentBuildStep->target();
    const auto projectName = d->m_currentBuildStep->project()->displayName();
    const auto targetName = t->displayName();
    addToOutputWindow(tr("Error while building/deploying project %1 (kit: %2)").arg(projectName, targetName), BuildStep::OutputFormat::Stderr);
    const auto kitTasks = t->kit()->validate();
    if (!kitTasks.isEmpty()) {
      addToOutputWindow(tr("The kit %1 has configuration issues which might be the root cause for this problem.").arg(targetName), BuildStep::OutputFormat::Stderr);
    }
    addToOutputWindow(tr("When executing step \"%1\"").arg(d->m_currentBuildStep->displayName()), BuildStep::OutputFormat::Stderr);

    auto abort = ProjectExplorerPlugin::projectExplorerSettings().abortBuildAllOnError;
    if (!abort) {
      while (!d->m_buildQueue.isEmpty() && d->m_buildQueue.front()->target() == t) {
        const auto nextStepForFailedTarget = d->m_buildQueue.takeFirst();
        disconnectOutput(nextStepForFailedTarget);
        decrementActiveBuildSteps(nextStepForFailedTarget);
      }
      if (d->m_buildQueue.isEmpty())
        abort = true;
    }

    if (abort) {
      // NBS TODO fix in qtconcurrent
      d->m_progressFutureInterface->setProgressValueAndText(d->m_progress * 100, tr("Error while building/deploying project %1 (kit: %2)").arg(projectName, targetName));
      clearBuildQueue();
    } else {
      nextStep();
    }
  }
}

auto BuildManager::progressChanged(int percent, const QString &text) -> void
{
  if (d->m_progressFutureInterface)
    d->m_progressFutureInterface->setProgressValueAndText(percent + 100 * d->m_progress, text);
}

auto BuildManager::nextStep() -> void
{
  if (!d->m_buildQueue.empty()) {
    d->m_currentBuildStep = d->m_buildQueue.front();
    d->m_buildQueue.pop_front();
    const auto name = d->m_stepNames.takeFirst();
    d->m_skipDisabled = !d->m_enabledState.takeFirst();
    if (d->m_futureProgress)
      d->m_futureProgress.data()->setTitle(name);

    if (d->m_currentBuildStep->project() != d->m_previousBuildStepProject) {
      const auto projectName = d->m_currentBuildStep->project()->displayName();
      addToOutputWindow(tr("Running steps for project %1...").arg(projectName), BuildStep::OutputFormat::NormalMessage);
      d->m_previousBuildStepProject = d->m_currentBuildStep->project();
    }

    if (d->m_skipDisabled) {
      addToOutputWindow(tr("Skipping disabled step %1.").arg(d->m_currentBuildStep->displayName()), BuildStep::OutputFormat::NormalMessage);
      nextBuildQueue();
      return;
    }

    static const auto finishedHandler = [](bool success) {
      d->m_outputWindow->flush();
      d->m_lastStepSucceeded = success;
      disconnect(d->m_currentBuildStep, nullptr, instance(), nullptr);
      nextBuildQueue();
    };
    connect(d->m_currentBuildStep, &BuildStep::finished, instance(), finishedHandler);
    connect(d->m_currentBuildStep, &BuildStep::progress, instance(), &BuildManager::progressChanged);
    d->m_outputWindow->reset();
    d->m_currentBuildStep->setupOutputFormatter(d->m_outputWindow->outputFormatter());
    d->m_currentBuildStep->run();
  } else {
    d->m_running = false;
    d->m_isDeploying = false;
    d->m_previousBuildStepProject = nullptr;
    d->m_progressFutureInterface->reportFinished();
    d->m_progressWatcher.setFuture(QFuture<void>());
    d->m_currentBuildStep = nullptr;
    delete d->m_progressFutureInterface;
    d->m_progressFutureInterface = nullptr;
    d->m_maxProgress = 0;
    emit m_instance->buildQueueFinished(d->m_allStepsSucceeded);
  }
}

auto BuildManager::buildQueueAppend(const QList<BuildStep*> &steps, QStringList names, const QStringList &preambleMessage) -> bool
{
  if (!d->m_running) {
    d->m_outputWindow->clearContents();
    if (ProjectExplorerPlugin::projectExplorerSettings().clearIssuesOnRebuild) {
      TaskHub::clearTasks(Constants::TASK_CATEGORY_COMPILE);
      TaskHub::clearTasks(Constants::TASK_CATEGORY_BUILDSYSTEM);
      TaskHub::clearTasks(Constants::TASK_CATEGORY_DEPLOYMENT);
      TaskHub::clearTasks(Constants::TASK_CATEGORY_AUTOTEST);
    }

    for (const auto &str : preambleMessage)
      addToOutputWindow(str, BuildStep::OutputFormat::NormalMessage, BuildStep::DontAppendNewline);
  }

  const int count = steps.size();
  auto init = true;
  auto i = 0;
  for (; i < count; ++i) {
    const auto bs = steps.at(i);
    connect(bs, &BuildStep::addTask, m_instance, &BuildManager::addToTaskWindow);
    connect(bs, &BuildStep::addOutput, m_instance, &BuildManager::addToOutputWindow);
    if (bs->enabled()) {
      init = bs->init();
      if (!init)
        break;
    }
  }
  if (!init) {
    const auto bs = steps.at(i);

    // cleaning up
    // print something for the user
    const auto projectName = bs->project()->displayName();
    const auto targetName = bs->target()->displayName();
    addToOutputWindow(tr("Error while building/deploying project %1 (kit: %2)").arg(projectName, targetName), BuildStep::OutputFormat::Stderr);
    addToOutputWindow(tr("When executing step \"%1\"").arg(bs->displayName()), BuildStep::OutputFormat::Stderr);

    // disconnect the buildsteps again
    for (auto j = 0; j <= i; ++j)
      disconnectOutput(steps.at(j));
    return false;
  }

  // Everthing init() well
  for (i = 0; i < count; ++i) {
    d->m_buildQueue.append(steps.at(i));
    d->m_stepNames.append(names.at(i));
    const auto enabled = steps.at(i)->enabled();
    d->m_enabledState.append(enabled);
    if (enabled)
      ++d->m_maxProgress;
    incrementActiveBuildSteps(steps.at(i));
  }
  return true;
}

auto BuildManager::buildList(BuildStepList *bsl) -> bool
{
  return buildLists({bsl});
}

auto BuildManager::buildLists(const QList<BuildStepList*> bsls, const QStringList &preambelMessage) -> bool
{
  QList<BuildStep*> steps;
  QStringList stepListNames;
  for (const auto list : bsls) {
    steps.append(list->steps());
    stepListNames.append(displayNameForStepId(list->id()));
    d->m_isDeploying = d->m_isDeploying || list->id() == Constants::BUILDSTEPS_DEPLOY;
  }

  QStringList names;
  names.reserve(steps.size());
  for (auto i = 0; i < bsls.size(); ++i) {
    for (auto j = 0; j < bsls.at(i)->count(); ++j)
      names.append(stepListNames.at(i));
  }

  const auto success = buildQueueAppend(steps, names, preambelMessage);
  if (!success) {
    d->m_outputWindow->popup(IOutputPane::NoModeSwitch);
    d->m_isDeploying = false;
    return false;
  }

  if (d->m_outputWindow->settings().popUp)
    d->m_outputWindow->popup(IOutputPane::NoModeSwitch);
  startBuildQueue();
  return true;
}

auto BuildManager::appendStep(BuildStep *step, const QString &name) -> void
{
  const auto success = buildQueueAppend({step}, {name});
  if (!success) {
    d->m_outputWindow->popup(IOutputPane::NoModeSwitch);
    return;
  }
  if (d->m_outputWindow->settings().popUp)
    d->m_outputWindow->popup(IOutputPane::NoModeSwitch);
  startBuildQueue();
}

template <class T>
auto count(const QHash<T*, int> &hash, const T *key) -> int
{
  typename QHash<T*, int>::const_iterator it = hash.find(const_cast<T*>(key));
  typename QHash<T*, int>::const_iterator end = hash.end();
  if (it != end)
    return *it;
  return 0;
}

auto BuildManager::isBuilding(const Project *pro) -> bool
{
  return count(d->m_activeBuildSteps, pro) > 0;
}

auto BuildManager::isBuilding(const Target *t) -> bool
{
  return count(d->m_activeBuildStepsPerTarget, t) > 0;
}

auto BuildManager::isBuilding(const ProjectConfiguration *p) -> bool
{
  return count(d->m_activeBuildStepsPerProjectConfiguration, p) > 0;
}

auto BuildManager::isBuilding(BuildStep *step) -> bool
{
  return (d->m_currentBuildStep == step) || d->m_buildQueue.contains(step);
}

template <class T>
auto increment(QHash<T*, int> &hash, T *key) -> bool
{
  typename QHash<T*, int>::iterator it = hash.find(key);
  typename QHash<T*, int>::iterator end = hash.end();
  if (it == end) {
    hash.insert(key, 1);
    return true;
  } else if (*it == 0) {
    ++*it;
    return true;
  } else {
    ++*it;
  }
  return false;
}

template <class T>
auto decrement(QHash<T*, int> &hash, T *key) -> bool
{
  typename QHash<T*, int>::iterator it = hash.find(key);
  typename QHash<T*, int>::iterator end = hash.end();
  if (it == end) {
    // Can't happen
  } else if (*it == 1) {
    --*it;
    return true;
  } else {
    --*it;
  }
  return false;
}

auto BuildManager::incrementActiveBuildSteps(BuildStep *bs) -> void
{
  increment<ProjectConfiguration>(d->m_activeBuildStepsPerProjectConfiguration, bs->projectConfiguration());
  increment<Target>(d->m_activeBuildStepsPerTarget, bs->target());
  if (increment<Project>(d->m_activeBuildSteps, bs->project())) emit m_instance->buildStateChanged(bs->project());
}

auto BuildManager::decrementActiveBuildSteps(BuildStep *bs) -> void
{
  decrement<ProjectConfiguration>(d->m_activeBuildStepsPerProjectConfiguration, bs->projectConfiguration());
  decrement<Target>(d->m_activeBuildStepsPerTarget, bs->target());
  if (decrement<Project>(d->m_activeBuildSteps, bs->project())) emit m_instance->buildStateChanged(bs->project());
}

auto BuildManager::disconnectOutput(BuildStep *bs) -> void
{
  disconnect(bs, &BuildStep::addTask, m_instance, nullptr);
  disconnect(bs, &BuildStep::addOutput, m_instance, nullptr);
}

} // namespace ProjectExplorer
