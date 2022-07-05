// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "algorithm.h"
#include "launchersocket.h"
#include "launcherinterface.h"

#include "qtcassert.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QMutexLocker>

#include <iostream>

namespace Utils {
namespace Internal {

class LauncherSignal {
public:
  auto signalType() const -> CallerHandle::SignalType { return m_signalType; }
  virtual ~LauncherSignal() = default;

protected:
  LauncherSignal(CallerHandle::SignalType signalType) : m_signalType(signalType) {}

private:
  const CallerHandle::SignalType m_signalType;
};

class ErrorSignal : public LauncherSignal {
public:
  ErrorSignal(QProcess::ProcessError error, const QString &errorString) : LauncherSignal(CallerHandle::SignalType::Error), m_error(error), m_errorString(errorString) {}
  auto error() const -> QProcess::ProcessError { return m_error; }
  auto errorString() const -> QString { return m_errorString; }

private:
  const QProcess::ProcessError m_error;
  const QString m_errorString;
};

class StartedSignal : public LauncherSignal {
public:
  StartedSignal(int processId) : LauncherSignal(CallerHandle::SignalType::Started), m_processId(processId) {}
  auto processId() const -> int { return m_processId; }

private:
  const int m_processId;
};

class ReadyReadSignal : public LauncherSignal {
public:
  ReadyReadSignal(const QByteArray &stdOut, const QByteArray &stdErr) : LauncherSignal(CallerHandle::SignalType::ReadyRead), m_stdOut(stdOut), m_stdErr(stdErr) {}
  auto stdOut() const -> QByteArray { return m_stdOut; }
  auto stdErr() const -> QByteArray { return m_stdErr; }

  auto mergeWith(ReadyReadSignal *newSignal) -> void
  {
    m_stdOut += newSignal->stdOut();
    m_stdErr += newSignal->stdErr();
  }

private:
  QByteArray m_stdOut;
  QByteArray m_stdErr;
};

class FinishedSignal : public LauncherSignal {
public:
  FinishedSignal(QProcess::ExitStatus exitStatus, int exitCode) : LauncherSignal(CallerHandle::SignalType::Finished), m_exitStatus(exitStatus), m_exitCode(exitCode) {}
  auto exitStatus() const -> QProcess::ExitStatus { return m_exitStatus; }
  auto exitCode() const -> int { return m_exitCode; }

private:
  const QProcess::ExitStatus m_exitStatus;
  const int m_exitCode;
};

CallerHandle::~CallerHandle()
{
  qDeleteAll(m_signals);
}

auto CallerHandle::waitForStarted(int msecs) -> bool
{
  return waitForSignal(msecs, SignalType::Started);
}

auto CallerHandle::waitForReadyRead(int msces) -> bool
{
  return waitForSignal(msces, SignalType::ReadyRead);
}

auto CallerHandle::waitForFinished(int msecs) -> bool
{
  return waitForSignal(msecs, SignalType::Finished);
}

auto CallerHandle::flush() -> QList<SignalType>
{
  return flushFor(SignalType::NoSignal);
}

auto CallerHandle::flushFor(SignalType signalType) -> QList<SignalType>
{
  QTC_ASSERT(isCalledFromCallersThread(), return {});
  QList<LauncherSignal*> oldSignals;
  QList<SignalType> flushedSignals;
  {
    // 1. If signalType is no signal - flush all
    // 2. Flush all if we have any error
    // 3. If we are flushing for Finished or ReadyRead, flush all, too
    // 4. If we are flushing for Started, flush Started only

    QMutexLocker locker(&m_mutex);

    const QList<SignalType> storedSignals = transform(qAsConst(m_signals), [](const LauncherSignal *launcherSignal) {
      return launcherSignal->signalType();
    });

    const bool flushAll = (signalType == SignalType::NoSignal) || (signalType == SignalType::ReadyRead) || (signalType == SignalType::Finished) || storedSignals.contains(SignalType::Error);
    if (flushAll) {
      oldSignals = m_signals;
      m_signals = {};
      flushedSignals = storedSignals;
    } else {
      auto matchingIndex = storedSignals.lastIndexOf(signalType);
      if (matchingIndex < 0 && (signalType == SignalType::ReadyRead))
        matchingIndex = storedSignals.lastIndexOf(SignalType::Started);
      if (matchingIndex >= 0) {
        oldSignals = m_signals.mid(0, matchingIndex + 1);
        m_signals = m_signals.mid(matchingIndex + 1);
        flushedSignals = storedSignals.mid(0, matchingIndex + 1);
      }
    }
  }
  for (const LauncherSignal *storedSignal : qAsConst(oldSignals)) {
    const SignalType storedSignalType = storedSignal->signalType();
    switch (storedSignalType) {
    case SignalType::NoSignal:
      break;
    case SignalType::Error:
      handleError(static_cast<const ErrorSignal*>(storedSignal));
      break;
    case SignalType::Started:
      handleStarted(static_cast<const StartedSignal*>(storedSignal));
      break;
    case SignalType::ReadyRead:
      handleReadyRead(static_cast<const ReadyReadSignal*>(storedSignal));
      break;
    case SignalType::Finished:
      handleFinished(static_cast<const FinishedSignal*>(storedSignal));
      break;
    }
    delete storedSignal;
  }
  return flushedSignals;
}

// Called from caller's thread exclusively.
auto CallerHandle::shouldFlushFor(SignalType signalType) const -> bool
{
  QTC_ASSERT(isCalledFromCallersThread(), return false);
  // TODO: Should we always flush when the list isn't empty?
  QMutexLocker locker(&m_mutex);
  for (const LauncherSignal *storedSignal : m_signals) {
    const SignalType storedSignalType = storedSignal->signalType();
    if (storedSignalType == signalType)
      return true;
    if (storedSignalType == SignalType::Error)
      return true;
    if (storedSignalType == SignalType::Finished)
      return true;
  }
  return false;
}

auto CallerHandle::handleError(const ErrorSignal *launcherSignal) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_processState = QProcess::NotRunning;
  m_error = launcherSignal->error();
  m_errorString = launcherSignal->errorString();
  if (m_error == QProcess::FailedToStart)
    m_exitCode = 255; // This code is being returned by QProcess when FailedToStart error occurred
  emit errorOccurred(m_error);
}

auto CallerHandle::handleStarted(const StartedSignal *launcherSignal) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_processState = QProcess::Running;
  m_processId = launcherSignal->processId();
  emit started();
}

