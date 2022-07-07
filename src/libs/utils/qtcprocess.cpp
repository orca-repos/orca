// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtcprocess.hpp"

#include "commandline.hpp"
#include "executeondestruction.hpp"
#include "hostosinfo.hpp"
#include "launcherinterface.hpp"
#include "launcherpackets.hpp"
#include "launchersocket.hpp"
#include "processreaper.hpp"
#include "qtcassert.hpp"
#include "stringutils.hpp"
#include "terminalprocess_p.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTextCodec>
#include <QThread>
#include <QTimer>

#ifdef QT_GUI_LIB
// qmlpuppet does not use that.
#include <QApplication>
#include <QMessageBox>
#endif

#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>

#ifdef Q_OS_WIN
#ifdef ORCA_PCH_H
#define CALLBACK WINAPI
#endif
#include <qt_windows.h>
#endif

using namespace Utils::Internal;

namespace Utils {
namespace Internal {

class MeasureAndRun {
public:
  MeasureAndRun(const char *functionName) : m_functionName(functionName), m_measureProcess(qEnvironmentVariableIsSet("QTC_MEASURE_PROCESS")) {}

  template <typename Function, typename... Args>
  auto measureAndRun(Function &&function, Args &&... args) -> std::invoke_result_t<Function, Args...>
  {
    if (!m_measureProcess)
      return std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
    QElapsedTimer timer;
    timer.start();
    auto cleanup = qScopeGuard([this, &timer] {
      const qint64 currentNsecs = timer.nsecsElapsed();
      const bool isMainThread = QThread::currentThread() == qApp->thread();
      const int hitThisAll = m_hitThisAll.fetch_add(1) + 1;
      const int hitAllAll = m_hitAllAll.fetch_add(1) + 1;
      const int hitThisMain = isMainThread ? m_hitThisMain.fetch_add(1) + 1 : m_hitThisMain.load();
      const int hitAllMain = isMainThread ? m_hitAllMain.fetch_add(1) + 1 : m_hitAllMain.load();
      const qint64 totalThisAll = toMs(m_totalThisAll.fetch_add(currentNsecs) + currentNsecs);
      const qint64 totalAllAll = toMs(m_totalAllAll.fetch_add(currentNsecs) + currentNsecs);
      const qint64 totalThisMain = toMs(isMainThread ? m_totalThisMain.fetch_add(currentNsecs) + currentNsecs : m_totalThisMain.load());
      const qint64 totalAllMain = toMs(isMainThread ? m_totalAllMain.fetch_add(currentNsecs) + currentNsecs : m_totalAllMain.load());
      printMeasurement(QLatin1String(m_functionName), hitThisAll, toMs(currentNsecs), totalThisAll, hitAllAll, totalAllAll, isMainThread, hitThisMain, totalThisMain, hitAllMain, totalAllMain);
    });
    return std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
  }
private:
  static auto printHeader() -> void
  {
    // [function/thread]: function:(T)his|(A)ll, thread:(M)ain|(A)ll
    qDebug() << "+----------------+-------+---------+----------+-------+----------+---------+-------+----------+-------+----------+";
    qDebug() << "| [Function/Thread] = [(T|A)/(M|A)], where: (T)his function, (A)ll functions / threads, (M)ain thread            |";
    qDebug() << "+----------------+-------+---------+----------+-------+----------+---------+-------+----------+-------+----------+";
    qDebug() << "|              1 |     2 |       3 |        4 |     5 |        6 |       7 |     8 |        9 |    10 |       11 |";
    qDebug() << "|                | [T/A] |   [T/A] |    [T/A] | [A/A] |    [A/A] |         | [T/M] |    [T/M] | [A/M] |    [A/M] |";
    qDebug() << "|       Function |   Hit | Current |    Total |   Hit |    Total | Current |   Hit |    Total |   Hit |    Total |";
    qDebug() << "|           Name | Count |  Measu- |   Measu- | Count |   Measu- | is Main | Count |   Measu- | Count |   Measu- |";
    qDebug() << "|                |       |  rement |   rement |       |   rement |  Thread |       |   rement |       |   rement |";
    qDebug() << "+----------------+-------+---------+----------+-------+----------+---------+-------+----------+-------+----------+";
  }

  static auto printMeasurement(const QString &functionName, int hitThisAll, int currentNsecs, int totalThisAll, int hitAllAll, int totalAllAll, bool isMainThread, int hitThisMain, int totalThisMain, int hitAllMain, int totalAllMain) -> void
  {
    static const int repeatHeaderLineCount = 25;
    if (s_lineCounter.fetch_add(1) % repeatHeaderLineCount == 0)
      printHeader();

    const QString &functionNameField = QString("%1").arg(functionName, 14);
    const QString &hitThisAllField = formatField(hitThisAll, 5);
    const QString &currentNsecsField = formatField(currentNsecs, 7, " ms");
    const QString &totalThisAllField = formatField(totalThisAll, 8, " ms");
    const QString &hitAllAllField = formatField(hitAllAll, 5);
    const QString &totalAllAllField = formatField(totalAllAll, 8, " ms");
    const QString &mainThreadField = isMainThread ? QString("%1").arg("yes", 7) : QString("%1").arg("no", 7);
    const QString &hitThisMainField = formatField(hitThisMain, 5);
    const QString &totalThisMainField = formatField(totalThisMain, 8, " ms");
    const QString &hitAllMainField = formatField(hitAllMain, 5);
    const QString &totalAllMainField = formatField(totalAllMain, 8, " ms");

    const QString &totalString = QString("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 | %11 |").arg(functionNameField, hitThisAllField, currentNsecsField, totalThisAllField, hitAllAllField, totalAllAllField, mainThreadField, hitThisMainField, totalThisMainField, hitAllMainField, totalAllMainField);
    qDebug("%s", qPrintable(totalString));
  }

  static auto formatField(int number, int fieldWidth, const QString &suffix = QString()) -> QString
  {
    return QString("%1%2").arg(number, fieldWidth - suffix.count()).arg(suffix);
  }

  static auto toMs(quint64 nsesc) -> int // nanoseconds to miliseconds
    
  {
    static const int halfMillion = 500000;
    static const int million = 2 * halfMillion;
    return int((nsesc + halfMillion) / million);
  }

  const char *const m_functionName;
  const bool m_measureProcess;
  std::atomic_int m_hitThisAll = 0;
  std::atomic_int m_hitThisMain = 0;
  std::atomic_int64_t m_totalThisAll = 0;
  std::atomic_int64_t m_totalThisMain = 0;
  static std::atomic_int m_hitAllAll;
  static std::atomic_int m_hitAllMain;
  static std::atomic_int64_t m_totalAllAll;
  static std::atomic_int64_t m_totalAllMain;
  static std::atomic_int s_lineCounter;
};

std::atomic_int MeasureAndRun::m_hitAllAll = 0;
std::atomic_int MeasureAndRun::m_hitAllMain = 0;
std::atomic_int64_t MeasureAndRun::m_totalAllAll = 0;
std::atomic_int64_t MeasureAndRun::m_totalAllMain = 0;
std::atomic_int MeasureAndRun::s_lineCounter = 0;

static MeasureAndRun s_start = MeasureAndRun("start");
static MeasureAndRun s_waitForStarted = MeasureAndRun("waitForStarted");

enum {
  debug = 0
};

enum {
  syncDebug = 0
};

enum {
  defaultMaxHangTimerCount = 10
};

static Q_LOGGING_CATEGORY(processLog, "qtc.utils.qtcprocess", QtWarningMsg)

static DeviceProcessHooks s_deviceHooks;

// Data for one channel buffer (stderr/stdout)
class ChannelBuffer {
public:
  auto clearForRun() -> void;
  auto handleRest() -> void;
  auto append(const QByteArray &text) -> void;

