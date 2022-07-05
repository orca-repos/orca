// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "launcherpackets.h"
#include "processutils.h"

#include <QDeadlineTimer>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QWaitCondition>

#include <atomic>
#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QLocalSocket;
QT_END_NAMESPACE

namespace Utils {
namespace Internal {

class LauncherInterfacePrivate;
class LauncherHandle;
class LauncherSignal;
class ErrorSignal;
class StartedSignal;
class ReadyReadSignal;
class FinishedSignal;

// All the methods and data fields in this class are called / accessed from the caller's thread.
// Exceptions are explicitly marked.
class CallerHandle : public QObject {
  Q_OBJECT public:
  enum class SignalType {
    NoSignal,
    Error,
    Started,
    ReadyRead,
    Finished
  };

  Q_ENUM(SignalType)
  CallerHandle(QObject *parent, quintptr token, ProcessMode mode) : QObject(parent), m_token(token), m_processMode(mode) {}
  ~CallerHandle() override;

  auto launcherHandle() const -> LauncherHandle* { return m_launcherHandle; }

  auto setLauncherHandle(LauncherHandle *handle) -> void
  {
    QMutexLocker locker(&m_mutex);
    m_launcherHandle = handle;
  }

  auto waitForStarted(int msecs) -> bool;
  auto waitForReadyRead(int msces) -> bool;
  auto waitForFinished(int msecs) -> bool;

  // Returns the list of flushed signals.
  auto flush() -> QList<SignalType>;
  auto flushFor(SignalType signalType) -> QList<SignalType>;
  auto shouldFlushFor(SignalType signalType) const -> bool;
  // Called from launcher's thread exclusively.
  auto appendSignal(LauncherSignal *launcherSignal) -> void;

  // Called from caller's or launcher's thread.
  auto state() const -> QProcess::ProcessState;
  auto cancel() -> void;
  auto readAllStandardOutput() -> QByteArray;
  auto readAllStandardError() -> QByteArray;
  auto processId() const -> qint64;
  auto exitCode() const -> int;
  auto errorString() const -> QString;
  auto setErrorString(const QString &str) -> void;
  auto start(const QString &program, const QStringList &arguments, const QByteArray &writeData) -> void;
  // Called from caller's or launcher's thread.
  auto startIfNeeded() -> void;
  auto write(const QByteArray &data) -> qint64;
  auto error() const -> QProcess::ProcessError;
  // Called from caller's or launcher's thread.
  auto program() const -> QString;
  // Called from caller's or launcher's thread.
  auto arguments() const -> QStringList;
  auto setStandardInputFile(const QString &fileName) -> void;
  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void;
  auto setProcessEnvironment(const QProcessEnvironment &environment) -> void;
  auto setWorkingDirectory(const QString &dir) -> void;
  auto exitStatus() const -> QProcess::ExitStatus;

  auto setBelowNormalPriority() -> void;
  auto setNativeArguments(const QString &arguments) -> void;
  auto setLowPriority() -> void;
  auto setUnixTerminalDisabled() -> void;

signals:
  auto errorOccurred(QProcess::ProcessError error) -> void;
  auto started() -> void;
  auto finished(int exitCode, QProcess::ExitStatus status) -> void;
  auto readyReadStandardOutput() -> void;
  auto readyReadStandardError() -> void;

private:
  auto waitForSignal(int msecs, CallerHandle::SignalType newSignal) -> bool;
  auto canWaitFor(SignalType newSignal) const -> bool; // TODO: employ me before calling waitForSignal()

  // Called from caller's or launcher's thread. Call me with mutex locked.
  auto doStart() -> void;
  // Called from caller's or launcher's thread.
  auto sendPacket(const Internal::LauncherPacket &packet) -> void;
  // Called from caller's or launcher's thread.
  auto isCalledFromCallersThread() const -> bool;
  // Called from caller's or launcher's thread. Call me with mutex locked.
  auto isCalledFromLaunchersThread() const -> bool;

  auto readAndClear(QByteArray &data) const -> QByteArray
  {
    const QByteArray tmp = data;
    data.clear();
    return tmp;
  }

  auto handleError(const ErrorSignal *launcherSignal) -> void;
  auto handleStarted(const StartedSignal *launcherSignal) -> void;
  auto handleReadyRead(const ReadyReadSignal *launcherSignal) -> void;
  auto handleFinished(const FinishedSignal *launcherSignal) -> void;

  // Lives in launcher's thread. Modified from caller's thread.
  LauncherHandle *m_launcherHandle = nullptr;

  mutable QMutex m_mutex;
  // Accessed from caller's and launcher's thread
  QList<LauncherSignal*> m_signals;

  const quintptr m_token;
  const ProcessMode m_processMode;