auto CallerHandle::handleReadyRead(const ReadyReadSignal *launcherSignal) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  if (m_channelMode == QProcess::ForwardedOutputChannel || m_channelMode == QProcess::ForwardedChannels) {
    std::cout << launcherSignal->stdOut().constData();
  } else {
    m_stdout += launcherSignal->stdOut();
    if (!m_stdout.isEmpty()) emit readyReadStandardOutput();
  }
  if (m_channelMode == QProcess::ForwardedErrorChannel || m_channelMode == QProcess::ForwardedChannels) {
    std::cerr << launcherSignal->stdErr().constData();
  } else {
    m_stderr += launcherSignal->stdErr();
    if (!m_stderr.isEmpty()) emit readyReadStandardError();
  }
}

auto CallerHandle::handleFinished(const FinishedSignal *launcherSignal) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_processState = QProcess::NotRunning;
  m_exitStatus = launcherSignal->exitStatus();
  m_exitCode = launcherSignal->exitCode();
  emit finished(m_exitCode, m_exitStatus);
}

// Called from launcher's thread exclusively.
auto CallerHandle::appendSignal(LauncherSignal *launcherSignal) -> void
{
  QTC_ASSERT(!isCalledFromCallersThread(), return);
  if (launcherSignal->signalType() == SignalType::NoSignal)
    return;

  QMutexLocker locker(&m_mutex);
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  // Merge ReadyRead signals into one.
  if (launcherSignal->signalType() == SignalType::ReadyRead && !m_signals.isEmpty() && m_signals.last()->signalType() == SignalType::ReadyRead) {
    ReadyReadSignal *lastSignal = static_cast<ReadyReadSignal*>(m_signals.last());
    ReadyReadSignal *newSignal = static_cast<ReadyReadSignal*>(launcherSignal);
    lastSignal->mergeWith(newSignal);
    delete newSignal;
    return;
  }
  m_signals.append(launcherSignal);
}

auto CallerHandle::state() const -> QProcess::ProcessState
{
  return m_processState;
}

