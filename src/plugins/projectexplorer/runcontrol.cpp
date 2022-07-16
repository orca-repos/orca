// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runcontrol.hpp"

#include "devicesupport/desktopdevice.hpp"
#include "abi.hpp"
#include "buildconfiguration.hpp"
#include "customparser.hpp"
#include "environmentaspect.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "runconfigurationaspects.hpp"
#include "session.hpp"
#include "target.hpp"
#include "toolchain.hpp"

#include <utils/algorithm.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/detailswidget.hpp>
#include <utils/fileinprojectfinder.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <core/core-context-interface.hpp>
#include <core/core-interface.hpp>

#include <ssh/sshsettings.h>

#include <QDir>
#include <QFormLayout>
#include <QHash>
#include <QPushButton>
#include <QTimer>
#include <QLoggingCategory>
#include <QSettings>

#if defined (WITH_JOURNALD)
#include "journaldwatcher.hpp"
#endif

using namespace Utils;
using namespace ProjectExplorer::Internal;

namespace {
static Q_LOGGING_CATEGORY(statesLog, "qtc.projectmanager.states", QtWarningMsg)
}

namespace ProjectExplorer {

// RunWorkerFactory

static QList<RunWorkerFactory *> g_runWorkerFactories;
static QSet<Id> g_runModes;
static QSet<Id> g_runConfigs;

RunWorkerFactory::RunWorkerFactory()
{
  g_runWorkerFactories.append(this);
}

RunWorkerFactory::RunWorkerFactory(const WorkerCreator &producer, const QList<Id> &runModes, const QList<Id> &runConfigs, const QList<Id> &deviceTypes) : m_producer(producer), m_supportedRunModes(runModes), m_supportedRunConfigurations(runConfigs), m_supportedDeviceTypes(deviceTypes)
{
  g_runWorkerFactories.append(this);

  // Debugging only.
  for (auto runMode : runModes)
    g_runModes.insert(runMode);
  for (auto runConfig : runConfigs)
    g_runConfigs.insert(runConfig);
}

RunWorkerFactory::~RunWorkerFactory()
{
  g_runWorkerFactories.removeOne(this);
}

auto RunWorkerFactory::setProducer(const WorkerCreator &producer) -> void
{
  m_producer = producer;
}

auto RunWorkerFactory::addSupportedRunMode(Id runMode) -> void
{
  m_supportedRunModes.append(runMode);
}

auto RunWorkerFactory::addSupportedRunConfig(Id runConfig) -> void
{
  m_supportedRunConfigurations.append(runConfig);
}

auto RunWorkerFactory::addSupportedDeviceType(Id deviceType) -> void
{
  m_supportedDeviceTypes.append(deviceType);
}

auto RunWorkerFactory::canRun(Id runMode, Id deviceType, const QString &runConfigId) const -> bool
{
  if (!m_supportedRunModes.contains(runMode))
    return false;

  if (!m_supportedRunConfigurations.isEmpty()) {
    // FIXME: That's to be used after mangled ids are gone.
    //if (!m_supportedRunConfigurations.contains(runConfigId)
    // return false;
    auto ok = false;
    for (const auto &id : m_supportedRunConfigurations) {
      if (runConfigId.startsWith(id.toString())) {
        ok = true;
        break;
      }
    }

    if (!ok)
      return false;
  }

  if (!m_supportedDeviceTypes.isEmpty())
    return m_supportedDeviceTypes.contains(deviceType);

  return true;
}

auto RunWorkerFactory::dumpAll() -> void
{
  const auto devices = transform(IDeviceFactory::allDeviceFactories(), &IDeviceFactory::deviceType);

  for (auto runMode : qAsConst(g_runModes)) {
    qDebug() << "";
    for (auto device : devices) {
      for (auto runConfig : qAsConst(g_runConfigs)) {
        const auto check = std::bind(&RunWorkerFactory::canRun, std::placeholders::_1, runMode, device, runConfig.toString());
        const auto factory = findOrDefault(g_runWorkerFactories, check);
        qDebug() << "MODE:" << runMode << device << runConfig << factory;
      }
    }
  }
}

/*!
    \class ProjectExplorer::RunControl
    \brief The RunControl class instances represent one item that is run.
*/

/*!
    \fn QIcon ProjectExplorer::RunControl::icon() const
    Returns the icon to be shown in the Outputwindow.

    TODO the icon differs currently only per "mode", so this is more flexible
    than it needs to be.
*/

namespace Internal {

enum class RunWorkerState {
  Initialized,
  Starting,
  Running,
  Stopping,
  Done
};

static auto stateName(RunWorkerState s) -> QString
{
  #define SN(x) case x: return QLatin1String(#x);
  switch (s) {
    SN(RunWorkerState::Initialized)
    SN(RunWorkerState::Starting)
    SN(RunWorkerState::Running)
    SN(RunWorkerState::Stopping)
    SN(RunWorkerState::Done)
  }
  return QString("<unknown: %1>").arg(int(s));
  #undef SN
}

class RunWorkerPrivate : public QObject {
public:
  RunWorkerPrivate(RunWorker *runWorker, RunControl *runControl);

  auto canStart() const -> bool;
  auto canStop() const -> bool;
  auto timerEvent(QTimerEvent *ev) -> void override;

  auto killStartWatchdog() -> void
  {
    if (startWatchdogTimerId != -1) {
      killTimer(startWatchdogTimerId);
      startWatchdogTimerId = -1;
    }
  }

  auto killStopWatchdog() -> void
  {
    if (stopWatchdogTimerId != -1) {
      killTimer(stopWatchdogTimerId);
      stopWatchdogTimerId = -1;
    }
  }

