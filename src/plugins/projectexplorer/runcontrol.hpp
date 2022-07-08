// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "applicationlauncher.hpp"
#include "buildconfiguration.hpp"
#include "devicesupport/idevice.hpp"
#include "projectexplorerconstants.hpp"
#include "runconfiguration.hpp"

#include <utils/environment.hpp>
#include <utils/processhandle.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/icon.hpp>

#include <QHash>
#include <QVariant>

#include <functional>
#include <memory>

namespace Utils {
class MacroExpander;
class OutputLineParser;
class OutputFormatter;
} // Utils

namespace ProjectExplorer {

class GlobalOrProjectAspect;
class Node;
class RunConfiguration;
class RunControl;
class Target;

namespace Internal {
class RunControlPrivate;
class RunWorkerPrivate;
} // Internal

class PROJECTEXPLORER_EXPORT Runnable {
public:
  Runnable() = default;

  Utils::CommandLine command;
  Utils::FilePath workingDirectory;
  Utils::Environment environment;
  IDevice::ConstPtr device; // Override the kit's device. Keep unset by default.
  QHash<Utils::Id, QVariant> extraData;

  // FIXME: Not necessarily a display name
  auto displayName() const -> QString;
};

class PROJECTEXPLORER_EXPORT RunWorker : public QObject {
  Q_OBJECT

public:
  explicit RunWorker(RunControl *runControl);
  ~RunWorker() override;

  auto runControl() const -> RunControl*;
  auto addStartDependency(RunWorker *dependency) -> void;
  auto addStopDependency(RunWorker *dependency) -> void;
  auto setId(const QString &id) -> void;
  auto setStartTimeout(int ms, const std::function<void()> &callback = {}) -> void;
  auto setStopTimeout(int ms, const std::function<void()> &callback = {}) -> void;
  auto recordData(const QString &channel, const QVariant &data) -> void;
  auto recordedData(const QString &channel) const -> QVariant;
  // Part of read-only interface of RunControl for convenience.
  auto appendMessage(const QString &msg, Utils::OutputFormat format, bool appendNewLine = true) -> void;
  auto device() const -> IDevice::ConstPtr;
  auto runnable() const -> const Runnable&;
  // States
  auto initiateStart() -> void;
  auto reportStarted() -> void;
  auto initiateStop() -> void;
  auto reportStopped() -> void;
  auto reportDone() -> void;
  auto reportFailure(const QString &msg = QString()) -> void;
  auto setSupportsReRunning(bool reRunningSupported) -> void;
  auto supportsReRunning() const -> bool;
  static auto userMessageForProcessError(QProcess::ProcessError, const Utils::FilePath &programName) -> QString;
  auto isEssential() const -> bool;
  auto setEssential(bool essential) -> void;

signals:
  auto started() -> void;
  auto stopped() -> void;

protected:
  virtual auto start() -> void;
  virtual auto stop() -> void;
  virtual auto onFinished() -> void {}

private:
  friend class Internal::RunControlPrivate;
  friend class Internal::RunWorkerPrivate;
  const std::unique_ptr<Internal::RunWorkerPrivate> d;
};

class PROJECTEXPLORER_EXPORT RunWorkerFactory {
public:
  using WorkerCreator = std::function<RunWorker *(RunControl *)>;

  RunWorkerFactory();
  RunWorkerFactory(const WorkerCreator &producer, const QList<Utils::Id> &runModes, const QList<Utils::Id> &runConfigs = {}, const QList<Utils::Id> &deviceTypes = {});

  ~RunWorkerFactory();

  auto canRun(Utils::Id runMode, Utils::Id deviceType, const QString &runConfigId) const -> bool;
  auto producer() const -> WorkerCreator { return m_producer; }

  template <typename Worker>
  static auto make() -> WorkerCreator
  {
    return [](RunControl *runControl) { return new Worker(runControl); };
  }

  // For debugging only.
  static auto dumpAll() -> void;

protected:
  template <typename Worker>
  auto setProduct() -> void { setProducer([](RunControl *rc) { return new Worker(rc); }); }
  auto setProducer(const WorkerCreator &producer) -> void;
  auto addSupportedRunMode(Utils::Id runMode) -> void;
  auto addSupportedRunConfig(Utils::Id runConfig) -> void;
  auto addSupportedDeviceType(Utils::Id deviceType) -> void;

private:
  WorkerCreator m_producer;
  QList<Utils::Id> m_supportedRunModes;
  QList<Utils::Id> m_supportedRunConfigurations;
  QList<Utils::Id> m_supportedDeviceTypes;
};

/**
 * A RunControl controls the running of an application or tool
 * on a target device. It controls start and stop, and handles
 * application output.
 *
 * RunControls are created by RunControlFactories.
 */

class PROJECTEXPLORER_EXPORT RunControl : public QObject {
  Q_OBJECT

public:
  explicit RunControl(Utils::Id mode);
  ~RunControl() override;

