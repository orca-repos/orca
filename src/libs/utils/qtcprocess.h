// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "environment.h"
#include "commandline.h"
#include "processutils.h"

#include <QProcess>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QDebug)
QT_FORWARD_DECLARE_CLASS(QTextCodec)

class tst_QtcProcess;

namespace Utils {

class CommandLine;
class Environment;
class QtcProcess;

namespace Internal {
class QtcProcessPrivate;
}

class DeviceProcessHooks {
public:
  std::function<void(QtcProcess &)> startProcessHook;
  std::function<Environment(const FilePath &)> systemEnvironmentForBinary;
};

class ORCA_UTILS_EXPORT QtcProcess : public QObject {
  Q_OBJECT

public:
  enum ProcessImpl {
    QProcessImpl,
    ProcessLauncherImpl,
    DefaultImpl,
  };

  enum TerminalMode {
    TerminalOff,
    TerminalRun,
    TerminalDebug,
    TerminalSuspend,
    TerminalOn = TerminalRun // default mode for ON
  };

  struct Setup {
    Setup() {}
    Setup(ProcessImpl processImpl) : processImpl(processImpl) {}
    Setup(ProcessMode processMode) : processMode(processMode) {}
    Setup(TerminalMode terminalMode) : terminalMode(terminalMode) {}

    ProcessImpl processImpl = DefaultImpl;
    ProcessMode processMode = ProcessMode::Reader;
    TerminalMode terminalMode = TerminalOff;
  };

  QtcProcess(const Setup &setup = {}, QObject *parent = nullptr);
  QtcProcess(QObject *parent);
  ~QtcProcess() override;

  auto processMode() const -> ProcessMode;
  auto terminalMode() const -> TerminalMode;

  enum Result {
    // Finished successfully. Unless an ExitCodeInterpreter is set
    // this corresponds to a return code 0.
    FinishedWithSuccess,
    Finished = FinishedWithSuccess,
    // FIXME: Kept to ease downstream transition
    // Finished unsuccessfully. Unless an ExitCodeInterpreter is set
    // this corresponds to a return code different from 0.
    FinishedWithError,
    FinishedError = FinishedWithError,
    // FIXME: Kept to ease downstream transition
    // Process terminated abnormally (kill)
    TerminatedAbnormally,
    // Executable could not be started
    StartFailed,
    // Hang, no output after time out
    Hang
  };

  auto setEnvironment(const Environment &env) -> void;
  auto unsetEnvironment() -> void;
  auto environment() const -> const Environment&;
  auto hasEnvironment() const -> bool;
  auto setCommand(const CommandLine &cmdLine) -> void;
  auto commandLine() const -> const CommandLine&;
  auto workingDirectory() const -> FilePath;
  auto setWorkingDirectory(const FilePath &dir) -> void;
  auto setUseCtrlCStub(bool enabled) -> void;
  auto setLowPriority() -> void;
  auto setDisableUnixTerminal() -> void;
  auto setRunAsRoot(bool on) -> void;
  auto isRunAsRoot() const -> bool;
  auto setAbortOnMetaChars(bool abort) -> void;
  auto start() -> void;
  virtual auto terminate() -> void;
  virtual auto interrupt() -> void;

  static auto startDetached(const CommandLine &cmd, const FilePath &workingDirectory = {}, qint64 *pid = nullptr) -> bool;

  enum EventLoopMode {
    NoEventLoop,
    WithEventLoop // Avoid
  };

  // Starts the command and waits for finish.
  // User input processing is enabled when WithEventLoop was passed.
  auto runBlocking(EventLoopMode eventLoopMode = NoEventLoop) -> void;

  /* Timeout for hanging processes (triggers after no more output
   * occurs on stderr/stdout). */
  auto setTimeoutS(int timeoutS) -> void;

  auto setCodec(QTextCodec *c) -> void;
  auto setTimeOutMessageBoxEnabled(bool) -> void;
  auto setExitCodeInterpreter(const std::function<QtcProcess::Result(int)> &interpreter) -> void;
  auto setWriteData(const QByteArray &writeData) -> void;
  auto setStdOutCallback(const std::function<void(const QString &)> &callback) -> void;
  auto setStdOutLineCallback(const std::function<void(const QString &)> &callback) -> void;
  auto setStdErrCallback(const std::function<void(const QString &)> &callback) -> void;
  auto setStdErrLineCallback(const std::function<void(const QString &)> &callback) -> void;
  static auto setRemoteProcessHooks(const DeviceProcessHooks &hooks) -> void;
  auto stopProcess() -> bool;
  auto readDataFromProcess(int timeoutS, QByteArray *stdOut, QByteArray *stdErr, bool showTimeOutMessageBox) -> bool;
  static auto normalizeNewlines(const QString &text) -> QString;
  auto result() const -> Result;
  auto setResult(Result result) -> void;
  auto allRawOutput() const -> QByteArray;
  auto allOutput() const -> QString;
  auto stdOut() const -> QString;
  auto stdErr() const -> QString;
  auto rawStdOut() const -> QByteArray;
  virtual auto exitCode() const -> int;
  auto exitMessage() -> QString;
  // Helpers to find binaries. Do not use it for other path variables
  // and file types.
  static auto locateBinary(const QString &binary) -> QString;
  static auto locateBinary(const QString &path, const QString &binary) -> QString;
  static auto systemEnvironmentForBinary(const FilePath &filePath) -> Environment;
  auto kickoffProcess() -> void;
  auto interruptProcess() -> void;
  auto applicationMainThreadID() const -> qint64;
  // FIXME: Cut down the following bits inherited from QProcess and QIODevice.
  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void;
  auto error() const -> QProcess::ProcessError;
  virtual auto state() const -> QProcess::ProcessState;
  auto isRunning() const -> bool; // Short for state() == QProcess::Running.
  virtual auto errorString() const -> QString;
  auto setErrorString(const QString &str) -> void;
  auto processId() const -> qint64;
  auto waitForStarted(int msecs = 30000) -> bool;
  auto waitForReadyRead(int msecs = 30000) -> bool;
  auto waitForFinished(int msecs = 30000) -> bool;
  virtual auto readAllStandardOutput() -> QByteArray;
  virtual auto readAllStandardError() -> QByteArray;
  virtual auto exitStatus() const -> QProcess::ExitStatus;
  virtual auto kill() -> void;
  virtual auto write(const QByteArray &input) -> qint64;
  auto close() -> void;
  auto setStandardInputFile(const QString &inputFile) -> void;
  auto toStandaloneCommandLine() const -> QString;

signals:
  auto started() -> void;
  auto finished() -> void;
  auto errorOccurred(QProcess::ProcessError error) -> void;
  auto readyReadStandardOutput() -> void;
  auto readyReadStandardError() -> void;

private:
  friend ORCA_UTILS_EXPORT auto operator<<(QDebug str, const QtcProcess &r) -> QDebug;

  Internal::QtcProcessPrivate *d = nullptr;

  friend tst_QtcProcess;
  auto beginFeed() -> void;
  auto feedStdOut(const QByteArray &data) -> void;
  auto endFeed() -> void;
};

using ExitCodeInterpreter = std::function<QtcProcess::Result(int /*exitCode*/)>;

} // namespace Utils