  auto startStartWatchdog() -> void
  {
    killStartWatchdog();
    killStopWatchdog();

    if (startWatchdogInterval != 0)
      startWatchdogTimerId = startTimer(startWatchdogInterval);
  }

  auto startStopWatchdog() -> void
  {
    killStopWatchdog();
    killStartWatchdog();

    if (stopWatchdogInterval != 0)
      stopWatchdogTimerId = startTimer(stopWatchdogInterval);
  }

  RunWorker *q;
  RunWorkerState state = RunWorkerState::Initialized;
  const QPointer<RunControl> runControl;
  QList<RunWorker*> startDependencies;
  QList<RunWorker*> stopDependencies;
  QString id;

  QVariantMap data;
  int startWatchdogInterval = 0;
  int startWatchdogTimerId = -1;
  std::function<void()> startWatchdogCallback;
  int stopWatchdogInterval = 0; // 5000;
  int stopWatchdogTimerId = -1;
  std::function<void()> stopWatchdogCallback;
  bool supportsReRunning = true;
  bool essential = false;
};

enum class RunControlState {
  Initialized,
  // Default value after creation.
  Starting,
  // Actual process/tool starts.
  Running,
  // All good and running.
  Stopping,
  // initiateStop() was called, stop application/tool
  Stopped,
  // all good, but stopped. Can possibly be re-started
  Finishing,
  // Application tab manually closed
  Finished // Final state, will self-destruct with deleteLater()
};

static auto stateName(RunControlState s) -> QString
{
  #    define SN(x) case x: return QLatin1String(#x);
  switch (s) {
  SN(RunControlState::Initialized)
  SN(RunControlState::Starting)
  SN(RunControlState::Running)
  SN(RunControlState::Stopping)
  SN(RunControlState::Stopped)
  SN(RunControlState::Finishing)
  SN(RunControlState::Finished)
  }
  return QString("<unknown: %1>").arg(int(s));
  #    undef SN
}

class RunControlPrivate : public QObject {
public:
  RunControlPrivate(RunControl *parent, Id mode) : q(parent), runMode(mode)
  {
    icon = Icons::RUN_SMALL_TOOLBAR;
  }

  ~RunControlPrivate() override
  {
    QTC_CHECK(state == RunControlState::Finished || state == RunControlState::Initialized);
    disconnect();
    q = nullptr;
    qDeleteAll(m_workers);
    m_workers.clear();
  }

  Q_ENUM(RunControlState)

  auto checkState(RunControlState expectedState) -> void;
  auto setState(RunControlState state) -> void;
  auto debugMessage(const QString &msg) -> void;
  auto initiateStart() -> void;
  auto initiateReStart() -> void;
  auto continueStart() -> void;
  auto initiateStop() -> void;
  auto forceStop() -> void;
  auto continueStopOrFinish() -> void;
  auto initiateFinish() -> void;
  auto onWorkerStarted(RunWorker *worker) -> void;
  auto onWorkerStopped(RunWorker *worker) -> void;
  auto onWorkerFailed(RunWorker *worker, const QString &msg) -> void;
  auto showError(const QString &msg) -> void;
  static auto isAllowedTransition(RunControlState from, RunControlState to) -> bool;
  auto supportsReRunning() const -> bool;