  QByteArray rawData;
  QString incompleteLineBuffer; // lines not yet signaled
  QTextCodec *codec = nullptr;  // Not owner
  std::unique_ptr<QTextCodec::ConverterState> codecState;
  std::function<void(const QString &lines)> outputCallback;

  bool emitSingleLines = true;
  bool keepRawData = true;
};

class ProcessInterface : public QObject {
  Q_OBJECT public:
  ProcessInterface(QObject *parent, ProcessMode processMode) : QObject(parent), m_processMode(processMode) {}

  virtual auto readAllStandardOutput() -> QByteArray = 0;
  virtual auto readAllStandardError() -> QByteArray = 0;
  virtual auto setProcessEnvironment(const QProcessEnvironment &environment) -> void = 0;
  virtual auto setWorkingDirectory(const QString &dir) -> void = 0;
  virtual auto start(const QString &program, const QStringList &arguments, const QByteArray &writeData) -> void = 0;

  virtual auto customStart(const CommandLine &, const FilePath &workingDirectory, const Environment &) -> void
  {
    Q_UNUSED(workingDirectory);
    QTC_CHECK(false);
  }

  virtual auto isCustomStart() const -> bool { return false; }
  virtual auto terminate() -> void = 0;
  virtual auto kill() -> void = 0;
  virtual auto close() -> void = 0;
  virtual auto write(const QByteArray &data) -> qint64 = 0;
  virtual auto setStandardInputFile(const QString &fileName) -> void = 0;
  virtual auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void = 0;
  virtual auto program() const -> QString = 0;
  virtual auto error() const -> QProcess::ProcessError = 0;
  virtual auto state() const -> QProcess::ProcessState = 0;
  virtual auto processId() const -> qint64 = 0;
  virtual auto exitCode() const -> int = 0;
  virtual auto exitStatus() const -> QProcess::ExitStatus = 0;
  virtual auto errorString() const -> QString = 0;
  virtual auto setErrorString(const QString &str) -> void = 0;
  virtual auto waitForStarted(int msecs) -> bool = 0;
  virtual auto waitForReadyRead(int msecs) -> bool = 0;
  virtual auto waitForFinished(int msecs) -> bool = 0;

  virtual auto kickoffProcess() -> void
  {
    QTC_CHECK(false);
  }

  virtual auto interruptProcess() -> void
  {
    QTC_CHECK(false);
  }

  virtual auto applicationMainThreadID() const -> qint64
  {
    QTC_CHECK(false);
    return -1;
  }

  auto setLowPriority() -> void { m_lowPriority = true; }
  auto isLowPriority() const -> bool { return m_lowPriority; }
  auto setUnixTerminalDisabled() -> void { m_unixTerminalDisabled = true; }
  auto isUnixTerminalDisabled() const -> bool { return m_unixTerminalDisabled; }
  auto setAbortOnMetaChars(bool abort) -> void { m_abortOnMetaChars = abort; }
  auto isAbortOnMetaChars() const -> bool { return m_abortOnMetaChars; }
  auto setBelowNormalPriority() -> void { m_belowNormalPriority = true; }
  auto isBelowNormalPriority() const -> bool { return m_belowNormalPriority; }
  auto setNativeArguments(const QString &arguments) -> void { m_nativeArguments = arguments; }
  auto nativeArguments() const -> QString { return m_nativeArguments; }

signals:
  auto started() -> void;
  auto finished(int exitCode, QProcess::ExitStatus status) -> void;
  auto errorOccurred(QProcess::ProcessError error) -> void;
  auto readyReadStandardOutput() -> void;
  auto readyReadStandardError() -> void;

protected:
  auto processMode() const -> ProcessMode { return m_processMode; }

private:
  const ProcessMode m_processMode;
  bool m_belowNormalPriority = false;
  QString m_nativeArguments;
  bool m_lowPriority = false;
  bool m_unixTerminalDisabled = false;
  bool m_abortOnMetaChars = true;
};

class TerminalImpl : public ProcessInterface {
public:
  TerminalImpl(QObject *parent, QtcProcess::ProcessImpl processImpl, QtcProcess::TerminalMode terminalMode) : ProcessInterface(parent, ProcessMode::Reader), m_terminal(this, processImpl, terminalMode)
  {
    connect(&m_terminal, &Internal::TerminalProcess::started, this, &ProcessInterface::started);
    connect(&m_terminal, &Internal::TerminalProcess::finished, this, &ProcessInterface::finished);
    connect(&m_terminal, &Internal::TerminalProcess::errorOccurred, this, &ProcessInterface::errorOccurred);
  }

  ~TerminalImpl() override { }

  auto readAllStandardOutput() -> QByteArray override
  {
    QTC_CHECK(false);
    return {};
  }

  auto readAllStandardError() -> QByteArray override
  {
    QTC_CHECK(false);
    return {};
  }

  auto setProcessEnvironment(const QProcessEnvironment &) -> void override
  {
    QTC_CHECK(false);
  }

  auto setWorkingDirectory(const QString &) -> void override
  {
    QTC_CHECK(false);
  }

  auto start(const QString &, const QStringList &, const QByteArray &) -> void override
  {
    QTC_CHECK(false);
  }

  auto customStart(const CommandLine &command, const FilePath &workingDirectory, const Environment &environment) -> void override
  {
    m_terminal.setAbortOnMetaChars(isAbortOnMetaChars());
    m_terminal.setCommand(command);
    m_terminal.setWorkingDirectory(workingDirectory);
    m_terminal.setEnvironment(environment);
    m_terminal.start();
  }

  auto isCustomStart() const -> bool override { return true; }
  auto terminate() -> void override { m_terminal.stopProcess(); }
  auto kill() -> void override { m_terminal.stopProcess(); }
  auto close() -> void override { m_terminal.stopProcess(); }

  auto write(const QByteArray &) -> qint64 override
  {
    QTC_CHECK(false);
    return -1;
  }

  auto setStandardInputFile(const QString &fileName) -> void override
  {
    Q_UNUSED(fileName)
    QTC_CHECK(false);
  }

  // intentionally no-op without an assert
  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void override
  {
    Q_UNUSED(mode)
  }

  auto program() const -> QString override
  {
    QTC_CHECK(false);
    return {};
  }

  auto error() const -> QProcess::ProcessError override { return m_terminal.error(); }
  auto state() const -> QProcess::ProcessState override { return m_terminal.state(); }
  auto processId() const -> qint64 override { return m_terminal.processId(); }
  auto exitCode() const -> int override { return m_terminal.exitCode(); }
  auto exitStatus() const -> QProcess::ExitStatus override { return m_terminal.exitStatus(); }
  auto errorString() const -> QString override { return m_terminal.errorString(); }

  auto setErrorString(const QString &) -> void override
  {
    QTC_CHECK(false);
  }

  // intentionally no-op without an assert
  auto waitForStarted(int) -> bool override { return false; }

  auto waitForReadyRead(int) -> bool override
  {
    QTC_CHECK(false);
    return false;
  }

  // intentionally no-op without an assert
  auto waitForFinished(int) -> bool override { return false; }

