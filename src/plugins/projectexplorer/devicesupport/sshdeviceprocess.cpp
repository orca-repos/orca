// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sshdeviceprocess.hpp"

#include "idevice.hpp"
#include "../runcontrol.hpp"

#include <core/core-interface.hpp>
#include <ssh/sshconnection.h>
#include <ssh/sshconnectionmanager.h>
#include <ssh/sshremoteprocess.h>
#include <utils/environment.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QString>
#include <QTimer>

using namespace Utils;

namespace ProjectExplorer {

enum class Signal {
  Interrupt,
  Terminate,
  Kill
};

class SshDeviceProcess::SshDeviceProcessPrivate {
public:
  SshDeviceProcessPrivate(SshDeviceProcess *q) : q(q) {}

  SshDeviceProcess *const q;
  bool ignoreSelfSignals = true;
  QSsh::SshConnection *connection = nullptr;
  QSsh::SshRemoteProcessPtr remoteProcess;
  Runnable runnable;
  QString errorMessage;
  QProcess::ExitStatus exitStatus = QProcess::NormalExit;
  DeviceProcessSignalOperation::Ptr killOperation;
  QTimer killTimer;

  enum State {
    Inactive,
    Connecting,
    Connected,
    ProcessRunning
  } state = Inactive;

  auto setState(State newState) -> void;
  auto doSignal(Signal signal) -> void;

  auto displayName() const -> QString
  {
    return runnable.extraData.value("Ssh.X11ForwardToDisplay").toString();
  }
};

SshDeviceProcess::SshDeviceProcess(const IDevice::ConstPtr &device, QObject *parent) : DeviceProcess(device, TerminalOn, parent), d(std::make_unique<SshDeviceProcessPrivate>(this))
{
  // Hack: we rely on fact that below slots were called before any other external slots connected
  // to this instance signals. That's why we don't re-emit them from inside our handlers since
  // these signal will reach all other external slots anyway after our handlers are done.
  connect(this, &QtcProcess::started, this, [this] {
    if (!d->ignoreSelfSignals)
      handleProcessStarted();
  });
  connect(this, &QtcProcess::finished, this, [this] {
    if (!d->ignoreSelfSignals)
      handleProcessFinished(QtcProcess::errorString());
  });
  connect(&d->killTimer, &QTimer::timeout, this, &SshDeviceProcess::handleKillOperationTimeout);
}

SshDeviceProcess::~SshDeviceProcess()
{
  d->setState(SshDeviceProcessPrivate::Inactive);
}

auto SshDeviceProcess::start(const Runnable &runnable) -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::Inactive, return);
  QTC_ASSERT(runInTerminal() || !runnable.command.isEmpty(), return);
  d->setState(SshDeviceProcessPrivate::Connecting);

  d->errorMessage.clear();
  d->exitStatus = QProcess::NormalExit;
  d->runnable = runnable;
  auto params = device()->sshParameters();
  params.x11DisplayName = d->displayName();
  d->connection = QSsh::SshConnectionManager::acquireConnection(params);
  connect(d->connection, &QSsh::SshConnection::errorOccurred, this, &SshDeviceProcess::handleConnectionError);
  connect(d->connection, &QSsh::SshConnection::disconnected, this, &SshDeviceProcess::handleDisconnected);
  if (d->connection->state() == QSsh::SshConnection::Connected) {
    handleConnected();
  } else {
    connect(d->connection, &QSsh::SshConnection::connected, this, &SshDeviceProcess::handleConnected);
    if (d->connection->state() == QSsh::SshConnection::Unconnected)
      d->connection->connectToHost();
  }
}

auto SshDeviceProcess::interrupt() -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
  d->doSignal(Signal::Interrupt);
}

auto SshDeviceProcess::terminate() -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
  d->doSignal(Signal::Terminate);
}

auto SshDeviceProcess::kill() -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
  d->doSignal(Signal::Kill);
}

auto SshDeviceProcess::state() const -> QProcess::ProcessState
{
  switch (d->state) {
  case SshDeviceProcessPrivate::Inactive:
    return QProcess::NotRunning;
  case SshDeviceProcessPrivate::Connecting:
  case SshDeviceProcessPrivate::Connected:
    return QProcess::Starting;
  case SshDeviceProcessPrivate::ProcessRunning:
    return QProcess::Running;
  default: QTC_CHECK(false);
    return QProcess::NotRunning;
  }
}

auto SshDeviceProcess::exitStatus() const -> QProcess::ExitStatus
{
  return d->exitStatus == QProcess::NormalExit && exitCode() != 255 ? QProcess::NormalExit : QProcess::CrashExit;
}

auto SshDeviceProcess::exitCode() const -> int
{
  return runInTerminal() ? QtcProcess::exitCode() : d->remoteProcess->exitCode();
}

auto SshDeviceProcess::errorString() const -> QString
{
  return d->errorMessage;
}

auto SshDeviceProcess::readAllStandardOutput() -> QByteArray
{
  return d->remoteProcess.get() ? d->remoteProcess->readAllStandardOutput() : QByteArray();
}

auto SshDeviceProcess::readAllStandardError() -> QByteArray
{
  return d->remoteProcess.get() ? d->remoteProcess->readAllStandardError() : QByteArray();
}

auto SshDeviceProcess::processId() const -> qint64
{
  return 0;
}