  RunControl *q;
  QString displayName;
  Runnable runnable;
  IDevice::ConstPtr device;
  Id runMode;
  Icon icon;
  const MacroExpander *macroExpander = nullptr;
  QPointer<RunConfiguration> runConfiguration; // Not owned. Avoid use.
  QString buildKey;
  QMap<Id, QVariantMap> settingsData;
  Id runConfigId;
  BuildTargetInfo buildTargetInfo;
  BuildConfiguration::BuildType buildType = BuildConfiguration::Unknown;
  FilePath buildDirectory;
  Environment buildEnvironment;
  Kit *kit = nullptr;        // Not owned.
  QPointer<Target> target;   // Not owned.
  QPointer<Project> project; // Not owned.
  std::function<bool(bool *)> promptToStop;
  std::vector<RunWorkerFactory> m_factories;
  // A handle to the actual application process.
  ProcessHandle applicationProcessHandle;
  RunControlState state = RunControlState::Initialized;
  QList<QPointer<RunWorker>> m_workers;
};

} // Internal

using namespace Internal;

RunControl::RunControl(Id mode) : d(std::make_unique<RunControlPrivate>(this, mode)) {}

auto RunControl::setRunConfiguration(RunConfiguration *runConfig) -> void
{
  QTC_ASSERT(runConfig, return);
  QTC_CHECK(!d->runConfiguration);
  d->runConfiguration = runConfig;
  d->runConfigId = runConfig->id();
  d->runnable = runConfig->runnable();
  d->displayName = runConfig->expandedDisplayName();
  d->buildKey = runConfig->buildKey();
  d->settingsData = runConfig->aspectData();

  setTarget(runConfig->target());

  d->macroExpander = runConfig->macroExpander();
}

auto RunControl::setTarget(Target *target) -> void
{
  QTC_ASSERT(target, return);
  QTC_CHECK(!d->target);
  d->target = target;

  if (!d->buildKey.isEmpty() && target->buildSystem())
    d->buildTargetInfo = target->buildTarget(d->buildKey);

  if (const auto bc = target->activeBuildConfiguration()) {
    d->buildType = bc->buildType();
    d->buildDirectory = bc->buildDirectory();
    d->buildEnvironment = bc->environment();
  }

  setKit(target->kit());
  d->macroExpander = target->macroExpander();
  d->project = target->project();
}

auto RunControl::setKit(Kit *kit) -> void
{
  QTC_ASSERT(kit, return);
  QTC_CHECK(!d->kit);
  d->kit = kit;
  d->macroExpander = kit->macroExpander();

  if (d->runnable.device)
    setDevice(d->runnable.device);
  else
    setDevice(DeviceKitAspect::device(kit));
}

auto RunControl::setDevice(const IDevice::ConstPtr &device) -> void
{
  QTC_CHECK(!d->device);
  d->device = device;
  #ifdef WITH_JOURNALD
    if (!device.isNull() && device->type() == ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE) {
        JournaldWatcher::instance()->subscribe(this, [this](const JournaldWatcher::LogEntry &entry) {

            if (entry.value("_MACHINE_ID") != JournaldWatcher::instance()->machineId())
                return;

            const QByteArray pid = entry.value("_PID");
            if (pid.isEmpty())
                return;

            const qint64 pidNum = static_cast<qint64>(QString::fromLatin1(pid).toInt());
            if (pidNum != d->applicationProcessHandle.pid())
                return;

            const QString message = QString::fromUtf8(entry.value("MESSAGE")) + "\n";
            appendMessage(message, Utils::OutputFormat::LogMessageFormat);
        });
    }
  #endif
}

RunControl::~RunControl()
{
  #ifdef WITH_JOURNALD
    JournaldWatcher::instance()->unsubscribe(this);
  #endif
}

auto RunControl::initiateStart() -> void
{
  emit aboutToStart();
  d->initiateStart();
}

auto RunControl::initiateReStart() -> void
{
  emit aboutToStart();
  d->initiateReStart();
}

auto RunControl::initiateStop() -> void
{
  d->initiateStop();
}

auto RunControl::forceStop() -> void
{
  d->forceStop();
}

auto RunControl::initiateFinish() -> void
{
  QTimer::singleShot(0, d.get(), &RunControlPrivate::initiateFinish);
}

auto RunControl::createWorker(Id workerId) -> RunWorker*
{
  const auto check = std::bind(&RunWorkerFactory::canRun, std::placeholders::_1, workerId, DeviceTypeKitAspect::deviceTypeId(d->kit), QString{});
  const auto factory = findOrDefault(g_runWorkerFactories, check);
  return factory ? factory->producer()(this) : nullptr;
}

auto RunControl::createMainWorker() -> bool
{
  const auto canRun = std::bind(&RunWorkerFactory::canRun, std::placeholders::_1, d->runMode, DeviceTypeKitAspect::deviceTypeId(d->kit), d->runConfigId.toString());

  const auto candidates = filtered(g_runWorkerFactories, canRun);
  // There might be combinations that cannot run. But that should have been checked
  // with canRun below.
  QTC_ASSERT(!candidates.empty(), return false);

  // There should be at most one top-level producer feeling responsible per combination.
  // Breaking a tie should be done by tightening the restrictions on one of them.
  QTC_CHECK(candidates.size() == 1);
  return candidates.front()->producer()(this) != nullptr;
}

auto RunControl::canRun(Id runMode, Id deviceType, Id runConfigId) -> bool
{
  for (const RunWorkerFactory *factory : qAsConst(g_runWorkerFactories)) {
    if (factory->canRun(runMode, deviceType, runConfigId.toString()))
      return true;
  }
  return false;
}

auto RunControlPrivate::initiateStart() -> void
{
  checkState(RunControlState::Initialized);
  setState(RunControlState::Starting);
  debugMessage("Queue: Starting");

  continueStart();
}

auto RunControlPrivate::initiateReStart() -> void
{
  checkState(RunControlState::Stopped);

  // Re-set worked on re-runs.
  for (const RunWorker *worker : qAsConst(m_workers)) {
    if (worker->d->state == RunWorkerState::Done)
      worker->d->state = RunWorkerState::Initialized;
  }

  setState(RunControlState::Starting);
  debugMessage("Queue: ReStarting");

  continueStart();
}

auto RunControlPrivate::continueStart() -> void
{
  checkState(RunControlState::Starting);
  auto allDone = true;
  debugMessage("Looking for next worker");
  for (const RunWorker *worker : qAsConst(m_workers)) {
    if (worker) {
      const auto &workerId = worker->d->id;
      debugMessage("  Examining worker " + workerId);
      switch (worker->d->state) {
      case RunWorkerState::Initialized:
        debugMessage("  " + workerId + " is not done yet.");
        if (worker->d->canStart()) {
          debugMessage("Starting " + workerId);
          worker->d->state = RunWorkerState::Starting;
          QTimer::singleShot(0, worker, &RunWorker::initiateStart);
          return;
        }
        allDone = false;
        debugMessage("  " + workerId + " cannot start.");
        break;
      case RunWorkerState::Starting:
        debugMessage("  " + workerId + " currently starting");
        allDone = false;
        break;
      case RunWorkerState::Running:
        debugMessage("  " + workerId + " currently running");
        break;
      case RunWorkerState::Stopping:
        debugMessage("  " + workerId + " currently stopping");
        continue;
      case RunWorkerState::Done:
        debugMessage("  " + workerId + " was done before");
        break;
      }
    } else {
      debugMessage("Found unknown deleted worker while starting");
    }
  }
  if (allDone)
    setState(RunControlState::Running);
}

auto RunControlPrivate::initiateStop() -> void
{
  if (state != RunControlState::Starting && state != RunControlState::Running)
    qDebug() << "Unexpected initiateStop() in state" << stateName(state);

  setState(RunControlState::Stopping);
  debugMessage("Queue: Stopping for all workers");

  continueStopOrFinish();
}

auto RunControlPrivate::continueStopOrFinish() -> void
{
  auto allDone = true;

  auto queueStop = [this](RunWorker *worker, const QString &message) {
    if (worker->d->canStop()) {
      debugMessage(message);
      worker->d->state = RunWorkerState::Stopping;
      QTimer::singleShot(0, worker, &RunWorker::initiateStop);
    } else {
      debugMessage(" " + worker->d->id + " is waiting for dependent workers to stop");
    }
  };

  for (RunWorker *worker : qAsConst(m_workers)) {
    if (worker) {
      const auto &workerId = worker->d->id;
      debugMessage("  Examining worker " + workerId);
      switch (worker->d->state) {
      case RunWorkerState::Initialized:
        debugMessage("  " + workerId + " was Initialized, setting to Done");
        worker->d->state = RunWorkerState::Done;
        break;
      case RunWorkerState::Stopping:
        debugMessage("  " + workerId + " was already Stopping. Keeping it that way");
        allDone = false;
        break;
      case RunWorkerState::Starting:
        queueStop(worker, "  " + workerId + " was Starting, queuing stop");
        allDone = false;
        break;
      case RunWorkerState::Running:
        queueStop(worker, "  " + workerId + " was Running, queuing stop");
        allDone = false;
        break;
      case RunWorkerState::Done:
        debugMessage("  " + workerId + " was Done. Good.");
        break;
      }
    } else {
      debugMessage("Found unknown deleted worker");
    }
  }

  RunControlState targetState;
  if (state == RunControlState::Finishing) {
    targetState = RunControlState::Finished;
  } else {
    checkState(RunControlState::Stopping);
    targetState = RunControlState::Stopped;
  }

  if (allDone) {
    debugMessage("All Stopped");
    setState(targetState);
  } else {
    debugMessage("Not all workers Stopped. Waiting...");
  }
}

auto RunControlPrivate::forceStop() -> void
{
  if (state == RunControlState::Finished) {
    debugMessage("Was finished, too late to force Stop");
    return;
  }
  for (const RunWorker *worker : qAsConst(m_workers)) {
    if (worker) {
      const auto &workerId = worker->d->id;
      debugMessage("  Examining worker " + workerId);
      switch (worker->d->state) {
      case RunWorkerState::Initialized:
        debugMessage("  " + workerId + " was Initialized, setting to Done");
        break;
      case RunWorkerState::Stopping:
        debugMessage("  " + workerId + " was already Stopping. Set it forcefully to Done.");
        break;
      case RunWorkerState::Starting:
        debugMessage("  " + workerId + " was Starting. Set it forcefully to Done.");
        break;
      case RunWorkerState::Running:
        debugMessage("  " + workerId + " was Running. Set it forcefully to Done.");
        break;
      case RunWorkerState::Done:
        debugMessage("  " + workerId + " was Done. Good.");
        break;
      }
      worker->d->state = RunWorkerState::Done;
    } else {
      debugMessage("Found unknown deleted worker");
    }
  }

  setState(RunControlState::Stopped);
  debugMessage("All Stopped");
}

auto RunControlPrivate::initiateFinish() -> void
{
  setState(RunControlState::Finishing);
  debugMessage("Ramping down");

  continueStopOrFinish();
}

auto RunControlPrivate::onWorkerStarted(RunWorker *worker) -> void
{
  worker->d->state = RunWorkerState::Running;

  if (state == RunControlState::Starting) {
    debugMessage(worker->d->id + " start succeeded");
    continueStart();
    return;
  }
  showError(RunControl::tr("Unexpected run control state %1 when worker %2 started.").arg(stateName(state)).arg(worker->d->id));
}

auto RunControlPrivate::onWorkerFailed(RunWorker *worker, const QString &msg) -> void
{
  worker->d->state = RunWorkerState::Done;

  showError(msg);
  switch (state) {
  case RunControlState::Initialized:
    // FIXME 1: We don't have an output pane yet, so use some other mechanism for now.
    // FIXME 2: Translation...
    QMessageBox::critical(Orca::Plugin::Core::ICore::dialogParent(), QCoreApplication::translate("TaskHub", "Error"), QString("Failure during startup. Aborting.") + "<p>" + msg);
    continueStopOrFinish();
    break;
  case RunControlState::Starting:
  case RunControlState::Running:
    initiateStop();
    break;
  case RunControlState::Stopping:
  case RunControlState::Finishing:
    continueStopOrFinish();
    break;
  case RunControlState::Stopped:
  case RunControlState::Finished: QTC_CHECK(false); // Should not happen.
    continueStopOrFinish();
    break;
  }
}

auto RunControlPrivate::onWorkerStopped(RunWorker *worker) -> void
{
  const auto &workerId = worker->d->id;
  switch (worker->d->state) {
  case RunWorkerState::Running:
    // That was a spontaneous stop.
    worker->d->state = RunWorkerState::Done;
    debugMessage(workerId + " stopped spontaneously.");
    break;
  case RunWorkerState::Stopping:
    worker->d->state = RunWorkerState::Done;
    debugMessage(workerId + " stopped expectedly.");
    break;
  case RunWorkerState::Done:
    worker->d->state = RunWorkerState::Done;
    debugMessage(workerId + " stopped twice. Huh? But harmless.");
    return; // Sic!
  default:
    debugMessage(workerId + " stopped unexpectedly in state" + stateName(worker->d->state));
    worker->d->state = RunWorkerState::Done;
    break;
  }

  if (state == RunControlState::Finishing || state == RunControlState::Stopping) {
    continueStopOrFinish();
    return;
  } else if (worker->isEssential()) {
    debugMessage(workerId + " is essential. Stopping all others.");
    initiateStop();
    return;
  }

  for (const auto dependent : qAsConst(worker->d->stopDependencies)) {
    switch (dependent->d->state) {
    case RunWorkerState::Done:
      break;
    case RunWorkerState::Initialized:
      dependent->d->state = RunWorkerState::Done;
      break;
    default:
      debugMessage("Killing " + dependent->d->id + " as it depends on stopped " + workerId);
      dependent->d->state = RunWorkerState::Stopping;
      QTimer::singleShot(0, dependent, &RunWorker::initiateStop);
      break;
    }
  }

  debugMessage("Checking whether all stopped");
  auto allDone = true;
  for (const RunWorker *worker : qAsConst(m_workers)) {
    if (worker) {
      const auto &workerId = worker->d->id;
      debugMessage("  Examining worker " + workerId);
      switch (worker->d->state) {
      case RunWorkerState::Initialized:
        debugMessage("  " + workerId + " was Initialized.");
        break;
      case RunWorkerState::Starting:
        debugMessage("  " + workerId + " was Starting, waiting for its response");
        allDone = false;
        break;
      case RunWorkerState::Running:
        debugMessage("  " + workerId + " was Running, waiting for its response");
        allDone = false;
        break;
      case RunWorkerState::Stopping:
        debugMessage("  " + workerId + " was already Stopping. Keeping it that way");
        allDone = false;
        break;
      case RunWorkerState::Done:
        debugMessage("  " + workerId + " was Done. Good.");
        break;
      }
    } else {
      debugMessage("Found unknown deleted worker");
    }
  }

  if (allDone) {
    if (state == RunControlState::Stopped) {
      debugMessage("All workers stopped, but runControl was already stopped.");
    } else {
      debugMessage("All workers stopped. Set runControl to Stopped");
      setState(RunControlState::Stopped);
    }
  } else {
    debugMessage("Not all workers stopped. Waiting...");
  }
}

auto RunControlPrivate::showError(const QString &msg) -> void
{
  if (!msg.isEmpty()) emit q->appendMessage(msg + '\n', ErrorMessageFormat);
}

auto RunControl::setupFormatter(OutputFormatter *formatter) const -> void
{
  auto parsers = OutputFormatterFactory::createFormatters(target());
  if (const auto customParsersAspect = (runConfiguration() ? runConfiguration()->aspect<CustomParsersAspect>() : nullptr)) {
    for (const auto id : customParsersAspect->parsers()) {
      if (const auto parser = CustomParser::createFromId(id))
        parsers << parser;
    }
  }
  formatter->setLineParsers(parsers);
  if (project()) {
    FileInProjectFinder fileFinder;
    fileFinder.setProjectDirectory(project()->projectDirectory());
    fileFinder.setProjectFiles(project()->files(Project::AllFiles));
    formatter->setFileFinder(fileFinder);
  }
}

auto RunControl::runMode() const -> Id
{
  return d->runMode;
}

auto RunControl::runnable() const -> const Runnable&
{
  return d->runnable;
}

auto RunControl::setRunnable(const Runnable &runnable) -> void
{
  d->runnable = runnable;
}

auto RunControl::displayName() const -> QString
{
  return d->displayName;
}

auto RunControl::setDisplayName(const QString &displayName) -> void
{
  d->displayName = displayName;
}

auto RunControl::setIcon(const Icon &icon) -> void
{
  d->icon = icon;
}

auto RunControl::icon() const -> Icon
{
  return d->icon;
}

auto RunControl::device() const -> IDevice::ConstPtr
{
  return d->device;
}

auto RunControl::runConfiguration() const -> RunConfiguration*
{
  return d->runConfiguration.data();
}

auto RunControl::target() const -> Target*
{
  return d->target;
}

auto RunControl::project() const -> Project*
{
  return d->project;
}

auto RunControl::kit() const -> Kit*
{
  return d->kit;
}

auto RunControl::macroExpander() const -> const MacroExpander*
{
  return d->macroExpander;
}

auto RunControl::aspect(Id id) const -> BaseAspect*
{
  return d->runConfiguration ? d->runConfiguration->aspect(id) : nullptr;
}

auto RunControl::settingsData(Id id) const -> QVariantMap
{
  return d->settingsData.value(id);
}

auto RunControl::buildKey() const -> QString
{
  return d->buildKey;
}

auto RunControl::buildType() const -> BuildConfiguration::BuildType
{
  return d->buildType;
}

auto RunControl::buildDirectory() const -> FilePath
{
  return d->buildDirectory;
}

auto RunControl::buildEnvironment() const -> Environment
{
  return d->buildEnvironment;
}

auto RunControl::targetFilePath() const -> FilePath
{
  return d->buildTargetInfo.targetFilePath;
}

auto RunControl::projectFilePath() const -> FilePath
{
  return d->buildTargetInfo.projectFilePath;
}

/*!
    A handle to the application process.

    This is typically a process id, but should be treated as
    opaque handle to the process controled by this \c RunControl.
*/

auto RunControl::applicationProcessHandle() const -> ProcessHandle
{
  return d->applicationProcessHandle;
}

auto RunControl::setApplicationProcessHandle(const ProcessHandle &handle) -> void
{
  if (d->applicationProcessHandle != handle) {
    d->applicationProcessHandle = handle;
    emit applicationProcessHandleChanged(QPrivateSignal());
  }
}

/*!
    Prompts to stop. If \a optionalPrompt is passed, a \gui {Do not ask again}
    checkbox is displayed and the result is returned in \a *optionalPrompt.
*/

auto RunControl::promptToStop(bool *optionalPrompt) const -> bool
{
  QTC_ASSERT(isRunning(), return true);
  if (optionalPrompt && !*optionalPrompt)
    return true;

  // Overridden.
  if (d->promptToStop)
    return d->promptToStop(optionalPrompt);

  const auto msg = tr("<html><head/><body><center><i>%1</i> is still running.<center/>" "<center>Force it to quit?</center></body></html>").arg(displayName());
  return showPromptToStopDialog(tr("Application Still Running"), msg, tr("Force &Quit"), tr("&Keep Running"), optionalPrompt);
}

auto RunControl::setPromptToStop(const std::function<bool (bool *)> &promptToStop) -> void
{
  d->promptToStop = promptToStop;
}

auto RunControl::supportsReRunning() const -> bool
{
  return d->supportsReRunning();
}

auto RunControlPrivate::supportsReRunning() const -> bool
{
  for (const RunWorker *worker : m_workers) {
    if (!worker->d->supportsReRunning)
      return false;
    if (worker->d->state != RunWorkerState::Done)
      return false;
  }
  return true;
}

auto RunControl::isRunning() const -> bool
{
  return d->state == RunControlState::Running;
}

auto RunControl::isStarting() const -> bool
{
  return d->state == RunControlState::Starting;
}

auto RunControl::isStopping() const -> bool
{
  return d->state == RunControlState::Stopping;
}

auto RunControl::isStopped() const -> bool
{
  return d->state == RunControlState::Stopped;
}

/*!
    Prompts to terminate the application with the \gui {Do not ask again}
    checkbox.
*/

auto RunControl::showPromptToStopDialog(const QString &title, const QString &text, const QString &stopButtonText, const QString &cancelButtonText, bool *prompt) -> bool
{
  // Show a question message box where user can uncheck this
  // question for this class.
  CheckableMessageBox messageBox(Orca::Plugin::Core::ICore::dialogParent());
  messageBox.setWindowTitle(title);
  messageBox.setText(text);
  messageBox.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::Cancel);
  if (!stopButtonText.isEmpty())
    messageBox.button(QDialogButtonBox::Yes)->setText(stopButtonText);
  if (!cancelButtonText.isEmpty())
    messageBox.button(QDialogButtonBox::Cancel)->setText(cancelButtonText);
  messageBox.setDefaultButton(QDialogButtonBox::Yes);
  if (prompt) {
    messageBox.setCheckBoxText(CheckableMessageBox::msgDoNotAskAgain());
    messageBox.setChecked(false);
  } else {
    messageBox.setCheckBoxVisible(false);
  }
  messageBox.exec();
  const auto close = messageBox.clickedStandardButton() == QDialogButtonBox::Yes;
  if (close && prompt && messageBox.isChecked())
    *prompt = false;
  return close;
}