  auto kickoffProcess() -> void override { m_terminal.kickoffProcess(); }
  auto interruptProcess() -> void override { m_terminal.interruptProcess(); }
  auto applicationMainThreadID() const -> qint64 override { return m_terminal.applicationMainThreadID(); }

private:
  Internal::TerminalProcess m_terminal;
};

class QProcessImpl : public ProcessInterface {
public:
  QProcessImpl(QObject *parent, ProcessMode processMode) : ProcessInterface(parent, processMode), m_process(new ProcessHelper(parent))
  {
    connect(m_process, &QProcess::started, this, &QProcessImpl::handleStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &ProcessInterface::finished);
    connect(m_process, &QProcess::errorOccurred, this, &ProcessInterface::errorOccurred);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessInterface::readyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &ProcessInterface::readyReadStandardError);
  }

  ~QProcessImpl() override
  {
    ProcessReaper::reap(m_process);
  }

  auto readAllStandardOutput() -> QByteArray override { return m_process->readAllStandardOutput(); }

  auto readAllStandardError() -> QByteArray override
  {
    if (m_process->processChannelMode() == QProcess::MergedChannels)
      return {};
    return m_process->readAllStandardError();
  }

  auto setProcessEnvironment(const QProcessEnvironment &environment) -> void override
  {
    m_process->setProcessEnvironment(environment);
  }

  auto setWorkingDirectory(const QString &dir) -> void override
  {
    m_process->setWorkingDirectory(dir);
  }

  auto start(const QString &program, const QStringList &arguments, const QByteArray &writeData) -> void override
  {
    ProcessStartHandler *handler = m_process->processStartHandler();
    handler->setProcessMode(processMode());
    handler->setWriteData(writeData);
    if (isBelowNormalPriority())
      handler->setBelowNormalPriority();
    handler->setNativeArguments(nativeArguments());
    if (isLowPriority())
      m_process->setLowPriority();
    if (isUnixTerminalDisabled())
      m_process->setUnixTerminalDisabled();
    m_process->start(program, arguments, handler->openMode());
    handler->handleProcessStart();
  }

  auto terminate() -> void override
  {
    m_process->terminate();
  }

  auto kill() -> void override
  {
    m_process->kill();
  }

  auto close() -> void override
  {
    m_process->close();
  }

  auto write(const QByteArray &data) -> qint64 override
  {
    return m_process->write(data);
  }

  auto setStandardInputFile(const QString &fileName) -> void override
  {
    m_process->setStandardInputFile(fileName);
  }

  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void override
  {
    m_process->setProcessChannelMode(mode);
  }

  auto program() const -> QString override
  {
    return m_process->program();
  }

  auto error() const -> QProcess::ProcessError override
  {
    return m_process->error();
  }

  auto state() const -> QProcess::ProcessState override
  {
    return m_process->state();
  }

  auto processId() const -> qint64 override
  {
    return m_process->processId();
  }

  auto exitCode() const -> int override
  {
    return m_process->exitCode();
  }

  auto exitStatus() const -> QProcess::ExitStatus override
  {
    return m_process->exitStatus();
  }

  auto errorString() const -> QString override
  {
    return m_process->errorString();
  }

  auto setErrorString(const QString &str) -> void override
  {
    m_process->setErrorString(str);
  }

  auto waitForStarted(int msecs) -> bool override
  {
    return m_process->waitForStarted(msecs);
  }

  auto waitForReadyRead(int msecs) -> bool override
  {
    return m_process->waitForReadyRead(msecs);
  }

  auto waitForFinished(int msecs) -> bool override
  {
    return m_process->waitForFinished(msecs);
  }

private:
  auto handleStarted() -> void
  {
    m_process->processStartHandler()->handleProcessStarted();
    emit started();
  }

  ProcessHelper *m_process;
};

static auto uniqueToken() -> uint
{
  static std::atomic_uint globalUniqueToken = 0;
  return ++globalUniqueToken;
}

class ProcessLauncherImpl : public ProcessInterface {
  Q_OBJECT public:
  ProcessLauncherImpl(QObject *parent, ProcessMode processMode) : ProcessInterface(parent, processMode), m_token(uniqueToken())
  {
    m_handle = LauncherInterface::registerHandle(parent, token(), processMode);
    connect(m_handle, &CallerHandle::errorOccurred, this, &ProcessInterface::errorOccurred);
    connect(m_handle, &CallerHandle::started, this, &ProcessInterface::started);
    connect(m_handle, &CallerHandle::finished, this, &ProcessInterface::finished);
    connect(m_handle, &CallerHandle::readyReadStandardOutput, this, &ProcessInterface::readyReadStandardOutput);
    connect(m_handle, &CallerHandle::readyReadStandardError, this, &ProcessInterface::readyReadStandardError);
  }

  ~ProcessLauncherImpl() override
  {
    cancel();
    LauncherInterface::unregisterHandle(token());
    m_handle = nullptr;
  }

  auto readAllStandardOutput() -> QByteArray override { return m_handle->readAllStandardOutput(); }
  auto readAllStandardError() -> QByteArray override { return m_handle->readAllStandardError(); }

  auto setProcessEnvironment(const QProcessEnvironment &environment) -> void override
  {
    m_handle->setProcessEnvironment(environment);
  }

  auto setWorkingDirectory(const QString &dir) -> void override { m_handle->setWorkingDirectory(dir); }

  auto start(const QString &program, const QStringList &arguments, const QByteArray &writeData) -> void override
  {
    if (isBelowNormalPriority())
      m_handle->setBelowNormalPriority();
    m_handle->setNativeArguments(nativeArguments());
    if (isLowPriority())
      m_handle->setLowPriority();
    if (isUnixTerminalDisabled())
      m_handle->setUnixTerminalDisabled();
    m_handle->start(program, arguments, writeData);
  }

  auto terminate() -> void override { cancel(); } // TODO: what are differences among terminate, kill and close?
  auto kill() -> void override { cancel(); }      // TODO: see above
  auto close() -> void override { cancel(); }     // TODO: see above
  auto write(const QByteArray &data) -> qint64 override { return m_handle->write(data); }

  auto setStandardInputFile(const QString &fileName) -> void override { m_handle->setStandardInputFile(fileName); }
  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void override { m_handle->setProcessChannelMode(mode); }

  auto program() const -> QString override { return m_handle->program(); }
  auto error() const -> QProcess::ProcessError override { return m_handle->error(); }
  auto state() const -> QProcess::ProcessState override { return m_handle->state(); }
  auto processId() const -> qint64 override { return m_handle->processId(); }
  auto exitCode() const -> int override { return m_handle->exitCode(); }
  auto exitStatus() const -> QProcess::ExitStatus override { return m_handle->exitStatus(); }
  auto errorString() const -> QString override { return m_handle->errorString(); }
  auto setErrorString(const QString &str) -> void override { m_handle->setErrorString(str); }

  auto waitForStarted(int msecs) -> bool override { return m_handle->waitForStarted(msecs); }
  auto waitForReadyRead(int msecs) -> bool override { return m_handle->waitForReadyRead(msecs); }
  auto waitForFinished(int msecs) -> bool override { return m_handle->waitForFinished(msecs); }

private:
  typedef void (ProcessLauncherImpl::*PreSignal)(void);

  auto cancel() -> void;

  auto handleSocketError(const QString &message) -> void;
  auto handleSocketReady() -> void;

  auto token() const -> quintptr { return m_token; }

