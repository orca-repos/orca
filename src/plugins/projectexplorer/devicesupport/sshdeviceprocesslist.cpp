// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sshdeviceprocesslist.hpp"

#include "idevice.hpp"

#include <ssh/sshremoteprocessrunner.h>
#include <utils/qtcassert.hpp>
#include <utils/fileutils.hpp>

using namespace QSsh;

namespace ProjectExplorer {

class SshDeviceProcessList::SshDeviceProcessListPrivate {
public:
  SshRemoteProcessRunner process;
  DeviceProcessSignalOperation::Ptr signalOperation;
};

SshDeviceProcessList::SshDeviceProcessList(const IDevice::ConstPtr &device, QObject *parent) : DeviceProcessList(device, parent), d(std::make_unique<SshDeviceProcessListPrivate>()) {}

SshDeviceProcessList::~SshDeviceProcessList() = default;

auto SshDeviceProcessList::doUpdate() -> void
{
  connect(&d->process, &SshRemoteProcessRunner::connectionError, this, &SshDeviceProcessList::handleConnectionError);
  connect(&d->process, &SshRemoteProcessRunner::processClosed, this, &SshDeviceProcessList::handleListProcessFinished);
  d->process.run(listProcessesCommandLine(), device()->sshParameters());
}

auto SshDeviceProcessList::doKillProcess(const DeviceProcessItem &process) -> void
{
  d->signalOperation = device()->signalOperation();
  QTC_ASSERT(d->signalOperation, return);
  connect(d->signalOperation.data(), &DeviceProcessSignalOperation::finished, this, &SshDeviceProcessList::handleKillProcessFinished);
  d->signalOperation->killProcess(process.pid);
}

auto SshDeviceProcessList::handleConnectionError() -> void
{
  setFinished();
  reportError(tr("Connection failure: %1").arg(d->process.lastConnectionErrorString()));
}

auto SshDeviceProcessList::handleListProcessFinished(const QString &error) -> void
{
  setFinished();
  if (!error.isEmpty()) {
    handleProcessError(error);
    return;
  }
  if (d->process.processExitCode() == 0) {
    const QByteArray remoteStdout = d->process.readAllStandardOutput();
    const QString stdoutString = QString::fromUtf8(remoteStdout.data(), remoteStdout.count());
    reportProcessListUpdated(buildProcessList(stdoutString));
  } else {
    handleProcessError(tr("Process listing command failed with exit code %1.").arg(d->process.processExitCode()));
  }
}

auto SshDeviceProcessList::handleKillProcessFinished(const QString &errorString) -> void
{
  if (errorString.isEmpty())
    reportProcessKilled();
  else
    reportError(tr("Error: Kill process failed: %1").arg(errorString));
  setFinished();
}

auto SshDeviceProcessList::handleProcessError(const QString &errorMessage) -> void
{
  auto fullMessage = errorMessage;
  const QByteArray remoteStderr = d->process.readAllStandardError();
  if (!remoteStderr.isEmpty())
    fullMessage += QLatin1Char('\n') + tr("Remote stderr was: %1").arg(QString::fromUtf8(remoteStderr));
  reportError(fullMessage);
}

auto SshDeviceProcessList::setFinished() -> void
{
  d->process.disconnect(this);
  if (d->signalOperation) {
    d->signalOperation->disconnect(this);
    d->signalOperation.clear();
  }
}

} // namespace ProjectExplorer