auto CallerHandle::cancel() -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  switch (m_processState.exchange(QProcess::NotRunning)) {
  case QProcess::NotRunning:
    break;
  case QProcess::Starting:
    m_errorString = QCoreApplication::translate("Utils::LauncherHandle", "Process was canceled before it was started.");
    m_error = QProcess::FailedToStart;
    if (LauncherInterface::isReady()) // TODO: race condition with m_processState???
      sendPacket(StopProcessPacket(m_token));
    else emit errorOccurred(m_error);
    break;
  case QProcess::Running:
    sendPacket(StopProcessPacket(m_token));
    break;
  }

  if (m_launcherHandle)
    m_launcherHandle->setCanceled();
}

auto CallerHandle::readAllStandardOutput() -> QByteArray
{
  QTC_ASSERT(isCalledFromCallersThread(), return {});
  return readAndClear(m_stdout);
}

auto CallerHandle::readAllStandardError() -> QByteArray
{
  QTC_ASSERT(isCalledFromCallersThread(), return {});
  return readAndClear(m_stderr);
}

auto CallerHandle::processId() const -> qint64
{
  QTC_ASSERT(isCalledFromCallersThread(), return 0);
  return m_processId;
}

auto CallerHandle::exitCode() const -> int
{
  QTC_ASSERT(isCalledFromCallersThread(), return -1);
  return m_exitCode;
}

auto CallerHandle::errorString() const -> QString
{
  QTC_ASSERT(isCalledFromCallersThread(), return {});
  return m_errorString;
}

auto CallerHandle::setErrorString(const QString &str) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_errorString = str;
}

auto CallerHandle::start(const QString &program, const QStringList &arguments, const QByteArray &writeData) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  if (!m_launcherHandle || m_launcherHandle->isSocketError()) {
    m_error = QProcess::FailedToStart;
    emit errorOccurred(m_error);
    return;
  }

  auto startWhenRunning = [&program, &oldProgram = m_command] {
    qWarning() << "Trying to start" << program << "while" << oldProgram << "is still running for the same QtcProcess instance." << "The current call will be ignored.";
  };
  QTC_ASSERT(m_processState == QProcess::NotRunning, startWhenRunning(); return);

  auto processLauncherNotStarted = [&program] {
    qWarning() << "Trying to start" << program << "while process launcher wasn't started yet.";
  };
  QTC_ASSERT(LauncherInterface::isStarted(), processLauncherNotStarted());

  QMutexLocker locker(&m_mutex);
  m_command = program;
  m_arguments = arguments;
  m_writeData = writeData;
  m_processState = QProcess::Starting;
  StartProcessPacket *p = new StartProcessPacket(m_token);
  p->command = m_command;
  p->arguments = m_arguments;
  p->env = m_environment.toStringList();
  p->workingDir = m_workingDirectory;
  p->processMode = m_processMode;
  p->writeData = m_writeData;
  p->channelMode = m_channelMode;
  p->standardInputFile = m_standardInputFile;
  p->belowNormalPriority = m_belowNormalPriority;
  p->nativeArguments = m_nativeArguments;
  p->lowPriority = m_lowPriority;
  p->unixTerminalDisabled = m_unixTerminalDisabled;
  m_startPacket.reset(p);
  if (LauncherInterface::isReady())
    doStart();
}

// Called from caller's or launcher's thread.
auto CallerHandle::startIfNeeded() -> void
{
  QMutexLocker locker(&m_mutex);
  if (m_processState == QProcess::Starting)
    doStart();
}

// Called from caller's or launcher's thread. Call me with mutex locked.
auto CallerHandle::doStart() -> void
{
  if (!m_startPacket)
    return;
  sendPacket(*m_startPacket);
  m_startPacket.reset(nullptr);
}

// Called from caller's or launcher's thread.
auto CallerHandle::sendPacket(const LauncherPacket &packet) -> void
{
  LauncherInterface::sendData(packet.serialize());
}

auto CallerHandle::write(const QByteArray &data) -> qint64
{
  QTC_ASSERT(isCalledFromCallersThread(), return -1);

  if (m_processState != QProcess::Running)
    return -1;

  WritePacket p(m_token);
  p.inputData = data;
  sendPacket(p);
  return data.size();
}

auto CallerHandle::error() const -> QProcess::ProcessError
{
  QTC_ASSERT(isCalledFromCallersThread(), return QProcess::UnknownError);
  return m_error;
}

auto CallerHandle::program() const -> QString
{
  QMutexLocker locker(&m_mutex);
  return m_command;
}

