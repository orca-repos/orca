// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "shellcommand.hpp"

#include "environment.hpp"
#include "fileutils.hpp"
#include "qtcassert.hpp"
#include "runextensions.hpp"

#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QStringList>
#include <QTextCodec>
#include <QThread>
#include <QVariant>

#include <numeric>

/*!
    \fn void Utils::ProgressParser::parseProgress(const QString &text)

    Reimplement to parse progress as it appears in the standard output.
    If a progress string is detected, call \c setProgressAndMaximum() to update
    the progress bar accordingly.

    \sa Utils::ProgressParser::setProgressAndMaximum()
*/

/*!
    \fn void Utils::ProgressParser::setProgressAndMaximum(int value, int maximum)

    Sets progress \a value and \a maximum for current command. Called by \c parseProgress()
    when a progress string is detected.
*/

namespace Utils {
namespace Internal {

class ShellCommandPrivate {
public:
  struct Job {
    explicit Job(const FilePath &wd, const CommandLine &command, int t, const ExitCodeInterpreter &interpreter);

    FilePath workingDirectory;
    CommandLine command;
    ExitCodeInterpreter exitCodeInterpreter;
    int timeoutS;
  };

  ShellCommandPrivate(const FilePath &defaultWorkingDirectory, const Environment &environment) : m_defaultWorkingDirectory(defaultWorkingDirectory), m_environment(environment) {}
  ~ShellCommandPrivate() { delete m_progressParser; }

  QString m_displayName;
  const FilePath m_defaultWorkingDirectory;
  const Environment m_environment;
  QVariant m_cookie;
  QTextCodec *m_codec = nullptr;
  ProgressParser *m_progressParser = nullptr;
  QFutureWatcher<void> m_watcher;
  QList<Job> m_jobs;