auto RunControl::provideAskPassEntry(Environment &env) -> void
{
  if (env.value("SUDO_ASKPASS").isEmpty()) {
    const FilePath askpass = QSsh::SshSettings::askpassFilePath();
    if (askpass.exists())
      env.set("SUDO_ASKPASS", askpass.toUserOutput());
  }
}

auto RunControlPrivate::isAllowedTransition(RunControlState from, RunControlState to) -> bool
{
  switch (from) {
  case RunControlState::Initialized:
    return to == RunControlState::Starting || to == RunControlState::Finishing;
  case RunControlState::Starting:
    return to == RunControlState::Running || to == RunControlState::Stopping || to == RunControlState::Finishing;
  case RunControlState::Running:
    return to == RunControlState::Stopping || to == RunControlState::Stopped || to == RunControlState::Finishing;
  case RunControlState::Stopping:
    return to == RunControlState::Stopped || to == RunControlState::Finishing;
  case RunControlState::Stopped:
    return to == RunControlState::Starting || to == RunControlState::Finishing;
  case RunControlState::Finishing:
    return to == RunControlState::Finished;
  case RunControlState::Finished:
    return false;
  }
  return false;
}

auto RunControlPrivate::checkState(RunControlState expectedState) -> void
{
  if (state != expectedState)
    qDebug() << "Unexpected run control state " << stateName(expectedState) << " have: " << stateName(state);
}