auto CallerHandle::arguments() const -> QStringList
{
  QMutexLocker locker(&m_mutex);
  return m_arguments;
}

auto CallerHandle::setStandardInputFile(const QString &fileName) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_standardInputFile = fileName;
}

auto CallerHandle::setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_channelMode = mode;
}

auto CallerHandle::setProcessEnvironment(const QProcessEnvironment &environment) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_environment = environment;
}

auto CallerHandle::setWorkingDirectory(const QString &dir) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_workingDirectory = dir;
}

auto CallerHandle::exitStatus() const -> QProcess::ExitStatus
{
  QTC_ASSERT(isCalledFromCallersThread(), return QProcess::CrashExit);
  return m_exitStatus;
}

auto CallerHandle::setBelowNormalPriority() -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_belowNormalPriority = true;
}

auto CallerHandle::setNativeArguments(const QString &arguments) -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_nativeArguments = arguments;
}

auto CallerHandle::setLowPriority() -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_lowPriority = true;
}

auto CallerHandle::setUnixTerminalDisabled() -> void
{
  QTC_ASSERT(isCalledFromCallersThread(), return);
  m_unixTerminalDisabled = true;
}

auto CallerHandle::waitForSignal(int msecs, SignalType newSignal) -> bool
{
  QTC_ASSERT(isCalledFromCallersThread(), return false);
  if (!canWaitFor(newSignal))
    return false;
  if (!m_launcherHandle)
    return false;
  return m_launcherHandle->waitForSignal(msecs, newSignal);
}

auto CallerHandle::canWaitFor(SignalType newSignal) const -> bool
{
  QTC_ASSERT(isCalledFromCallersThread(), return false);
  switch (newSignal) {
  case SignalType::Started:
    return m_processState == QProcess::Starting;
  case SignalType::ReadyRead:
  case SignalType::Finished:
    return m_processState != QProcess::NotRunning;
  default:
    break;
  }
  return false;
}

// Called from caller's or launcher's thread.
auto CallerHandle::isCalledFromCallersThread() const -> bool
{
  return QThread::currentThread() == thread();
}

// Called from caller's or launcher's thread. Call me with mutex locked.
auto CallerHandle::isCalledFromLaunchersThread() const -> bool
{
  if (!m_launcherHandle)
    return false;
  return QThread::currentThread() == m_launcherHandle->thread();
}

// Called from caller's thread exclusively.
auto LauncherHandle::waitForSignal(int msecs, CallerHandle::SignalType newSignal) -> bool
{
  QTC_ASSERT(!isCalledFromLaunchersThread(), return false);
  QDeadlineTimer deadline(msecs);
  while (true) {
    if (deadline.hasExpired())
      break;
    if (!doWaitForSignal(deadline, newSignal))
      break;
    m_awaitingShouldContinue = true; // TODO: make it recursive?
    const QList<CallerHandle::SignalType> flushedSignals = m_callerHandle->flushFor(newSignal);
    const bool wasCanceled = !m_awaitingShouldContinue;
    m_awaitingShouldContinue = false;
    const bool errorOccurred = flushedSignals.contains(CallerHandle::SignalType::Error);
    if (errorOccurred)
      return false; // apparently QProcess behaves like this in case of error
    const bool newSignalFlushed = flushedSignals.contains(newSignal);
    if (newSignalFlushed) // so we don't continue waiting
      return true;
    if (wasCanceled)
      return true; // or false? is false only in case of timeout?
    const bool finishedSignalFlushed = flushedSignals.contains(CallerHandle::SignalType::Finished);
    if (finishedSignalFlushed)
      return false; // finish has appeared but we were waiting for other signal
  }
  return false;
}

// Called from caller's thread exclusively.
auto LauncherHandle::doWaitForSignal(QDeadlineTimer deadline, CallerHandle::SignalType newSignal) -> bool
{
  QMutexLocker locker(&m_mutex);
  QTC_ASSERT(isCalledFromCallersThread(), return false);
  QTC_ASSERT(m_waitingFor == CallerHandle::SignalType::NoSignal, return false);
  // It may happen, that after calling start() and before calling waitForStarted() we might have
  // reached the Running (or even Finished) state already. In this case we should have
  // collected Started (or even Finished) signal to be flushed - so we return true
  // and we are going to flush pending signals synchronously.
  // It could also happen, that some new readyRead data has appeared, so before we wait for
  // more we flush it, too.
  if (m_callerHandle->shouldFlushFor(newSignal))
    return true;

  m_waitingFor = newSignal;
  const bool ret = m_waitCondition.wait(&m_mutex, deadline);
  m_waitingFor = CallerHandle::SignalType::NoSignal;
  return ret;
}