  const uint m_token = 0;
  // Lives in caller's thread.
  CallerHandle *m_handle = nullptr;
};

auto ProcessLauncherImpl::cancel() -> void
{
  m_handle->cancel();
}

static auto newProcessInstance(QObject *parent, QtcProcess::ProcessImpl processImpl, ProcessMode mode, QtcProcess::TerminalMode terminalMode) -> ProcessInterface*
{
  if (terminalMode != QtcProcess::TerminalOff)
    return new TerminalImpl(parent, processImpl, terminalMode);
  if (processImpl == QtcProcess::QProcessImpl)
    return new QProcessImpl(parent, mode);
  return new ProcessLauncherImpl(parent, mode);
}

class QtcProcessPrivate : public QObject {
public:
  enum StartFailure {
    NoFailure,
    WrongFileNameFailure,
    OtherFailure
  };

  explicit QtcProcessPrivate(QtcProcess *parent, QtcProcess::ProcessImpl processImpl, ProcessMode processMode, QtcProcess::TerminalMode terminalMode) : QObject(parent), q(parent), m_process(newProcessInstance(parent, processImpl, processMode, terminalMode)), m_processMode(processMode), m_terminalMode(terminalMode)
  {
    connect(m_process, &ProcessInterface::started, q, &QtcProcess::started);
    connect(m_process, &ProcessInterface::finished, this, &QtcProcessPrivate::slotFinished);
    connect(m_process, &ProcessInterface::errorOccurred, this, [this](QProcess::ProcessError error) { handleError(error, OtherFailure); });
    connect(m_process, &ProcessInterface::readyReadStandardOutput, this, &QtcProcessPrivate::handleReadyReadStandardOutput);
    connect(m_process, &ProcessInterface::readyReadStandardError, this, &QtcProcessPrivate::handleReadyReadStandardError);
  }

  auto handleReadyReadStandardOutput() -> void
  {
    m_stdOut.append(m_process->readAllStandardOutput());
    m_hangTimerCount = 0;
    emit q->readyReadStandardOutput();
  }

  auto handleReadyReadStandardError() -> void
  {
    m_stdErr.append(m_process->readAllStandardError());
    m_hangTimerCount = 0;
    emit q->readyReadStandardError();
  }

  auto resolve(const FilePath &workingDir, const FilePath &filePath) const -> FilePath
  {
    if (filePath.isAbsolutePath())
      return filePath;

    const FilePath fromWorkingDir = workingDir.resolvePath(filePath);
    if (fromWorkingDir.exists() && fromWorkingDir.isExecutableFile())
      return fromWorkingDir;
    return filePath.searchInPath();
  }

  auto defaultStart(const CommandLine &commandLine, const FilePath &workingDirectory, const Environment &environment) -> void
  {
    if (processLog().isDebugEnabled()) {
      static int n = 0;
      qCDebug(processLog) << "STARTING PROCESS: " << ++n << "  " << commandLine.toUserOutput();
    }

    m_process->setProcessEnvironment(environment.toProcessEnvironment());
    m_process->setWorkingDirectory(workingDirectory.path());

    QString commandString;
    ProcessArgs arguments;
    const bool success = ProcessArgs::prepareCommand(commandLine, &commandString, &arguments, &environment, &workingDirectory);

    if (commandLine.executable().osType() == OsTypeWindows) {
      QString args;
      if (m_useCtrlCStub) {
        if (m_process->isLowPriority())
          ProcessArgs::addArg(&args, "-nice");
        ProcessArgs::addArg(&args, QDir::toNativeSeparators(commandString));
        commandString = QCoreApplication::applicationDirPath() + QLatin1String("/orca_ctrlc_stub.exe");
      } else if (m_process->isLowPriority()) {
        m_process->setBelowNormalPriority();
      }
      ProcessArgs::addArgs(&args, arguments.toWindowsArgs());
      #ifdef Q_OS_WIN
      m_process->setNativeArguments(args);
      #endif
      // Note: Arguments set with setNativeArgs will be appended to the ones
      // passed with start() below.
      start(commandString, QStringList(), workingDirectory, m_writeData);
    } else {
      if (!success) {
        q->setErrorString(QtcProcess::tr("Error in command line."));
        // Should be FailedToStart, but we cannot set the process error from the outside,
        // so it would be inconsistent.
        emit q->errorOccurred(QProcess::UnknownError);
        return;
      }
      start(commandString, arguments.toUnixArgs(), workingDirectory, m_writeData);
    }
  }

  auto start(const QString &program, const QStringList &arguments, const FilePath &workingDirectory, const QByteArray &writeData) -> void
  {
    const FilePath programFilePath = resolve(workingDirectory, FilePath::fromString(program));
    if (programFilePath.exists() && programFilePath.isExecutableFile()) {
      s_start.measureAndRun(&ProcessInterface::start, m_process, program, arguments, writeData);
    } else {
      m_process->setErrorString(QLatin1String("The program \"%1\" does not exist or is not executable.").arg(program));
      handleError(QProcess::FailedToStart, WrongFileNameFailure);
    }
  }

  auto fullCommandLine() const -> CommandLine
  {
    if (!m_runAsRoot || HostOsInfo::isWindowsHost())
      return m_commandLine;
    CommandLine rootCommand("sudo", {"-A"});
    rootCommand.addCommandLineAsArgs(m_commandLine);
    return rootCommand;
  }

  auto fullEnvironment() const -> Environment
  {
    Environment env;
    if (m_haveEnv) {
      if (m_environment.size() == 0)
        qWarning("QtcProcess::start: Empty environment set when running '%s'.", qPrintable(m_commandLine.executable().toString()));
      env = m_environment;
    } else {
      env = Environment::systemEnvironment();
    }

    // TODO: needs SshSettings
    //        if (m_runAsRoot)
    //            RunControl::provideAskPassEntry(env);
    return env;
  }

  QtcProcess *q;
  ProcessInterface *m_process;
  const ProcessMode m_processMode;
  const QtcProcess::TerminalMode m_terminalMode;
  CommandLine m_commandLine;
  FilePath m_workingDirectory;
  Environment m_environment;
  QByteArray m_writeData;
  bool m_runAsRoot = false;
  bool m_haveEnv = false;
  bool m_useCtrlCStub = false;

  auto slotTimeout() -> void;
  auto slotFinished(int exitCode, QProcess::ExitStatus e) -> void;
  auto handleError(QProcess::ProcessError error, StartFailure startFailure) -> void;
  auto clearForRun() -> void;

  auto interpretExitCode(int exitCode) -> QtcProcess::Result;

  QTextCodec *m_codec = QTextCodec::codecForLocale();
  QEventLoop *m_eventLoop = nullptr;
  QtcProcess::Result m_result = QtcProcess::StartFailed;
  ChannelBuffer m_stdOut;
  ChannelBuffer m_stdErr;
  ExitCodeInterpreter m_exitCodeInterpreter;

  int m_hangTimerCount = 0;
  int m_maxHangTimerCount = defaultMaxHangTimerCount;
  StartFailure m_startFailure = NoFailure;
  bool m_timeOutMessageBoxEnabled = false;
  bool m_waitingForUser = false;
};

auto QtcProcessPrivate::clearForRun() -> void
{
  m_hangTimerCount = 0;
  m_stdOut.clearForRun();
  m_stdOut.codec = m_codec;
  m_stdErr.clearForRun();
  m_stdErr.codec = m_codec;
  m_result = QtcProcess::StartFailed;
  m_startFailure = NoFailure;
}

auto QtcProcessPrivate::interpretExitCode(int exitCode) -> QtcProcess::Result
{
  if (m_exitCodeInterpreter)
    return m_exitCodeInterpreter(exitCode);

  // default:
  return exitCode ? QtcProcess::FinishedWithError : QtcProcess::FinishedWithSuccess;
}

} // Internal