auto RunControlPrivate::setState(RunControlState newState) -> void
{
  if (!isAllowedTransition(state, newState))
    qDebug() << "Invalid run control state transition from " << stateName(state) << " to " << stateName(newState);

  state = newState;

  debugMessage("Entering state " + stateName(newState));

  // Extra reporting.
  switch (state) {
  case RunControlState::Running: emit q->started();
    break;
  case RunControlState::Stopped:
    q->setApplicationProcessHandle(ProcessHandle());
    emit q->stopped();
    break;
  case RunControlState::Finished: emit q->finished();
    debugMessage("All finished. Deleting myself");
    q->deleteLater();
    break;
  default:
    break;
  }
}

auto RunControlPrivate::debugMessage(const QString &msg) -> void
{
  qCDebug(statesLog()) << msg;
}

// SimpleTargetRunner

SimpleTargetRunner::SimpleTargetRunner(RunControl *runControl) : RunWorker(runControl)
{
  setId("SimpleTargetRunner");
  if (const auto terminalAspect = runControl->aspect<TerminalAspect>())
    m_useTerminal = terminalAspect->useTerminal();
  if (const auto runAsRootAspect = runControl->aspect<RunAsRootAspect>())
    m_runAsRoot = runAsRootAspect->value();
}

auto SimpleTargetRunner::start() -> void
{
  if (m_starter)
    m_starter();
  else
    doStart(runControl()->runnable(), runControl()->device());
}