// Called from launcher's thread exclusively. Call me with mutex locked.
auto LauncherHandle::wakeUpIfWaitingFor(CallerHandle::SignalType newSignal) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  // TODO: should we always wake up in case m_waitingFor != NoSignal?
  // The matching signal came
  const bool signalMatched = (m_waitingFor == newSignal);
  // E.g. if we are waiting for ReadyRead and we got Finished or Error signal instead -> wake it, too.
  const bool finishedOrErrorWhileWaiting = (m_waitingFor != CallerHandle::SignalType::NoSignal) && ((newSignal == CallerHandle::SignalType::Finished) || (newSignal == CallerHandle::SignalType::Error));
  // Wake up, flush and continue waiting.
  // E.g. when being in waitingForFinished() state and Started or ReadyRead signal came.
  const bool continueWaitingAfterFlushing = ((m_waitingFor == CallerHandle::SignalType::Finished) && (newSignal != CallerHandle::SignalType::Finished)) || ((m_waitingFor == CallerHandle::SignalType::ReadyRead) && (newSignal == CallerHandle::SignalType::Started));
  const bool shouldWake = signalMatched || finishedOrErrorWhileWaiting || continueWaitingAfterFlushing;

  if (shouldWake)
    m_waitCondition.wakeOne();
}

// Called from launcher's thread exclusively. Call me with mutex locked.
auto LauncherHandle::flushCaller() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  if (!m_callerHandle)
    return;

  // call in callers thread
  QMetaObject::invokeMethod(m_callerHandle, &CallerHandle::flush);
}

auto LauncherHandle::handlePacket(LauncherPacketType type, const QByteArray &payload) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  switch (type) {
  case LauncherPacketType::ProcessError:
    handleErrorPacket(payload);
    break;
  case LauncherPacketType::ProcessStarted:
    handleStartedPacket(payload);
    break;
  case LauncherPacketType::ReadyReadStandardOutput:
    handleReadyReadStandardOutput(payload);
    break;
  case LauncherPacketType::ReadyReadStandardError:
    handleReadyReadStandardError(payload);
    break;
  case LauncherPacketType::ProcessFinished:
    handleFinishedPacket(payload);
    break;
  default: QTC_ASSERT(false, break);
  }
}

auto LauncherHandle::handleErrorPacket(const QByteArray &packetData) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::Error);
  if (!m_callerHandle)
    return;

  const auto packet = LauncherPacket::extractPacket<ProcessErrorPacket>(m_token, packetData);
  m_callerHandle->appendSignal(new ErrorSignal(packet.error, packet.errorString));
  flushCaller();
}

auto LauncherHandle::handleStartedPacket(const QByteArray &packetData) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::Started);
  if (!m_callerHandle)
    return;

  const auto packet = LauncherPacket::extractPacket<ProcessStartedPacket>(m_token, packetData);
  m_callerHandle->appendSignal(new StartedSignal(packet.processId));
  flushCaller();
}

auto LauncherHandle::handleReadyReadStandardOutput(const QByteArray &packetData) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::ReadyRead);
  if (!m_callerHandle)
    return;

  const auto packet = LauncherPacket::extractPacket<ReadyReadStandardOutputPacket>(m_token, packetData);
  if (packet.standardChannel.isEmpty())
    return;

  m_callerHandle->appendSignal(new ReadyReadSignal(packet.standardChannel, {}));
  flushCaller();
}

auto LauncherHandle::handleReadyReadStandardError(const QByteArray &packetData) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::ReadyRead);
  if (!m_callerHandle)
    return;

  const auto packet = LauncherPacket::extractPacket<ReadyReadStandardErrorPacket>(m_token, packetData);
  if (packet.standardChannel.isEmpty())
    return;

  m_callerHandle->appendSignal(new ReadyReadSignal({}, packet.standardChannel));
  flushCaller();
}