/*!
    \class Utils::QtcProcess

    \brief The QtcProcess class provides functionality for with processes.

    \sa Utils::ProcessArgs
*/

static auto defaultProcessImpl() -> QtcProcess::ProcessImpl
{
  if (qEnvironmentVariableIsSet("QTC_USE_QPROCESS"))
    return QtcProcess::QProcessImpl;
  return QtcProcess::ProcessLauncherImpl;
}

QtcProcess::QtcProcess(const Setup &setup, QObject *parent) : QObject(parent), d(new QtcProcessPrivate(this, setup.processImpl == DefaultImpl ? defaultProcessImpl() : setup.processImpl, setup.processMode, setup.terminalMode))
{
  static int qProcessExitStatusMeta = qRegisterMetaType<QProcess::ExitStatus>();
  static int qProcessProcessErrorMeta = qRegisterMetaType<QProcess::ProcessError>();
  Q_UNUSED(qProcessExitStatusMeta)
  Q_UNUSED(qProcessProcessErrorMeta)
}

QtcProcess::QtcProcess(QObject *parent) : QtcProcess({}, parent) {}

QtcProcess::~QtcProcess()
{
  delete d;
}

auto QtcProcess::processMode() const -> ProcessMode
{
  return d->m_processMode;
}

auto QtcProcess::terminalMode() const -> QtcProcess::TerminalMode
{
  return d->m_terminalMode;
}

auto QtcProcess::setEnvironment(const Environment &env) -> void
{
  d->m_environment = env;
  d->m_haveEnv = true;
}

auto QtcProcess::unsetEnvironment() -> void
{
  d->m_environment = Environment();
  d->m_haveEnv = false;
}

auto QtcProcess::environment() const -> const Environment&
{
  return d->m_environment;
}

auto QtcProcess::hasEnvironment() const -> bool
{
  return d->m_haveEnv;
}

auto QtcProcess::setCommand(const CommandLine &cmdLine) -> void
{
  if (d->m_workingDirectory.needsDevice() && cmdLine.executable().needsDevice()) {
    QTC_CHECK(d->m_workingDirectory.host() == cmdLine.executable().host());
  }
  d->m_commandLine = cmdLine;
}

auto QtcProcess::commandLine() const -> const CommandLine&
{
  return d->m_commandLine;
}

auto QtcProcess::workingDirectory() const -> FilePath
{
  return d->m_workingDirectory;
}

auto QtcProcess::setWorkingDirectory(const FilePath &dir) -> void
{
  if (dir.needsDevice() && d->m_commandLine.executable().needsDevice()) {
    QTC_CHECK(dir.host() == d->m_commandLine.executable().host());
  }
  d->m_workingDirectory = dir;
}

auto QtcProcess::setUseCtrlCStub(bool enabled) -> void
{
  // Do not use the stub in debug mode. Activating the stub will shut down
  // Orca otherwise, because they share the same Windows console.
  // See ORCABUG-11995 for details.
  #ifndef QT_DEBUG
    d->m_useCtrlCStub = enabled;
  #else
  Q_UNUSED(enabled)
  #endif
}

auto QtcProcess::start() -> void
{
  if (d->m_commandLine.executable().needsDevice()) {
    QTC_ASSERT(s_deviceHooks.startProcessHook, return);
    s_deviceHooks.startProcessHook(*this);
    return;
  }
  d->clearForRun();
  const CommandLine cmd = d->fullCommandLine();
  const Environment env = d->fullEnvironment();
  if (d->m_process->isCustomStart())
    d->m_process->customStart(cmd, d->m_workingDirectory, env);
  else
    d->defaultStart(cmd, d->m_workingDirectory, env);
}

#ifdef Q_OS_WIN
static auto sendMessage(UINT message, HWND hwnd, LPARAM lParam) -> BOOL
{
  DWORD dwProcessID;
  GetWindowThreadProcessId(hwnd, &dwProcessID);
  if ((DWORD)lParam == dwProcessID) {
    SendNotifyMessage(hwnd, message, 0, 0);
    return FALSE;
  }
  return TRUE;
}

auto CALLBACK sendShutDownMessageToAllWindowsOfProcess_enumWnd(HWND hwnd, LPARAM lParam) -> BOOL
{
  static UINT uiShutDownMessage = RegisterWindowMessage(L"qtcctrlcstub_shutdown");
  return sendMessage(uiShutDownMessage, hwnd, lParam);
}

auto CALLBACK sendInterruptMessageToAllWindowsOfProcess_enumWnd(HWND hwnd, LPARAM lParam) -> BOOL
{
  static UINT uiInterruptMessage = RegisterWindowMessage(L"qtcctrlcstub_interrupt");
  return sendMessage(uiInterruptMessage, hwnd, lParam);
}
#endif

auto QtcProcess::terminate() -> void
{
  #ifdef Q_OS_WIN
  if (d->m_useCtrlCStub)
    EnumWindows(sendShutDownMessageToAllWindowsOfProcess_enumWnd, processId());
  else
  #endif
    d->m_process->terminate();
}

auto QtcProcess::interrupt() -> void
{
  #ifdef Q_OS_WIN
  QTC_ASSERT(d->m_useCtrlCStub, return);
  EnumWindows(sendInterruptMessageToAllWindowsOfProcess_enumWnd, processId());
  #endif
}

auto QtcProcess::startDetached(const CommandLine &cmd, const FilePath &workingDirectory, qint64 *pid) -> bool
{
  return QProcess::startDetached(cmd.executable().toUserOutput(), cmd.splitArguments(), workingDirectory.toUserOutput(), pid);
}

auto QtcProcess::setLowPriority() -> void
{
  d->m_process->setLowPriority();
}

auto QtcProcess::setDisableUnixTerminal() -> void
{
  d->m_process->setUnixTerminalDisabled();
}

auto QtcProcess::setAbortOnMetaChars(bool abort) -> void
{
  d->m_process->setAbortOnMetaChars(abort);
}

auto QtcProcess::setRunAsRoot(bool on) -> void
{
  d->m_runAsRoot = on;
}

auto QtcProcess::isRunAsRoot() const -> bool
{
  return d->m_runAsRoot;
}

auto QtcProcess::setStandardInputFile(const QString &inputFile) -> void
{
  d->m_process->setStandardInputFile(inputFile);
}

auto QtcProcess::toStandaloneCommandLine() const -> QString
{
  QStringList parts;
  parts.append("/usr/bin/env");
  if (!d->m_workingDirectory.isEmpty()) {
    parts.append("-C");
    d->m_workingDirectory.path();
  }
  parts.append("-i");
  if (d->m_environment.size() > 0) {
    const QStringList envVars = d->m_environment.toStringList();
    std::transform(envVars.cbegin(), envVars.cend(), std::back_inserter(parts), ProcessArgs::quoteArgUnix);
  }
  parts.append(d->m_commandLine.executable().path());
  parts.append(d->m_commandLine.splitArguments());
  return parts.join(" ");
}

auto QtcProcess::setRemoteProcessHooks(const DeviceProcessHooks &hooks) -> void
{
  s_deviceHooks = hooks;
}

auto QtcProcess::stopProcess() -> bool
{
  if (state() == QProcess::NotRunning)
    return true;
  terminate();
  if (waitForFinished(300))
    return true;
  kill();
  return waitForFinished(300);
}