  unsigned m_flags = 0;
  int m_defaultTimeoutS = 10;
  int m_lastExecExitCode = -1;
  bool m_lastExecSuccess = false;
  bool m_progressiveOutput = false;
  bool m_hadOutput = false;
  bool m_aborted = false;
  bool m_disableUnixTerminal = false;
};

ShellCommandPrivate::Job::Job(const FilePath &wd, const CommandLine &command, int t, const ExitCodeInterpreter &interpreter) : workingDirectory(wd), command(command), exitCodeInterpreter(interpreter), timeoutS(t)
{
  // Finished cookie is emitted via queued slot, needs metatype
  static const int qvMetaId = qRegisterMetaType<QVariant>();
  Q_UNUSED(qvMetaId)
}

} // namespace Internal

ShellCommand::ShellCommand(const FilePath &workingDirectory, const Environment &environment) : d(new Internal::ShellCommandPrivate(workingDirectory, environment))
{
  connect(&d->m_watcher, &QFutureWatcher<void>::canceled, this, &ShellCommand::cancel);
}

ShellCommand::~ShellCommand()
{
  delete d;
}

auto ShellCommand::displayName() const -> QString
{
  if (!d->m_displayName.isEmpty())
    return d->m_displayName;
  if (!d->m_jobs.isEmpty()) {
    const Internal::ShellCommandPrivate::Job &job = d->m_jobs.at(0);
    QString result = job.command.executable().baseName();
    if (!result.isEmpty())
      result[0] = result.at(0).toTitleCase();
    else
      result = tr("UNKNOWN");

    if (!job.command.arguments().isEmpty())
      result += ' ' + job.command.splitArguments().at(0);

    return result;
  }
  return tr("Unknown");
}

auto ShellCommand::setDisplayName(const QString &name) -> void
{
  d->m_displayName = name;
}

auto ShellCommand::defaultWorkingDirectory() const -> const FilePath&
{
  return d->m_defaultWorkingDirectory;
}

auto ShellCommand::processEnvironment() const -> const Environment
{
  return d->m_environment;
}

auto ShellCommand::defaultTimeoutS() const -> int
{
  return d->m_defaultTimeoutS;
}

auto ShellCommand::setDefaultTimeoutS(int timeout) -> void
{
  d->m_defaultTimeoutS = timeout;
}

auto ShellCommand::flags() const -> unsigned
{
  return d->m_flags;
}

auto ShellCommand::addFlags(unsigned f) -> void
{
  d->m_flags |= f;
}

auto ShellCommand::addJob(const CommandLine &command, const FilePath &workingDirectory, const ExitCodeInterpreter &interpreter) -> void
{
  addJob(command, defaultTimeoutS(), workingDirectory, interpreter);
}

auto ShellCommand::addJob(const CommandLine &command, int timeoutS, const FilePath &workingDirectory, const ExitCodeInterpreter &interpreter) -> void
{
  d->m_jobs.push_back(Internal::ShellCommandPrivate::Job(workDirectory(workingDirectory), command, timeoutS, interpreter));
}

auto ShellCommand::execute() -> void
{
  d->m_lastExecSuccess = false;
  d->m_lastExecExitCode = -1;

  if (d->m_jobs.empty())
    return;

  QFuture<void> task = Utils::runAsync(&ShellCommand::run, this);
  d->m_watcher.setFuture(task);
  if (!(d->m_flags & SuppressCommandLogging))
    addTask(task);
}

auto ShellCommand::abort() -> void
{
  d->m_aborted = true;
  d->m_watcher.future().cancel();
}

auto ShellCommand::cancel() -> void
{
  emit terminate();
}

auto ShellCommand::addTask(QFuture<void> &future) -> void
{
  Q_UNUSED(future)
}

auto ShellCommand::timeoutS() const -> int
{
  return std::accumulate(d->m_jobs.cbegin(), d->m_jobs.cend(), 0, [](int sum, const Internal::ShellCommandPrivate::Job &job) {
    return sum + job.timeoutS;
  });
}

auto ShellCommand::workDirectory(const FilePath &wd) const -> FilePath
{
  if (!wd.isEmpty())
    return wd;
  return defaultWorkingDirectory();
}

auto ShellCommand::lastExecutionSuccess() const -> bool
{
  return d->m_lastExecSuccess;
}

auto ShellCommand::lastExecutionExitCode() const -> int
{
  return d->m_lastExecExitCode;
}

auto ShellCommand::run(QFutureInterface<void> &future) -> void
{
  // Check that the binary path is not empty
  QTC_ASSERT(!d->m_jobs.isEmpty(), return);

  QString stdOut;
  QString stdErr;

  emit started();
  if (d->m_progressParser)
    d->m_progressParser->setFuture(&future);
  else
    future.setProgressRange(0, 1);
  const int count = d->m_jobs.size();
  d->m_lastExecExitCode = -1;
  d->m_lastExecSuccess = true;
  for (int j = 0; j < count; j++) {
    const Internal::ShellCommandPrivate::Job &job = d->m_jobs.at(j);
    QtcProcess proc;
    proc.setExitCodeInterpreter(job.exitCodeInterpreter);
    proc.setTimeoutS(job.timeoutS);
    runCommand(proc, job.command, job.workingDirectory);
    stdOut += proc.stdOut();
    stdErr += proc.stdErr();
    d->m_lastExecExitCode = proc.exitCode();
    d->m_lastExecSuccess = proc.result() == QtcProcess::FinishedWithSuccess;
    if (!d->m_lastExecSuccess)
      break;
  }

  if (!d->m_aborted) {
    if (!d->m_progressiveOutput) {
      emit stdOutText(stdOut);
      if (!stdErr.isEmpty()) emit stdErrText(stdErr);
    }

    emit finished(d->m_lastExecSuccess, d->m_lastExecExitCode, cookie());
    if (d->m_lastExecSuccess) {
      emit success(cookie());
      future.setProgressValue(future.progressMaximum());
    } else {
      future.cancel(); // sets the progress indicator red
    }
  }

  if (d->m_progressParser)
    d->m_progressParser->setFuture(nullptr);
  // As it is used asynchronously, we need to delete ourselves
  this->deleteLater();
}

auto ShellCommand::runCommand(QtcProcess &proc, const CommandLine &command, const FilePath &workingDirectory) -> void
{
  const FilePath dir = workDirectory(workingDirectory);

  if (command.executable().isEmpty()) {
    proc.setResult(QtcProcess::StartFailed);
    return;
  }

  if (!(d->m_flags & SuppressCommandLogging)) emit appendCommand(dir, command);

  proc.setCommand(command);
  if ((d->m_flags & FullySynchronously) || (!(d->m_flags & NoFullySync) && QThread::currentThread() == QCoreApplication::instance()->thread())) {
    runFullySynchronous(proc, dir);
  } else {
    runSynchronous(proc, dir);
  }

  if (!d->m_aborted) {
    // Success/Fail message in appropriate window?
    if (proc.result() == QtcProcess::FinishedWithSuccess) {
      if (d->m_flags & ShowSuccessMessage) emit appendMessage(proc.exitMessage());
    } else if (!(d->m_flags & SuppressFailMessage)) {
      emit appendError(proc.exitMessage());
    }
  }
}

auto ShellCommand::runFullySynchronous(QtcProcess &process, const FilePath &workingDirectory) -> void
{
  // Set up process
  if (d->m_disableUnixTerminal)
    process.setDisableUnixTerminal();
  const FilePath dir = workDirectory(workingDirectory);
  if (!dir.isEmpty())
    process.setWorkingDirectory(dir);
  process.setEnvironment(processEnvironment());
  if (d->m_flags & MergeOutputChannels)
    process.setProcessChannelMode(QProcess::MergedChannels);
  if (d->m_codec)
    process.setCodec(d->m_codec);

  process.runBlocking();

  if (!d->m_aborted) {
    const QString stdErr = process.stdErr();
    if (!stdErr.isEmpty() && !(d->m_flags & SuppressStdErr)) emit append(stdErr);

    const QString stdOut = process.stdOut();
    if (!stdOut.isEmpty() && d->m_flags & ShowStdOut) {
      if (d->m_flags & SilentOutput) emit appendSilently(stdOut);
      else emit append(stdOut);
    }
  }
}

auto ShellCommand::runSynchronous(QtcProcess &process, const FilePath &workingDirectory) -> void
{
  connect(this, &ShellCommand::terminate, &process, &QtcProcess::stopProcess);
  process.setEnvironment(processEnvironment());
  if (d->m_codec)
    process.setCodec(d->m_codec);
  if (d->m_disableUnixTerminal)
    process.setDisableUnixTerminal();
  const FilePath dir = workDirectory(workingDirectory);
  if (!dir.isEmpty())
    process.setWorkingDirectory(dir);
  // connect stderr to the output window if desired
  if (d->m_flags & MergeOutputChannels) {
    process.setProcessChannelMode(QProcess::MergedChannels);
  } else if (d->m_progressiveOutput || !(d->m_flags & SuppressStdErr)) {
    process.setStdErrCallback([this](const QString &text) {
      if (d->m_progressParser)
        d->m_progressParser->parseProgress(text);
      if (!(d->m_flags & SuppressStdErr)) emit appendError(text);
      if (d->m_progressiveOutput) emit stdErrText(text);
    });
  }

  // connect stdout to the output window if desired
  if (d->m_progressParser || d->m_progressiveOutput || (d->m_flags & ShowStdOut)) {
    process.setStdOutCallback([this](const QString &text) {
      if (d->m_progressParser)
        d->m_progressParser->parseProgress(text);
      if (d->m_flags & ShowStdOut) emit append(text);
      if (d->m_progressiveOutput) {
        emit stdOutText(text);
        d->m_hadOutput = true;
      }
    });
  }

  process.setTimeOutMessageBoxEnabled(true);

  if (d->m_codec)
    process.setCodec(d->m_codec);

  process.runBlocking(QtcProcess::WithEventLoop);
}

auto ShellCommand::cookie() const -> const QVariant&
{
  return d->m_cookie;
}

auto ShellCommand::setCookie(const QVariant &cookie) -> void
{
  d->m_cookie = cookie;
}

auto ShellCommand::codec() const -> QTextCodec*
{
  return d->m_codec;
}

auto ShellCommand::setCodec(QTextCodec *codec) -> void
{
  d->m_codec = codec;
}

//! Use \a parser to parse progress data from stdout. Command takes ownership of \a parser
auto ShellCommand::setProgressParser(ProgressParser *parser) -> void
{
  QTC_ASSERT(!d->m_progressParser, return);
  d->m_progressParser = parser;
}

auto ShellCommand::hasProgressParser() const -> bool
{
  return d->m_progressParser;
}

auto ShellCommand::setProgressiveOutput(bool progressive) -> void
{
  d->m_progressiveOutput = progressive;
}

auto ShellCommand::setDisableUnixTerminal() -> void
{
  d->m_disableUnixTerminal = true;
}

ProgressParser::ProgressParser() : m_futureMutex(new QMutex) { }

ProgressParser::~ProgressParser()
{
  delete m_futureMutex;
}

auto ProgressParser::setProgressAndMaximum(int value, int maximum) -> void
{
  QMutexLocker lock(m_futureMutex);
  if (!m_future)
    return;
  m_future->setProgressRange(0, maximum);
  m_future->setProgressValue(value);
}

auto ProgressParser::setFuture(QFutureInterface<void> *future) -> void
{
  QMutexLocker lock(m_futureMutex);
  m_future = future;
}

} // namespace Utils