  auto setRunConfiguration(RunConfiguration *runConfig) -> void;
  auto setTarget(Target *target) -> void;
  auto setKit(Kit *kit) -> void;
  auto initiateStart() -> void;
  auto initiateReStart() -> void;
  auto initiateStop() -> void;
  auto forceStop() -> void;
  auto initiateFinish() -> void;
  auto promptToStop(bool *optionalPrompt = nullptr) const -> bool;
  auto setPromptToStop(const std::function<bool(bool *)> &promptToStop) -> void;
  auto supportsReRunning() const -> bool;
  virtual auto displayName() const -> QString;
  auto setDisplayName(const QString &displayName) -> void;
  auto isRunning() const -> bool;
  auto isStarting() const -> bool;
  auto isStopping() const -> bool;
  auto isStopped() const -> bool;
  auto setIcon(const Utils::Icon &icon) -> void;
  auto icon() const -> Utils::Icon;
  auto applicationProcessHandle() const -> Utils::ProcessHandle;
  auto setApplicationProcessHandle(const Utils::ProcessHandle &handle) -> void;
  auto device() const -> IDevice::ConstPtr;
  auto runConfiguration() const -> RunConfiguration*; // FIXME: Remove.
  // FIXME: Try to cut down to amount of functions.
  auto target() const -> Target*;
  auto project() const -> Project*;
  auto kit() const -> Kit*;
  auto macroExpander() const -> const Utils::MacroExpander*;
  auto aspect(Utils::Id id) const -> Utils::BaseAspect*;

  template <typename T>
  auto aspect() const -> T*
  {
    return runConfiguration() ? runConfiguration()->aspect<T>() : nullptr;
  }

  auto buildKey() const -> QString;
  auto buildType() const -> BuildConfiguration::BuildType;
  auto buildDirectory() const -> Utils::FilePath;
  auto buildEnvironment() const -> Utils::Environment;
  auto settingsData(Utils::Id id) const -> QVariantMap;
  auto targetFilePath() const -> Utils::FilePath;
  auto projectFilePath() const -> Utils::FilePath;
  auto setupFormatter(Utils::OutputFormatter *formatter) const -> void;
  auto runMode() const -> Utils::Id;
  auto runnable() const -> const Runnable&;
  auto setRunnable(const Runnable &runnable) -> void;
  static auto showPromptToStopDialog(const QString &title, const QString &text, const QString &stopButtonText = QString(), const QString &cancelButtonText = QString(), bool *prompt = nullptr) -> bool;
  static auto provideAskPassEntry(Utils::Environment &env) -> void;
  auto createWorker(Utils::Id workerId) -> RunWorker*;
  auto createMainWorker() -> bool;
  static auto canRun(Utils::Id runMode, Utils::Id deviceType, Utils::Id runConfigId) -> bool;

signals:
  auto appendMessage(const QString &msg, Utils::OutputFormat format) -> void;
  auto aboutToStart() -> void;
  auto started() -> void;
  auto stopped() -> void;
  auto finished() -> void;
  auto applicationProcessHandleChanged(QPrivateSignal) -> void; // Use setApplicationProcessHandle

private:
  auto setDevice(const IDevice::ConstPtr &device) -> void;

  friend class RunWorker;
  friend class Internal::RunWorkerPrivate;

  const std::unique_ptr<Internal::RunControlPrivate> d;
};

/**
 * A simple TargetRunner for cases where a plain ApplicationLauncher is
 * sufficient for running purposes.
 */

class PROJECTEXPLORER_EXPORT SimpleTargetRunner : public RunWorker {
  Q_OBJECT

public:
  explicit SimpleTargetRunner(RunControl *runControl);

protected:
  auto setStarter(const std::function<void()> &starter) -> void;
  auto doStart(const Runnable &runnable, const IDevice::ConstPtr &device) -> void;

private:
  auto start() -> void final;
  auto stop() -> void final;
  auto runnable() const -> const Runnable& = delete;

  ApplicationLauncher m_launcher;
  std::function<void()> m_starter;

  bool m_stopReported = false;
  bool m_useTerminal = false;
  bool m_runAsRoot = false;
  bool m_stopForced = false;
};

class PROJECTEXPLORER_EXPORT OutputFormatterFactory {
protected:
  OutputFormatterFactory();

public:
  virtual ~OutputFormatterFactory();
  static auto createFormatters(Target *target) -> QList<Utils::OutputLineParser*>;

protected:
  using FormatterCreator = std::function<QList<Utils::OutputLineParser*>(Target *)>;
  auto setFormatterCreator(const FormatterCreator &creator) -> void;

private:
  FormatterCreator m_creator;
};

} // namespace ProjectExplorer