static auto askToKill(const QString &command) -> bool
{
  #ifdef QT_GUI_LIB
  if (QThread::currentThread() != QCoreApplication::instance()->thread())
    return true;
  const QString title = QtcProcess::tr("Process Not Responding");
  QString msg = command.isEmpty() ? QtcProcess::tr("The process is not responding.") : QtcProcess::tr("The process \"%1\" is not responding.").arg(command);
  msg += ' ';
  msg += QtcProcess::tr("Terminate the process?");
  // Restore the cursor that is set to wait while running.
  const bool hasOverrideCursor = QApplication::overrideCursor() != nullptr;
  if (hasOverrideCursor)
    QApplication::restoreOverrideCursor();
  QMessageBox::StandardButton answer = QMessageBox::question(nullptr, title, msg, QMessageBox::Yes | QMessageBox::No);
  if (hasOverrideCursor)
    QApplication::setOverrideCursor(Qt::WaitCursor);
  return answer == QMessageBox::Yes;
  #else
    Q_UNUSED(command)
    return true;
  #endif
}

// Helper for running a process synchronously in the foreground with timeout
// detection (taking effect after no more output
// occurs on stderr/stdout as opposed to waitForFinished()). Returns false if a timeout
// occurs. Checking of the process' exit state/code still has to be done.

auto QtcProcess::readDataFromProcess(int timeoutS, QByteArray *stdOut, QByteArray *stdErr, bool showTimeOutMessageBox) -> bool
{
  enum { syncDebug = 0 };
  if (syncDebug)
    qDebug() << ">readDataFromProcess" << timeoutS;
  if (state() != QProcess::Running) {
    qWarning("readDataFromProcess: Process in non-running state passed in.");
    return false;
  }

  // Keep the process running until it has no longer has data
  bool finished = false;
  bool hasData = false;
  do {
    finished = waitForFinished(timeoutS > 0 ? timeoutS * 1000 : -1) || state() == QProcess::NotRunning;
    // First check 'stdout'
    const QByteArray newStdOut = readAllStandardOutput();
    if (!newStdOut.isEmpty()) {
      hasData = true;
      if (stdOut)
        stdOut->append(newStdOut);
    }
    // Check 'stderr' separately. This is a special handling
    // for 'git pull' and the like which prints its progress on stderr.
    const QByteArray newStdErr = readAllStandardError();
    if (!newStdErr.isEmpty()) {
      hasData = true;
      if (stdErr)
        stdErr->append(newStdErr);
    }
    // Prompt user, pretend we have data if says 'No'.
    const bool hang = !hasData && !finished;
    hasData = hang && showTimeOutMessageBox && !askToKill(d->m_process->program());
  } while (hasData && !finished);
  if (syncDebug)
    qDebug() << "<readDataFromProcess" << finished;
  return finished;
}

auto QtcProcess::normalizeNewlines(const QString &text) -> QString
{
  QString res = text;
  const auto newEnd = std::unique(res.begin(), res.end(), [](const QChar &c1, const QChar &c2) {
    return c1 == '\r' && c2 == '\r'; // ORCABUG-24556
  });
  res.chop(std::distance(newEnd, res.end()));
  res.replace("\r\n", "\n");
  return res;
}

auto QtcProcess::result() const -> QtcProcess::Result
{
  return d->m_result;
}

auto QtcProcess::setResult(Result result) -> void
{
  d->m_result = result;
}

auto QtcProcess::exitCode() const -> int
{
  if (d->m_startFailure == QtcProcessPrivate::WrongFileNameFailure)
    return 255; // This code is being returned by QProcess when FailedToStart error occurred
  return d->m_process->exitCode();
}

// Path utilities

// Locate a binary in a directory, applying all kinds of
// extensions the operating system supports.
static auto checkBinary(const QDir &dir, const QString &binary) -> QString
{
  // naive UNIX approach
  const QFileInfo info(dir.filePath(binary));
  if (info.isFile() && info.isExecutable())
    return info.absoluteFilePath();

  // Does the OS have some weird extension concept or does the
  // binary have a 3 letter extension?
  if (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost())
    return QString();
  const int dotIndex = binary.lastIndexOf(QLatin1Char('.'));
  if (dotIndex != -1 && dotIndex == binary.size() - 4)
    return QString();

  switch (HostOsInfo::hostOs()) {
  case OsTypeLinux:
  case OsTypeOtherUnix:
  case OsTypeOther:
    break;
  case OsTypeWindows: {
    static const char *windowsExtensions[] = {".cmd", ".bat", ".exe", ".com"};
    // Check the Windows extensions using the order
    const int windowsExtensionCount = sizeof(windowsExtensions) / sizeof(const char*);
    for (int e = 0; e < windowsExtensionCount; e ++) {
      const QFileInfo windowsBinary(dir.filePath(binary + QLatin1String(windowsExtensions[e])));
      if (windowsBinary.isFile() && windowsBinary.isExecutable())
        return windowsBinary.absoluteFilePath();
    }
  }
  break;
  case OsTypeMac: {
    // Check for Mac app folders
    const QFileInfo appFolder(dir.filePath(binary + QLatin1String(".app")));
    if (appFolder.isDir()) {
      QString macBinaryPath = appFolder.absoluteFilePath();
      macBinaryPath += QLatin1String("/Contents/MacOS/");
      macBinaryPath += binary;
      const QFileInfo macBinary(macBinaryPath);
      if (macBinary.isFile() && macBinary.isExecutable())
        return macBinary.absoluteFilePath();
    }
  }
  break;
  }
  return QString();
}

auto QtcProcess::locateBinary(const QString &path, const QString &binary) -> QString
{
  // Absolute file?
  const QFileInfo absInfo(binary);
  if (absInfo.isAbsolute())
    return checkBinary(absInfo.dir(), absInfo.fileName());

  // Windows finds binaries  in the current directory
  if (HostOsInfo::isWindowsHost()) {
    const QString currentDirBinary = checkBinary(QDir::current(), binary);
    if (!currentDirBinary.isEmpty())
      return currentDirBinary;
  }

  const QStringList paths = path.split(HostOsInfo::pathListSeparator());
  if (paths.empty())
    return QString();
  const QStringList::const_iterator cend = paths.constEnd();
  for (QStringList::const_iterator it = paths.constBegin(); it != cend; ++it) {
    const QDir dir(*it);
    const QString rc = checkBinary(dir, binary);
    if (!rc.isEmpty())
      return rc;
  }
  return QString();
}

auto QtcProcess::systemEnvironmentForBinary(const FilePath &filePath) -> Environment
{
  if (filePath.needsDevice()) {
    QTC_ASSERT(s_deviceHooks.systemEnvironmentForBinary, return {});
    return s_deviceHooks.systemEnvironmentForBinary(filePath);
  }

  return Environment::systemEnvironment();
}

auto QtcProcess::kickoffProcess() -> void
{
  d->m_process->kickoffProcess();
}

auto QtcProcess::interruptProcess() -> void
{
  d->m_process->interruptProcess();
}

auto QtcProcess::applicationMainThreadID() const -> qint64
{
  return d->m_process->applicationMainThreadID();
}

auto QtcProcess::setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void
{
  d->m_process->setProcessChannelMode(mode);
}

auto QtcProcess::error() const -> QProcess::ProcessError
{
  if (d->m_startFailure == QtcProcessPrivate::WrongFileNameFailure)
    return QProcess::FailedToStart;
  return d->m_process->error();
}

