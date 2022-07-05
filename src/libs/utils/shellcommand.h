// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "qtcprocess.h"

QT_BEGIN_NAMESPACE
class QMutex;
class QVariant;
template <typename T>
class QFutureInterface;
template <typename T>
class QFuture;
QT_END_NAMESPACE

namespace Utils {

namespace Internal { class ShellCommandPrivate; }

class ORCA_UTILS_EXPORT ProgressParser {
public:
  ProgressParser();
  virtual ~ProgressParser();

protected:
  virtual auto parseProgress(const QString &text) -> void = 0;
  auto setProgressAndMaximum(int value, int maximum) -> void;

private:
  auto setFuture(QFutureInterface<void> *future) -> void;

  QFutureInterface<void> *m_future;
  QMutex *m_futureMutex = nullptr;
  friend class ShellCommand;
};

class ORCA_UTILS_EXPORT ShellCommand : public QObject {
  Q_OBJECT public:
  // Convenience to synchronously run commands
  enum RunFlags {
    ShowStdOut = 0x1,
    // Show standard output.
    MergeOutputChannels = 0x2,
    // see QProcess: Merge stderr/stdout.
    SuppressStdErr = 0x4,
    // Suppress standard error output.
    SuppressFailMessage = 0x8,
    // No message about command failure.
    SuppressCommandLogging = 0x10,
    // No command log entry.
    ShowSuccessMessage = 0x20,
    // Show message about successful completion of command.
    ForceCLocale = 0x40,
    // Force C-locale for commands whose output is parsed.
    FullySynchronously = 0x80,
    // Suppress local event loop (in case UI actions are
    // triggered by file watchers).
    SilentOutput = 0x100,
    // Suppress user notifications about the output happening.
    NoFullySync = 0x200,
    // Avoid fully synchronous execution even in UI thread.
    NoOutput = SuppressStdErr | SuppressFailMessage | SuppressCommandLogging
  };

  ShellCommand(const FilePath &workingDirectory, const Environment &environment);
  ~ShellCommand() override;

  auto displayName() const -> QString;
  auto setDisplayName(const QString &name) -> void;

  auto addJob(const CommandLine &command, const FilePath &workingDirectory = {}, const ExitCodeInterpreter &interpreter = {}) -> void;
  auto addJob(const CommandLine &command, int timeoutS, const FilePath &workingDirectory = {}, const ExitCodeInterpreter &interpreter = {}) -> void;
  auto execute() -> void; // Execute tasks asynchronously!
  auto abort() -> void;
  auto lastExecutionSuccess() const -> bool;
  auto lastExecutionExitCode() const -> int;
  auto defaultWorkingDirectory() const -> const FilePath&;
  virtual auto processEnvironment() const -> const Environment;
  auto defaultTimeoutS() const -> int;
  auto setDefaultTimeoutS(int timeout) -> void;
  auto flags() const -> unsigned;
  auto addFlags(unsigned f) -> void;
  auto cookie() const -> const QVariant&;
  auto setCookie(const QVariant &cookie) -> void;
  auto codec() const -> QTextCodec*;
  auto setCodec(QTextCodec *codec) -> void;
  auto setProgressParser(ProgressParser *parser) -> void;
  auto hasProgressParser() const -> bool;
  auto setProgressiveOutput(bool progressive) -> void;
  auto setDisableUnixTerminal() -> void;

  // This is called once per job in a thread.
  // When called from the UI thread it will execute fully synchronously, so no signals will
  // be triggered!
  virtual auto runCommand(QtcProcess &process, const CommandLine &command, const FilePath &workingDirectory = {}) -> void;

  auto cancel() -> void;

signals:
  auto stdOutText(const QString &) -> void;
  auto stdErrText(const QString &) -> void;
  auto started() -> void;
  auto finished(bool ok, int exitCode, const QVariant &cookie) -> void;
  auto success(const QVariant &cookie) -> void;
  auto terminate() -> void; // Internal
  auto append(const QString &text) -> void;
  auto appendSilently(const QString &text) -> void;
  auto appendError(const QString &text) -> void;
  auto appendCommand(const Utils::FilePath &workingDirectory, const Utils::CommandLine &command) -> void;
  auto appendMessage(const QString &text) -> void;

protected:
  virtual auto addTask(QFuture<void> &future) -> void;
  auto timeoutS() const -> int;
  auto workDirectory(const FilePath &wd) const -> FilePath;

private:
  auto run(QFutureInterface<void> &future) -> void;

  // Run without a event loop in fully blocking mode. No signals will be delivered.
  auto runFullySynchronous(QtcProcess &proc, const FilePath &workingDirectory) -> void;
  // Run with an event loop. Signals will be delivered.
  auto runSynchronous(QtcProcess &proc, const FilePath &workingDirectory) -> void;

  class Internal::ShellCommandPrivate *const d;
};

} // namespace Utils