auto SimpleTargetRunner::doStart(const Runnable &runnable, const IDevice::ConstPtr &device) -> void
{
  m_stopForced = false;
  m_stopReported = false;
  m_launcher.disconnect(this);
  m_launcher.setUseTerminal(m_useTerminal);
  m_launcher.setRunAsRoot(m_runAsRoot);

  const auto isDesktop = device.isNull() || device.dynamicCast<const DesktopDevice>();
  const auto msg = RunControl::tr("Starting %1...").arg(runnable.command.toUserOutput());
  appendMessage(msg, NormalMessageFormat);

  connect(&m_launcher, &ApplicationLauncher::processExited, this, [this, runnable](int exitCode, QProcess::ExitStatus status) {
    if (m_stopReported)
      return;
    const auto msg = (status == QProcess::CrashExit) ? tr("%1 crashed.") : tr("%2 exited with code %1").arg(exitCode);
    const auto displayName = runnable.command.executable().toUserOutput();
    appendMessage(msg.arg(displayName), NormalMessageFormat);
    m_stopReported = true;
    reportStopped();
  });

  connect(&m_launcher, &ApplicationLauncher::error, this, [this, runnable](QProcess::ProcessError error) {
    if (m_stopReported)
      return;
    if (error == QProcess::Timedout)
      return; // No actual change on the process side.
    const auto msg = m_stopForced ? tr("The process was ended forcefully.") : userMessageForProcessError(error, runnable.command.executable());
    appendMessage(msg, NormalMessageFormat);
    m_stopReported = true;
    reportStopped();
  });

  connect(&m_launcher, &ApplicationLauncher::appendMessage, this, &RunWorker::appendMessage);

  if (isDesktop) {
    connect(&m_launcher, &ApplicationLauncher::processStarted, this, [this] {
      // Console processes only know their pid after being started
      auto pid = m_launcher.applicationPID();
      runControl()->setApplicationProcessHandle(pid);
      pid.activate();
      reportStarted();
    });

    if (runnable.command.isEmpty()) {
      reportFailure(RunControl::tr("No executable specified."));
    } else {
      m_launcher.start(runnable);
    }

  } else {
    connect(&m_launcher, &ApplicationLauncher::processStarted, this, &RunWorker::reportStarted);
    m_launcher.start(runnable, device);
  }
}