auto QtcProcess::state() const -> QProcess::ProcessState
{
  return d->m_process->state();
}

auto QtcProcess::isRunning() const -> bool
{
  return state() == QProcess::Running;
}

auto QtcProcess::errorString() const -> QString
{
  return d->m_process->errorString();
}

auto QtcProcess::setErrorString(const QString &str) -> void
{
  d->m_process->setErrorString(str);
}

auto QtcProcess::processId() const -> qint64
{
  return d->m_process->processId();
}

auto QtcProcess::waitForStarted(int msecs) -> bool
{
  return s_waitForStarted.measureAndRun(&ProcessInterface::waitForStarted, d->m_process, msecs);
}

auto QtcProcess::waitForReadyRead(int msecs) -> bool
{
  return d->m_process->waitForReadyRead(msecs);
}

auto QtcProcess::waitForFinished(int msecs) -> bool
{
  return d->m_process->waitForFinished(msecs);
}

auto QtcProcess::readAllStandardOutput() -> QByteArray
{
  QByteArray buf = d->m_stdOut.rawData;
  d->m_stdOut.rawData.clear();
  return buf;
}

auto QtcProcess::readAllStandardError() -> QByteArray
{
  QByteArray buf = d->m_stdErr.rawData;
  d->m_stdErr.rawData.clear();
  return buf;
}

auto QtcProcess::exitStatus() const -> QProcess::ExitStatus
{
  return d->m_process->exitStatus();
}

auto QtcProcess::kill() -> void
{
  d->m_process->kill();
}

auto QtcProcess::write(const QByteArray &input) -> qint64
{
  QTC_ASSERT(processMode() == ProcessMode::Writer, return -1);
  return d->m_process->write(input);
}

auto QtcProcess::close() -> void
{
  d->m_process->close();
}

auto QtcProcess::beginFeed() -> void
{
  d->clearForRun();
}

auto QtcProcess::endFeed() -> void
{
  d->slotFinished(0, QProcess::NormalExit);
}

auto QtcProcess::feedStdOut(const QByteArray &data) -> void
{
  d->m_stdOut.append(data);
  d->m_hangTimerCount = 0;
  emit readyReadStandardOutput();
}

auto QtcProcess::locateBinary(const QString &binary) -> QString
{
  const QByteArray path = qgetenv("PATH");
  return locateBinary(QString::fromLocal8Bit(path), binary);
}

/*!
    \class Utils::SynchronousProcess

    \brief The SynchronousProcess class runs a synchronous process in its own
    event loop that blocks only user input events. Thus, it allows for the GUI to
    repaint and append output to log windows.

    The callbacks set with setStdOutCallback(), setStdErrCallback() are called
    with complete lines based on the '\\n' marker.
    They would typically be used for log windows.

    Alternatively you can used setStdOutLineCallback() and setStdErrLineCallback()
    to process the output line by line.

    There is a timeout handling that takes effect after the last data have been
    read from stdout/stdin (as opposed to waitForFinished(), which measures time
    since it was invoked). It is thus also suitable for slow processes that
    continuously output data (like version system operations).

    The property timeOutMessageBoxEnabled influences whether a message box is
    shown asking the user if they want to kill the process on timeout (default: false).

    There are also static utility functions for dealing with fully synchronous
    processes, like reading the output with correct timeout handling.

    Caution: This class should NOT be used if there is a chance that the process
    triggers opening dialog boxes (for example, by file watchers triggering),
    as this will cause event loop problems.
*/

auto QtcProcess::exitMessage() -> QString
{
  const QString fullCmd = commandLine().toUserOutput();
  switch (result()) {
  case FinishedWithSuccess:
    return QtcProcess::tr("The command \"%1\" finished successfully.").arg(fullCmd);
  case FinishedWithError:
    return QtcProcess::tr("The command \"%1\" terminated with exit code %2.").arg(fullCmd).arg(exitCode());
  case TerminatedAbnormally:
    return QtcProcess::tr("The command \"%1\" terminated abnormally.").arg(fullCmd);
  case StartFailed:
    return QtcProcess::tr("The command \"%1\" could not be started.").arg(fullCmd);
  case Hang:
    return QtcProcess::tr("The command \"%1\" did not respond within the timeout limit (%2 s).").arg(fullCmd).arg(d->m_maxHangTimerCount);
  }
  return QString();
}

auto QtcProcess::allRawOutput() const -> QByteArray
{
  QTC_CHECK(d->m_stdOut.keepRawData);
  QTC_CHECK(d->m_stdErr.keepRawData);
  if (!d->m_stdOut.rawData.isEmpty() && !d->m_stdErr.rawData.isEmpty()) {
    QByteArray result = d->m_stdOut.rawData;
    if (!result.endsWith('\n'))
      result += '\n';
    result += d->m_stdErr.rawData;
    return result;
  }
  return !d->m_stdOut.rawData.isEmpty() ? d->m_stdOut.rawData : d->m_stdErr.rawData;
}

auto QtcProcess::allOutput() const -> QString
{
  QTC_CHECK(d->m_stdOut.keepRawData);
  QTC_CHECK(d->m_stdErr.keepRawData);
  const QString out = stdOut();
  const QString err = stdErr();

  if (!out.isEmpty() && !err.isEmpty()) {
    QString result = out;
    if (!result.endsWith('\n'))
      result += '\n';
    result += err;
    return result;
  }
  return !out.isEmpty() ? out : err;
}

auto QtcProcess::stdOut() const -> QString
{
  QTC_CHECK(d->m_stdOut.keepRawData);
  return normalizeNewlines(d->m_codec->toUnicode(d->m_stdOut.rawData));
}

auto QtcProcess::stdErr() const -> QString
{
  // FIXME: The tighter check below is actually good theoretically, but currently
  // ShellCommand::runFullySynchronous triggers it and disentangling there
  // is not trivial. So weaken it a bit for now.
  //QTC_CHECK(d->m_stdErr.keepRawData);
  QTC_CHECK(d->m_stdErr.keepRawData || d->m_stdErr.rawData.isEmpty());
  return normalizeNewlines(d->m_codec->toUnicode(d->m_stdErr.rawData));
}

auto QtcProcess::rawStdOut() const -> QByteArray
{
  QTC_CHECK(d->m_stdOut.keepRawData);
  return d->m_stdOut.rawData;
}

ORCA_UTILS_EXPORT auto operator<<(QDebug str, const QtcProcess &r) -> QDebug
{
  QDebug nsp = str.nospace();
  nsp << "QtcProcess: result=" << r.d->m_result << " ex=" << r.exitCode() << '\n' << r.d->m_stdOut.rawData.size() << " bytes stdout, stderr=" << r.d->m_stdErr.rawData << '\n';
  return str;
}

auto ChannelBuffer::clearForRun() -> void
{
  rawData.clear();
  codecState.reset(new QTextCodec::ConverterState);
  incompleteLineBuffer.clear();
}

/* Check for complete lines read from the device and return them, moving the
 * buffer position. */