auto SshDeviceProcess::handleConnected() -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::Connecting, return);
  d->setState(SshDeviceProcessPrivate::Connected);

  d->remoteProcess = runInTerminal() && d->runnable.command.isEmpty() ? d->connection->createRemoteShell() : d->connection->createRemoteProcess(fullCommandLine(d->runnable));
  const auto display = d->displayName();
  if (!display.isEmpty())
    d->remoteProcess->requestX11Forwarding(display);
  d->ignoreSelfSignals = !runInTerminal();
  if (runInTerminal()) {
    setAbortOnMetaChars(false);
    setCommand(d->remoteProcess->fullLocalCommandLine(true));
    QtcProcess::start();
  } else {
    connect(d->remoteProcess.get(), &QSsh::SshRemoteProcess::started, this, &SshDeviceProcess::handleProcessStarted);
    connect(d->remoteProcess.get(), &QSsh::SshRemoteProcess::done, this, &SshDeviceProcess::handleProcessFinished);
    connect(d->remoteProcess.get(), &QSsh::SshRemoteProcess::readyReadStandardOutput, this, &QtcProcess::readyReadStandardOutput);
    connect(d->remoteProcess.get(), &QSsh::SshRemoteProcess::readyReadStandardError, this, &QtcProcess::readyReadStandardError);
    d->remoteProcess->start();
  }
}

auto SshDeviceProcess::handleConnectionError() -> void
{
  QTC_ASSERT(d->state != SshDeviceProcessPrivate::Inactive, return);

  d->errorMessage = d->connection->errorString();
  handleDisconnected();
}

auto SshDeviceProcess::handleDisconnected() -> void
{
  QTC_ASSERT(d->state != SshDeviceProcessPrivate::Inactive, return);
  const auto oldState = d->state;
  d->setState(SshDeviceProcessPrivate::Inactive);
  switch (oldState) {
  case SshDeviceProcessPrivate::Connecting:
  case SshDeviceProcessPrivate::Connected: emit errorOccurred(QProcess::FailedToStart);
    break;
  case SshDeviceProcessPrivate::ProcessRunning:
    d->exitStatus = QProcess::CrashExit;
    emit finished();
  default:
    break;
  }
}

auto SshDeviceProcess::handleProcessStarted() -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::Connected, return);

  d->setState(SshDeviceProcessPrivate::ProcessRunning);
  if (d->ignoreSelfSignals) emit started();
}

auto SshDeviceProcess::handleProcessFinished(const QString &error) -> void
{
  d->errorMessage = error;
  if (d->killOperation && error.isEmpty())
    d->errorMessage = tr("The process was ended forcefully.");
  d->setState(SshDeviceProcessPrivate::Inactive);
  if (d->ignoreSelfSignals) emit finished();
}

auto SshDeviceProcess::handleKillOperationFinished(const QString &errorMessage) -> void
{
  QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
  if (errorMessage.isEmpty()) // Process will finish as expected; nothing to do here.
    return;

  d->exitStatus = QProcess::CrashExit; // Not entirely true, but it will get the message across.
  d->errorMessage = tr("Failed to kill remote process: %1").arg(errorMessage);
  d->setState(SshDeviceProcessPrivate::Inactive);
  emit finished();
}

auto SshDeviceProcess::handleKillOperationTimeout() -> void
{
  d->exitStatus = QProcess::CrashExit; // Not entirely true, but it will get the message across.
  d->errorMessage = tr("Timeout waiting for remote process to finish.");
  d->setState(SshDeviceProcessPrivate::Inactive);
  emit finished();
}

auto SshDeviceProcess::fullCommandLine(const Runnable &runnable) const -> QString
{
  auto cmdLine = runnable.command.executable().toString();
  // FIXME: That quotes wrongly.
  if (!runnable.command.arguments().isEmpty())
    cmdLine.append(QLatin1Char(' ')).append(runnable.command.arguments());
  return cmdLine;
}

auto SshDeviceProcess::SshDeviceProcessPrivate::doSignal(Signal signal) -> void
{
  if (runnable.command.isEmpty())
    return;
  switch (state) {
  case Inactive: QTC_ASSERT(false, return);
    break;
  case Connecting:
    errorMessage = tr("Terminated by request.");
    setState(Inactive);
    emit q->errorOccurred(QProcess::FailedToStart);
    break;
  case Connected:
  case ProcessRunning:
    const auto signalOperation = q->device()->signalOperation();
    const auto processId = q->processId();
    if (signal == Signal::Interrupt) {
      if (processId != 0)
        signalOperation->interruptProcess(processId);
      else
        signalOperation->interruptProcess(runnable.command.executable().toString());
    } else {
      if (killOperation) // We are already in the process of killing the app.
        return;
      killOperation = signalOperation;
      connect(signalOperation.data(), &DeviceProcessSignalOperation::finished, q, &SshDeviceProcess::handleKillOperationFinished);
      killTimer.start(5000);
      if (processId != 0)
        signalOperation->killProcess(processId);
      else
        signalOperation->killProcess(runnable.command.executable().toString());
    }
    break;
  }
}

auto SshDeviceProcess::SshDeviceProcessPrivate::setState(State newState) -> void
{
  if (state == newState)
    return;

  state = newState;
  if (state != Inactive)
    return;

  if (killOperation) {
    killOperation->disconnect(q);
    killOperation.clear();
    if (q->runInTerminal())
      QMetaObject::invokeMethod(q, &QtcProcess::stopProcess, Qt::QueuedConnection);
  }
  killTimer.stop();
  if (remoteProcess)
    remoteProcess->disconnect(q);
  if (connection) {
    connection->disconnect(q);
    QSsh::SshConnectionManager::releaseConnection(connection);
    connection = nullptr;
  }
}

auto SshDeviceProcess::write(const QByteArray &data) -> qint64
{
  QTC_ASSERT(!runInTerminal(), return -1);
  return d->remoteProcess->write(data);
}

} // namespace ProjectExplorer