auto SimpleTargetRunner::stop() -> void
{
  m_stopForced = true;
  m_launcher.stop();
}

auto SimpleTargetRunner::setStarter(const std::function<void ()> &starter) -> void
{
  m_starter = starter;
}

// RunWorkerPrivate

RunWorkerPrivate::RunWorkerPrivate(RunWorker *runWorker, RunControl *runControl) : q(runWorker), runControl(runControl)
{
  runControl->d->m_workers.append(runWorker);
}

auto RunWorkerPrivate::canStart() const -> bool
{
  if (state != RunWorkerState::Initialized)
    return false;
  for (const auto worker : startDependencies) {
    QTC_ASSERT(worker, continue);
    if (worker->d->state != RunWorkerState::Done && worker->d->state != RunWorkerState::Running)
      return false;
  }
  return true;
}

auto RunWorkerPrivate::canStop() const -> bool
{
  if (state != RunWorkerState::Starting && state != RunWorkerState::Running)
    return false;
  for (const auto worker : stopDependencies) {
    QTC_ASSERT(worker, continue);
    if (worker->d->state != RunWorkerState::Done)
      return false;
  }
  return true;
}

auto RunWorkerPrivate::timerEvent(QTimerEvent *ev) -> void
{
  if (ev->timerId() == startWatchdogTimerId) {
    if (startWatchdogCallback) {
      killStartWatchdog();
      startWatchdogCallback();
    } else {
      q->reportFailure(RunWorker::tr("Worker start timed out."));
    }
    return;
  }
  if (ev->timerId() == stopWatchdogTimerId) {
    if (stopWatchdogCallback) {
      killStopWatchdog();
      stopWatchdogCallback();
    } else {
      q->reportFailure(RunWorker::tr("Worker stop timed out."));
    }
    return;
  }
}

/*!
    \class ProjectExplorer::RunWorker

    \brief The RunWorker class encapsulates a task that forms part, or
    the whole of the operation of a tool for a certain \c RunConfiguration
    according to some \c RunMode.

    A typical example for a \c RunWorker is a process, either the
    application process itself, or a helper process, such as a watchdog
    or a log parser.

    A \c RunWorker has a simple state model covering the \c Initialized,
    \c Starting, \c Running, \c Stopping, and \c Done states.

    In the course of the operation of tools several \c RunWorkers
    may co-operate and form a combined state that is presented
    to the user as \c RunControl, with direct interaction made
    possible through the buttons in the \uicontrol{Application Output}
    pane.

    RunWorkers are typically created together with their RunControl.
    The startup order of RunWorkers under a RunControl can be
    specified by making a RunWorker dependent on others.

    When a RunControl starts, it calls \c initiateStart() on RunWorkers
    with fulfilled dependencies until all workers are \c Running, or in case
    of short-lived helper tasks, \c Done.

    A RunWorker can stop spontaneously, for example when the main application
    process ends. In this case, it typically calls \c initiateStop()
    on its RunControl, which in turn passes this to all sibling
    RunWorkers.

    Pressing the stop button in the \uicontrol{Application Output} pane
    also calls \c initiateStop on the RunControl.
*/

RunWorker::RunWorker(RunControl *runControl) : d(std::make_unique<RunWorkerPrivate>(this, runControl)) { }

RunWorker::~RunWorker() = default;

/*!
 * This function is called by the RunControl once all dependencies
 * are fulfilled.
 */
auto RunWorker::initiateStart() -> void
{
  d->startStartWatchdog();
  d->runControl->d->debugMessage("Initiate start for " + d->id);
  start();
}

/*!
 * This function has to be called by a RunWorker implementation
 * to notify its RunControl about the successful start of this RunWorker.
 *
 * The RunControl may start other RunWorkers in response.
 */
auto RunWorker::reportStarted() -> void
{
  d->killStartWatchdog();
  d->runControl->d->onWorkerStarted(this);
  emit started();
}

/*!
 * This function is called by the RunControl in its own \c initiateStop
 * implementation, which is triggered in response to pressing the
 * stop button in the \uicontrol{Application Output} pane or on direct
 * request of one of the sibling RunWorkers.
 */
auto RunWorker::initiateStop() -> void
{
  d->startStopWatchdog();
  d->runControl->d->debugMessage("Initiate stop for " + d->id);
  stop();
}