auto ChannelBuffer::append(const QByteArray &text) -> void
{
  if (text.isEmpty())
    return;

  if (keepRawData)
    rawData += text;

  // Line-wise operation below:
  if (!outputCallback)
    return;

  // Convert and append the new input to the buffer of incomplete lines
  incompleteLineBuffer.append(codec->toUnicode(text.constData(), text.size(), codecState.get()));

  do {
    // Any completed lines in the incompleteLineBuffer?
    int pos = -1;
    if (emitSingleLines) {
      const int posn = incompleteLineBuffer.indexOf('\n');
      const int posr = incompleteLineBuffer.indexOf('\r');
      if (posn != -1) {
        if (posr != -1) {
          if (posn == posr + 1)
            pos = posn; // \r followed by \n -> line end, use the \n.
          else
            pos = qMin(posr, posn); // free floating \r and \n: Use the first one.
        } else {
          pos = posn;
        }
      } else {
        pos = posr; // Make sure internal '\r' triggers a line output
      }
    } else {
      pos = qMax(incompleteLineBuffer.lastIndexOf('\n'), incompleteLineBuffer.lastIndexOf('\r'));
    }

    if (pos == -1)
      break;

    // Get completed lines and remove them from the incompleteLinesBuffer:
    const QString line = QtcProcess::normalizeNewlines(incompleteLineBuffer.left(pos + 1));
    incompleteLineBuffer = incompleteLineBuffer.mid(pos + 1);

    QTC_ASSERT(outputCallback, return);
    outputCallback(line);

    if (!emitSingleLines)
      break;
  } while (true);
}

auto ChannelBuffer::handleRest() -> void
{
  if (outputCallback && !incompleteLineBuffer.isEmpty()) {
    outputCallback(incompleteLineBuffer);
    incompleteLineBuffer.clear();
  }
}

auto QtcProcess::setTimeoutS(int timeoutS) -> void
{
  if (timeoutS > 0)
    d->m_maxHangTimerCount = qMax(2, timeoutS);
  else
    d->m_maxHangTimerCount = INT_MAX / 1000;
}

auto QtcProcess::setCodec(QTextCodec *c) -> void
{
  QTC_ASSERT(c, return);
  d->m_codec = c;
}

auto QtcProcess::setTimeOutMessageBoxEnabled(bool v) -> void
{
  d->m_timeOutMessageBoxEnabled = v;
}

auto QtcProcess::setExitCodeInterpreter(const ExitCodeInterpreter &interpreter) -> void
{
  d->m_exitCodeInterpreter = interpreter;
}

auto QtcProcess::setWriteData(const QByteArray &writeData) -> void
{
  d->m_writeData = writeData;
}

#ifdef QT_GUI_LIB
static auto isGuiThread() -> bool
{
  return QThread::currentThread() == QCoreApplication::instance()->thread();
}
#endif

auto QtcProcess::runBlocking(QtcProcess::EventLoopMode eventLoopMode) -> void
{
  // FIXME: Implement properly

  if (d->m_commandLine.executable().needsDevice()) {
    QtcProcess::start();
    waitForFinished();
    return;
  };

  qCDebug(processLog).noquote() << "Starting blocking:" << d->m_commandLine.toUserOutput() << " process user events: " << (eventLoopMode == QtcProcess::WithEventLoop);
  ExecuteOnDestruction logResult([this] { qCDebug(processLog) << *this; });

  if (eventLoopMode == QtcProcess::WithEventLoop) {
    QtcProcess::start();

    // On Windows, start failure is triggered immediately if the
    // executable cannot be found in the path. Do not start the
    // event loop in that case.
    if (d->m_startFailure == QtcProcessPrivate::NoFailure) {
      QTimer timer(this);
      connect(&timer, &QTimer::timeout, d, &QtcProcessPrivate::slotTimeout);
      timer.setInterval(1000);
      timer.start();
      #ifdef QT_GUI_LIB
      if (isGuiThread())
        QApplication::setOverrideCursor(Qt::WaitCursor);
      #endif
      QEventLoop eventLoop(this);
      QTC_ASSERT(!d->m_eventLoop, return);
      d->m_eventLoop = &eventLoop;
      eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
      d->m_eventLoop = nullptr;
      d->m_stdOut.append(d->m_process->readAllStandardOutput());
      d->m_stdErr.append(d->m_process->readAllStandardError());

      timer.stop();
      #ifdef QT_GUI_LIB
      if (isGuiThread())
        QApplication::restoreOverrideCursor();
      #endif
    }
  } else {
    QtcProcess::start();
    if (!waitForStarted(d->m_maxHangTimerCount * 1000)) {
      d->m_result = QtcProcess::StartFailed;
      return;
    }
    if (!waitForFinished(d->m_maxHangTimerCount * 1000)) {
      d->m_result = QtcProcess::Hang;
      terminate();
      if (!waitForFinished(1000)) {
        kill();
        waitForFinished(1000);
      }
    }

    if (state() != QProcess::NotRunning)
      return;

    d->m_stdOut.append(d->m_process->readAllStandardOutput());
    d->m_stdErr.append(d->m_process->readAllStandardError());
  }
}

auto QtcProcess::setStdOutCallback(const std::function<void (const QString &)> &callback) -> void
{
  d->m_stdOut.outputCallback = callback;
  d->m_stdOut.emitSingleLines = false;
}

auto QtcProcess::setStdOutLineCallback(const std::function<void (const QString &)> &callback) -> void
{
  d->m_stdOut.outputCallback = callback;
  d->m_stdOut.emitSingleLines = true;
  d->m_stdOut.keepRawData = false;
}

auto QtcProcess::setStdErrCallback(const std::function<void (const QString &)> &callback) -> void
{
  d->m_stdErr.outputCallback = callback;
  d->m_stdErr.emitSingleLines = false;
}

auto QtcProcess::setStdErrLineCallback(const std::function<void (const QString &)> &callback) -> void
{
  d->m_stdErr.outputCallback = callback;
  d->m_stdErr.emitSingleLines = true;
  d->m_stdErr.keepRawData = false;
}

auto QtcProcessPrivate::slotTimeout() -> void
{
  if (!m_waitingForUser && (++m_hangTimerCount > m_maxHangTimerCount)) {
    if (debug)
      qDebug() << Q_FUNC_INFO << "HANG detected, killing";
    m_waitingForUser = true;
    const bool terminate = !m_timeOutMessageBoxEnabled || askToKill(m_commandLine.executable().toString());
    m_waitingForUser = false;
    if (terminate) {
      q->stopProcess();
      m_result = QtcProcess::Hang;
    } else {
      m_hangTimerCount = 0;
    }
  } else {
    if (debug)
      qDebug() << Q_FUNC_INFO << m_hangTimerCount;
  }
}

auto QtcProcessPrivate::slotFinished(int exitCode, QProcess::ExitStatus status) -> void
{
  if (debug)
    qDebug() << Q_FUNC_INFO << exitCode << status;
  m_hangTimerCount = 0;

  switch (status) {
  case QProcess::NormalExit:
    m_result = interpretExitCode(exitCode);
    break;
  case QProcess::CrashExit:
    // Was hang detected before and killed?
    if (m_result != QtcProcess::Hang)
      m_result = QtcProcess::TerminatedAbnormally;
    break;
  }
  if (m_eventLoop)
    m_eventLoop->quit();

  m_stdOut.handleRest();
  m_stdErr.handleRest();

  emit q->finished();
}

auto QtcProcessPrivate::handleError(QProcess::ProcessError error, StartFailure startFailure) -> void
{
  m_hangTimerCount = 0;
  if (debug)
    qDebug() << Q_FUNC_INFO << error;
  // Was hang detected before and killed?
  if (m_result != QtcProcess::Hang)
    m_result = QtcProcess::StartFailed;
  m_startFailure = startFailure;
  if (m_eventLoop)
    m_eventLoop->quit();

  emit q->errorOccurred(error);
}

} // namespace Utils

#include "qtcprocess.moc"