auto LauncherHandle::handleFinishedPacket(const QByteArray &packetData) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::Finished);
  if (!m_callerHandle)
    return;

  const auto packet = LauncherPacket::extractPacket<ProcessFinishedPacket>(m_token, packetData);
  const QByteArray stdOut = packet.stdOut;
  const QByteArray stdErr = packet.stdErr;
  const QProcess::ProcessError error = packet.error;
  const QString errorString = packet.errorString;

  // We assume that if error is UnknownError, everything went fine.
  // By default QProcess returns "Unknown error" for errorString()
  if (error != QProcess::UnknownError)
    m_callerHandle->appendSignal(new ErrorSignal(error, errorString));
  if (!stdOut.isEmpty() || !stdErr.isEmpty())
    m_callerHandle->appendSignal(new ReadyReadSignal(stdOut, stdErr));
  m_callerHandle->appendSignal(new FinishedSignal(packet.exitStatus, packet.exitCode));
  flushCaller();
}

auto LauncherHandle::handleSocketReady() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  m_socketError = false;
  QMutexLocker locker(&m_mutex);
  if (m_callerHandle)
    m_callerHandle->startIfNeeded();
}

auto LauncherHandle::handleSocketError(const QString &message) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  m_socketError = true; // TODO: ???
  QMutexLocker locker(&m_mutex);
  wakeUpIfWaitingFor(CallerHandle::SignalType::Error);
  if (!m_callerHandle)
    return;

  const QString errorString = QCoreApplication::translate("Utils::QtcProcess", "Internal socket error: %1").arg(message);
  m_callerHandle->appendSignal(new ErrorSignal(QProcess::FailedToStart, errorString));
  flushCaller();
}

auto LauncherHandle::isCalledFromLaunchersThread() const -> bool
{
  return QThread::currentThread() == thread();
}

// call me with mutex locked
auto LauncherHandle::isCalledFromCallersThread() const -> bool
{
  if (!m_callerHandle)
    return false;
  return QThread::currentThread() == m_callerHandle->thread();
}

LauncherSocket::LauncherSocket(QObject *parent) : QObject(parent)
{
  qRegisterMetaType<LauncherPacketType>();
  qRegisterMetaType<quintptr>("quintptr");
}

LauncherSocket::~LauncherSocket()
{
  QMutexLocker locker(&m_mutex);
  auto displayHandles = [&handles = m_handles] {
    qWarning() << "Destroying process launcher while" << handles.count() << "processes are still alive. The following processes are still alive:";
    for (LauncherHandle *handle : handles) {
      CallerHandle *callerHandle = handle->callerHandle();
      if (callerHandle->state() != QProcess::NotRunning) {
        qWarning() << "  " << callerHandle->program() << callerHandle->arguments() << "in thread" << (void*)callerHandle->thread();
      } else {
        qWarning() << "  Not running process in thread" << (void*)callerHandle->thread();
      }
    }
  };
  QTC_ASSERT(m_handles.isEmpty(), displayHandles());
}

auto LauncherSocket::sendData(const QByteArray &data) -> void
{
  if (!isReady())
    return;

  auto storeRequest = [this](const QByteArray &data) {
    QMutexLocker locker(&m_mutex);
    m_requests.push_back(data);
    return m_requests.size() == 1; // Returns true if requests handling should be triggered.
  };

  if (storeRequest(data)) // Call handleRequests() in launcher's thread.
    QMetaObject::invokeMethod(this, &LauncherSocket::handleRequests);
}

auto LauncherSocket::registerHandle(QObject *parent, quintptr token, ProcessMode mode) -> CallerHandle*
{
  QTC_ASSERT(!isCalledFromLaunchersThread(), return nullptr);
  QMutexLocker locker(&m_mutex);
  if (m_handles.contains(token))
    return nullptr; // TODO: issue a warning

  CallerHandle *callerHandle = new CallerHandle(parent, token, mode);
  LauncherHandle *launcherHandle = new LauncherHandle(token, mode);
  callerHandle->setLauncherHandle(launcherHandle);
  launcherHandle->setCallerHandle(callerHandle);
  launcherHandle->moveToThread(thread());
  // Call it after moving LauncherHandle to the launcher's thread.
  // Since this method is invoked from caller's thread, CallerHandle will live in caller's thread.
  m_handles.insert(token, launcherHandle);
  connect(this, &LauncherSocket::ready, launcherHandle, &LauncherHandle::handleSocketReady);
  connect(this, &LauncherSocket::errorOccurred, launcherHandle, &LauncherHandle::handleSocketError);

  return callerHandle;
}