  // Modified from caller's thread, read from launcher's thread
  std::atomic<QProcess::ProcessState> m_processState = QProcess::NotRunning;
  std::unique_ptr<StartProcessPacket> m_startPacket;
  int m_processId = 0;
  int m_exitCode = 0;
  QProcess::ExitStatus m_exitStatus = QProcess::ExitStatus::NormalExit;
  QByteArray m_stdout;
  QByteArray m_stderr;
  QString m_errorString;
  QProcess::ProcessError m_error = QProcess::UnknownError;
  QString m_command;
  QStringList m_arguments;
  QProcessEnvironment m_environment;
  QString m_workingDirectory;
  QByteArray m_writeData;
  QProcess::ProcessChannelMode m_channelMode = QProcess::SeparateChannels;
  QString m_standardInputFile;
  bool m_belowNormalPriority = false;
  QString m_nativeArguments;
  bool m_lowPriority = false;
  bool m_unixTerminalDisabled = false;
};

// Moved to the launcher thread, returned to caller's thread.
// It's assumed that this object will be alive at least
// as long as the corresponding QtcProcess is alive.

class LauncherHandle : public QObject {
  Q_OBJECT public:
  // Called from caller's thread, moved to launcher's thread afterwards.
  LauncherHandle(quintptr token, ProcessMode) : m_token(token) {}
  // Called from caller's thread exclusively.
  auto waitForSignal(int msecs, CallerHandle::SignalType newSignal) -> bool;
  auto callerHandle() const -> CallerHandle* { return m_callerHandle; }

  auto setCallerHandle(CallerHandle *handle) -> void
  {
    QMutexLocker locker(&m_mutex);
    m_callerHandle = handle;
  }

  // Called from caller's thread exclusively.
  auto setCanceled() -> void { m_awaitingShouldContinue = false; }

  // Called from launcher's thread exclusively.
  auto handleSocketReady() -> void;
  auto handleSocketError(const QString &message) -> void;
  auto handlePacket(LauncherPacketType type, const QByteArray &payload) -> void;

  // Called from caller's thread exclusively.
  auto isSocketError() const -> bool { return m_socketError; }

private:
  // Called from caller's thread exclusively.
  auto doWaitForSignal(QDeadlineTimer deadline, CallerHandle::SignalType newSignal) -> bool;
  // Called from launcher's thread exclusively. Call me with mutex locked.
  auto wakeUpIfWaitingFor(CallerHandle::SignalType newSignal) -> void;

  // Called from launcher's thread exclusively. Call me with mutex locked.
  auto flushCaller() -> void;
  // Called from launcher's thread exclusively.
  auto handleErrorPacket(const QByteArray &packetData) -> void;
  auto handleStartedPacket(const QByteArray &packetData) -> void;
  auto handleReadyReadStandardOutput(const QByteArray &packetData) -> void;
  auto handleReadyReadStandardError(const QByteArray &packetData) -> void;
  auto handleFinishedPacket(const QByteArray &packetData) -> void;

  // Called from caller's or launcher's thread.
  auto isCalledFromLaunchersThread() const -> bool;
  auto isCalledFromCallersThread() const -> bool;

  // Lives in caller's thread. Modified only in caller's thread. TODO: check usages - all should be with mutex
  CallerHandle *m_callerHandle = nullptr;

  // Modified only in caller's thread.
  bool m_awaitingShouldContinue = false;
  mutable QMutex m_mutex;
  QWaitCondition m_waitCondition;
  const quintptr m_token;
  std::atomic_bool m_socketError = false;
  // Modified only in caller's thread.
  CallerHandle::SignalType m_waitingFor = CallerHandle::SignalType::NoSignal;
};

class LauncherSocket : public QObject {
  Q_OBJECT friend class LauncherInterfacePrivate;
public:
  // Called from caller's or launcher's thread.
  auto isReady() const -> bool { return m_socket.load(); }
  auto sendData(const QByteArray &data) -> void;

  // Called from caller's thread exclusively.
  auto registerHandle(QObject *parent, quintptr token, ProcessMode mode) -> CallerHandle*;
  auto unregisterHandle(quintptr token) -> void;

signals:
  auto ready() -> void;
  auto errorOccurred(const QString &error) -> void;

private:
  // Called from caller's thread, moved to launcher's thread.
  LauncherSocket(QObject *parent = nullptr);
  // Called from launcher's thread exclusively.
  ~LauncherSocket() override;

  // Called from launcher's thread exclusively.
  auto handleForToken(quintptr token) const -> LauncherHandle*;

  // Called from launcher's thread exclusively.
  auto setSocket(QLocalSocket *socket) -> void;
  auto shutdown() -> void;

  // Called from launcher's thread exclusively.
  auto handleSocketError() -> void;
  auto handleSocketDataAvailable() -> void;
  auto handleSocketDisconnected() -> void;
  auto handleError(const QString &error) -> void;
  auto handleRequests() -> void;

  // Called from caller's or launcher's thread.
  auto isCalledFromLaunchersThread() const -> bool;

  std::atomic<QLocalSocket*> m_socket{nullptr};
  PacketParser m_packetParser;
  std::vector<QByteArray> m_requests;
  mutable QMutex m_mutex;
  QHash<quintptr, LauncherHandle*> m_handles;
};

} // namespace Internal
} // namespace Utils