/*!
 * This function has to be called by a RunWorker implementation
 * to notify its RunControl about this RunWorker having stopped.
 *
 * The stop can be spontaneous, or in response to an initiateStop()
 * or an initiateFinish() call.
 *
 * The RunControl will adjust its global state in response.
 */
auto RunWorker::reportStopped() -> void
{
  d->killStopWatchdog();
  d->runControl->d->onWorkerStopped(this);
  emit stopped();
}

/*!
 * This function can be called by a RunWorker implementation for short-lived
 * tasks to notify its RunControl about this task being successful finished.
 * Dependent startup tasks can proceed, in cases of spontaneous or scheduled
 * stops, the effect is the same as \c reportStopped().
 *
 */
auto RunWorker::reportDone() -> void
{
  d->killStartWatchdog();
  d->killStopWatchdog();
  switch (d->state) {
  case RunWorkerState::Initialized: QTC_CHECK(false);
    d->state = RunWorkerState::Done;
    break;
  case RunWorkerState::Starting:
    reportStarted();
    reportStopped();
    break;
  case RunWorkerState::Running:
  case RunWorkerState::Stopping:
    reportStopped();
    break;
  case RunWorkerState::Done:
    break;
  }
}

/*!
 * This function can be called by a RunWorker implementation to
 * signal a problem in the operation in this worker. The
 * RunControl will start to ramp down through initiateStop().
 */
auto RunWorker::reportFailure(const QString &msg) -> void
{
  d->killStartWatchdog();
  d->killStopWatchdog();
  d->runControl->d->onWorkerFailed(this, msg);
}

/*!
 * Appends a message in the specified \a format to
 * the owning RunControl's \uicontrol{Application Output} pane.
 */
auto RunWorker::appendMessage(const QString &msg, OutputFormat format, bool appendNewLine) -> void
{
  if (!appendNewLine || msg.endsWith('\n')) emit d->runControl->appendMessage(msg, format);
  else emit d->runControl->appendMessage(msg + '\n', format);
}

auto RunWorker::device() const -> IDevice::ConstPtr
{
  return d->runControl->device();
}

auto RunWorker::runnable() const -> const Runnable&
{
  return d->runControl->runnable();
}

auto RunWorker::addStartDependency(RunWorker *dependency) -> void
{
  d->startDependencies.append(dependency);
}

auto RunWorker::addStopDependency(RunWorker *dependency) -> void
{
  d->stopDependencies.append(dependency);
}

auto RunWorker::runControl() const -> RunControl*
{
  return d->runControl;
}

auto RunWorker::setId(const QString &id) -> void
{
  d->id = id;
}

auto RunWorker::setStartTimeout(int ms, const std::function<void()> &callback) -> void
{
  d->startWatchdogInterval = ms;
  d->startWatchdogCallback = callback;
}

auto RunWorker::setStopTimeout(int ms, const std::function<void()> &callback) -> void
{
  d->stopWatchdogInterval = ms;
  d->stopWatchdogCallback = callback;
}

auto RunWorker::recordData(const QString &channel, const QVariant &data) -> void
{
  d->data[channel] = data;
}

auto RunWorker::recordedData(const QString &channel) const -> QVariant
{
  return d->data[channel];
}

auto RunWorker::setSupportsReRunning(bool reRunningSupported) -> void
{
  d->supportsReRunning = reRunningSupported;
}

auto RunWorker::supportsReRunning() const -> bool
{
  return d->supportsReRunning;
}

auto RunWorker::userMessageForProcessError(QProcess::ProcessError error, const FilePath &program) -> QString
{
  const auto failedToStart = tr("The process failed to start.");
  auto msg = tr("An unknown error in the process occurred.");
  switch (error) {
  case QProcess::FailedToStart:
    msg = failedToStart + ' ' + tr("Either the " "invoked program \"%1\" is missing, or you may have insufficient " "permissions to invoke the program.").arg(program.toUserOutput());
    break;
  case QProcess::Crashed:
    msg = tr("The process crashed.");
    break;
  case QProcess::Timedout:
    // "The last waitFor...() function timed out. "
    //   "The state of QProcess is unchanged, and you can try calling "
    // "waitFor...() again."
    return QString(); // sic!
  case QProcess::WriteError:
    msg = tr("An error occurred when attempting to write " "to the process. For example, the process may not be running, " "or it may have closed its input channel.");
    break;
  case QProcess::ReadError:
    msg = tr("An error occurred when attempting to read from " "the process. For example, the process may not be running.");
    break;
  case QProcess::UnknownError:
    break;
  }
  return msg;
}

auto RunWorker::isEssential() const -> bool
{
  return d->essential;
}

auto RunWorker::setEssential(bool essential) -> void
{
  d->essential = essential;
}

auto RunWorker::start() -> void
{
  reportStarted();
}

auto RunWorker::stop() -> void
{
  reportStopped();
}

auto Runnable::displayName() const -> QString
{
  return command.executable().toString();
}

// OutputFormatterFactory

static QList<OutputFormatterFactory*> g_outputFormatterFactories;

OutputFormatterFactory::OutputFormatterFactory()
{
  g_outputFormatterFactories.append(this);
}

OutputFormatterFactory::~OutputFormatterFactory()
{
  g_outputFormatterFactories.removeOne(this);
}

auto OutputFormatterFactory::createFormatters(Target *target) -> QList<OutputLineParser*>
{
  QList<OutputLineParser*> formatters;
  for (const auto factory : qAsConst(g_outputFormatterFactories))
    formatters << factory->m_creator(target);
  return formatters;
}

auto OutputFormatterFactory::setFormatterCreator(const FormatterCreator &creator) -> void
{
  m_creator = creator;
}

} // namespace ProjectExplorer