auto LauncherSocket::unregisterHandle(quintptr token) -> void
{
  QTC_ASSERT(!isCalledFromLaunchersThread(), return);
  QMutexLocker locker(&m_mutex);
  auto it = m_handles.find(token);
  if (it == m_handles.end())
    return; // TODO: issue a warning

  LauncherHandle *launcherHandle = it.value();
  CallerHandle *callerHandle = launcherHandle->callerHandle();
  launcherHandle->setCallerHandle(nullptr);
  callerHandle->setLauncherHandle(nullptr);
  launcherHandle->deleteLater();
  callerHandle->deleteLater();
  m_handles.erase(it);
}

auto LauncherSocket::handleForToken(quintptr token) const -> LauncherHandle*
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return nullptr);
  QMutexLocker locker(&m_mutex);
  return m_handles.value(token);
}

auto LauncherSocket::setSocket(QLocalSocket *socket) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  QTC_ASSERT(!m_socket, return);
  m_socket.store(socket);
  m_packetParser.setDevice(m_socket);
  connect(m_socket,
          #if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
            static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
          #else
          &QLocalSocket::errorOccurred,
          #endif
          this, &LauncherSocket::handleSocketError);
  connect(m_socket, &QLocalSocket::readyRead, this, &LauncherSocket::handleSocketDataAvailable);
  connect(m_socket, &QLocalSocket::disconnected, this, &LauncherSocket::handleSocketDisconnected);
  emit ready();
}

auto LauncherSocket::shutdown() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  const auto socket = m_socket.exchange(nullptr);
  if (!socket)
    return;
  socket->disconnect();
  socket->write(ShutdownPacket().serialize());
  socket->waitForBytesWritten(1000);
  socket->deleteLater(); // or schedule a queued call to delete later?
}

auto LauncherSocket::handleSocketError() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  auto socket = m_socket.load();
  if (socket->error() != QLocalSocket::PeerClosedError)
    handleError(QCoreApplication::translate("Utils::LauncherSocket", "Socket error: %1").arg(socket->errorString()));
}

auto LauncherSocket::handleSocketDataAvailable() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  try {
    if (!m_packetParser.parse())
      return;
  } catch (const PacketParser::InvalidPacketSizeException &e) {
    handleError(QCoreApplication::translate("Utils::LauncherSocket", "Internal protocol error: invalid packet size %1.").arg(e.size));
    return;
  }
  LauncherHandle *handle = handleForToken(m_packetParser.token());
  if (handle) {
    switch (m_packetParser.type()) {
    case LauncherPacketType::ProcessError:
    case LauncherPacketType::ProcessStarted:
    case LauncherPacketType::ReadyReadStandardOutput:
    case LauncherPacketType::ReadyReadStandardError:
    case LauncherPacketType::ProcessFinished:
      handle->handlePacket(m_packetParser.type(), m_packetParser.packetData());
      break;
    default:
      handleError(QCoreApplication::translate("Utils::LauncherSocket", "Internal protocol error: invalid packet type %1.").arg(static_cast<int>(m_packetParser.type())));
      return;
    }
  } else {
    //        qDebug() << "No handler for token" << m_packetParser.token() << m_handles;
    // in this case the QtcProcess was canceled and deleted
  }
  handleSocketDataAvailable();
}

auto LauncherSocket::handleSocketDisconnected() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  handleError(QCoreApplication::translate("Utils::LauncherSocket", "Launcher socket closed unexpectedly."));
}

auto LauncherSocket::handleError(const QString &error) -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  const auto socket = m_socket.exchange(nullptr);
  socket->disconnect();
  socket->deleteLater();
  emit errorOccurred(error);
}

auto LauncherSocket::handleRequests() -> void
{
  QTC_ASSERT(isCalledFromLaunchersThread(), return);
  const auto socket = m_socket.load();
  QTC_ASSERT(socket, return);
  QMutexLocker locker(&m_mutex);
  for (const QByteArray &request : qAsConst(m_requests))
    socket->write(request);
  m_requests.clear();
}

auto LauncherSocket::isCalledFromLaunchersThread() const -> bool
{
  return QThread::currentThread() == thread();
}

} // namespace Internal
} // namespace Utils
