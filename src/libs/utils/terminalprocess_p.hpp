// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtcprocess.hpp"

namespace Utils {

class CommandLine;
class Environment;
class FilePath;

namespace Internal {

class TerminalProcess : public QObject {
  Q_OBJECT

public:
  explicit TerminalProcess(QObject *parent, QtcProcess::ProcessImpl processImpl, QtcProcess::TerminalMode terminalMode);
  ~TerminalProcess() override;

  auto setCommand(const CommandLine &command) -> void;
  auto commandLine() const -> const CommandLine&;
  auto setWorkingDirectory(const FilePath &dir) -> void;
  auto workingDirectory() const -> FilePath;
  auto setEnvironment(const Environment &env) -> void;
  auto environment() const -> const Environment&;
  auto error() const -> QProcess::ProcessError;
  auto errorString() const -> QString;
  auto start() -> void;
  auto stopProcess() -> void;

  // OK, however, impl looks a bit different (!= NotRunning vs == Running).
  // Most probably changing it into (== Running) should be OK.
  auto isRunning() const -> bool;
  auto state() const -> QProcess::ProcessState;
  auto processId() const -> qint64;
  auto exitCode() const -> int;
  auto exitStatus() const -> QProcess::ExitStatus;
  auto setAbortOnMetaChars(bool abort) -> void;   // used only in sshDeviceProcess
  auto kickoffProcess() -> void;                  // only debugger terminal, only non-windows
  auto interruptProcess() -> void;                // only debugger terminal, only non-windows
  auto applicationMainThreadID() const -> qint64; // only debugger terminal, only windows (-1 otherwise)

signals:
  auto started() -> void;
  auto finished(int exitCode, QProcess::ExitStatus status) -> void;
  auto errorOccurred(QProcess::ProcessError error) -> void;

private:
  auto stubConnectionAvailable() -> void;
  auto readStubOutput() -> void;
  auto stubExited() -> void;
  auto cleanupAfterStartFailure(const QString &errorMessage) -> void;
  auto finish(int exitCode, QProcess::ExitStatus exitStatus) -> void;
  auto killProcess() -> void;
  auto killStub() -> void;
  auto emitError(QProcess::ProcessError err, const QString &errorString) -> void;
  auto stubServerListen() -> QString;
  auto stubServerShutdown() -> void;
  auto cleanupStub() -> void;
  auto cleanupInferior() -> void;

  class TerminalProcessPrivate *d;
};

} // Internal
} // Utils
