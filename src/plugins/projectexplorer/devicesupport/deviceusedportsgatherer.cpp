// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deviceprocess.hpp"
#include "deviceusedportsgatherer.hpp"

#include <ssh/sshconnection.h>

#include <utils/port.hpp>
#include <utils/portlist.hpp>
#include <utils/qtcassert.hpp>
#include <utils/url.hpp>

#include <QPointer>

using namespace QSsh;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class DeviceUsedPortsGathererPrivate {
public:
  QPointer<DeviceProcess> process;
  QList<Port> usedPorts;
  QByteArray remoteStdout;
  QByteArray remoteStderr;
  IDevice::ConstPtr device;
  PortsGatheringMethod::Ptr portsGatheringMethod;
};

} // namespace Internal

DeviceUsedPortsGatherer::DeviceUsedPortsGatherer(QObject *parent) : QObject(parent), d(new Internal::DeviceUsedPortsGathererPrivate) {}

DeviceUsedPortsGatherer::~DeviceUsedPortsGatherer()
{
  stop();
  delete d;
}

auto DeviceUsedPortsGatherer::start(const IDevice::ConstPtr &device) -> void
{
  d->usedPorts.clear();
  d->device = device;
  QTC_ASSERT(d->device, emit error("No device given"); return);

  d->portsGatheringMethod = d->device->portsGatheringMethod();
  QTC_ASSERT(d->portsGatheringMethod, emit error("Not implemented"); return);

  const auto protocol = QAbstractSocket::AnyIPProtocol;
  d->process = d->device->createProcess(this);

  connect(d->process.data(), &DeviceProcess::finished, this, &DeviceUsedPortsGatherer::handleProcessFinished);
  connect(d->process.data(), &DeviceProcess::errorOccurred, this, &DeviceUsedPortsGatherer::handleProcessError);
  connect(d->process.data(), &DeviceProcess::readyReadStandardOutput, this, &DeviceUsedPortsGatherer::handleRemoteStdOut);
  connect(d->process.data(), &DeviceProcess::readyReadStandardError, this, &DeviceUsedPortsGatherer::handleRemoteStdErr);

  Runnable runnable;
  runnable.command = d->portsGatheringMethod->commandLine(protocol);
  d->process->start(runnable);
}

auto DeviceUsedPortsGatherer::stop() -> void
{
  d->remoteStdout.clear();
  d->remoteStderr.clear();
  if (d->process)
    disconnect(d->process.data(), nullptr, this, nullptr);
  d->process.clear();
}

auto DeviceUsedPortsGatherer::getNextFreePort(PortList *freePorts) const -> Port
{
  while (freePorts->hasMore()) {
    const auto port = freePorts->getNext();
    if (!d->usedPorts.contains(port))
      return port;
  }
  return Port();
}

auto DeviceUsedPortsGatherer::usedPorts() const -> QList<Port>
{
  return d->usedPorts;
}

auto DeviceUsedPortsGatherer::setupUsedPorts() -> void
{
  d->usedPorts.clear();
  const auto usedPorts = d->portsGatheringMethod->usedPorts(d->remoteStdout);
  foreach(const Port port, usedPorts) {
    if (d->device->freePorts().contains(port))
      d->usedPorts << port;
  }
  emit portListReady();
}

auto DeviceUsedPortsGatherer::handleProcessError() -> void
{
  emit error(tr("Connection error: %1").arg(d->process->errorString()));
  stop();
}

auto DeviceUsedPortsGatherer::handleProcessFinished() -> void
{
  if (!d->process)
    return;
  QString errMsg;
  const auto exitStatus = d->process->exitStatus();
  switch (exitStatus) {
  case QProcess::CrashExit:
    errMsg = tr("Remote process crashed: %1").arg(d->process->errorString());
    break;
  case QProcess::NormalExit:
    if (d->process->exitCode() == 0)
      setupUsedPorts();
    else
      errMsg = tr("Remote process failed; exit code was %1.").arg(d->process->exitCode());
    break;
  default: Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid exit status");
  }

  if (!errMsg.isEmpty()) {
    if (!d->remoteStderr.isEmpty()) {
      errMsg += QLatin1Char('\n');
      errMsg += tr("Remote error output was: %1").arg(QString::fromUtf8(d->remoteStderr));
    }
    emit error(errMsg);
  }
  stop();
}

auto DeviceUsedPortsGatherer::handleRemoteStdOut() -> void
{
  if (d->process)
    d->remoteStdout += d->process->readAllStandardOutput();
}

auto DeviceUsedPortsGatherer::handleRemoteStdErr() -> void
{
  if (d->process)
    d->remoteStderr += d->process->readAllStandardError();
}

// PortGatherer

PortsGatherer::PortsGatherer(RunControl *runControl) : RunWorker(runControl)
{
  setId("PortGatherer");

  connect(&m_portsGatherer, &DeviceUsedPortsGatherer::error, this, &PortsGatherer::reportFailure);
  connect(&m_portsGatherer, &DeviceUsedPortsGatherer::portListReady, this, [this] {
    m_portList = device()->freePorts();
    appendMessage(tr("Found %n free ports.", nullptr, m_portList.count()), NormalMessageFormat);
    reportStarted();
  });
}

PortsGatherer::~PortsGatherer() = default;

auto PortsGatherer::start() -> void
{
  appendMessage(tr("Checking available ports..."), NormalMessageFormat);
  m_portsGatherer.start(device());
}

auto PortsGatherer::findEndPoint() -> QUrl
{
  QUrl result;
  result.setScheme(urlTcpScheme());
  result.setHost(device()->sshParameters().host());
  result.setPort(m_portsGatherer.getNextFreePort(&m_portList).number());
  return result;
}

auto PortsGatherer::stop() -> void
{
  m_portsGatherer.stop();
  reportStopped();
}

// ChannelForwarder

/*!
    \class ProjectExplorer::ChannelForwarder

    \internal

    \brief The class provides a \c RunWorker handling the forwarding
    from one device to another.

    Both endpoints are specified by \c{QUrl}s, typically with
    a "tcp" or "socket" scheme.
*/

ChannelForwarder::ChannelForwarder(RunControl *runControl) : RunWorker(runControl) {}

auto ChannelForwarder::setFromUrlGetter(const UrlGetter &urlGetter) -> void
{
  m_fromUrlGetter = urlGetter;
}

namespace Internal {

// SubChannelProvider

/*!
    \class ProjectExplorer::SubChannelProvider

    \internal

    This is a helper RunWorker implementation to either use or not
    use port forwarding for one SubChannel in the ChannelProvider
    implementation.

    A device implementation can provide a  "ChannelForwarder"
    RunWorker non-trivial implementation if needed.

    By default it is assumed that no forwarding is needed, i.e.
    end points provided by the shared endpoint resource provider
    are directly accessible.
*/

class SubChannelProvider : public RunWorker {
public:
  SubChannelProvider(RunControl *runControl, RunWorker *sharedEndpointGatherer) : RunWorker(runControl)
  {
    setId("SubChannelProvider");

    m_portGatherer = qobject_cast<PortsGatherer*>(sharedEndpointGatherer);
    if (m_portGatherer) {
      if (const auto forwarder = runControl->createWorker("ChannelForwarder")) {
        m_channelForwarder = qobject_cast<ChannelForwarder*>(forwarder);
        if (m_channelForwarder) {
          m_channelForwarder->addStartDependency(m_portGatherer);
          m_channelForwarder->setFromUrlGetter([this] {
            return m_portGatherer->findEndPoint();
          });
          addStartDependency(m_channelForwarder);
        }
      }
    }
  }

  auto start() -> void final
  {
    m_channel.setScheme(urlTcpScheme());
    m_channel.setHost(device()->toolControlChannel(IDevice::ControlChannelHint()).host());
    if (m_channelForwarder)
      m_channel.setPort(m_channelForwarder->recordedData("LocalPort").toUInt());
    else if (m_portGatherer)
      m_channel.setPort(m_portGatherer->findEndPoint().port());
    reportStarted();
  }

  auto channel() const -> QUrl { return m_channel; }

private:
  QUrl m_channel;
  PortsGatherer *m_portGatherer = nullptr;
  ChannelForwarder *m_channelForwarder = nullptr;
};

} // Internal

// ChannelProvider

/*!
    \class ProjectExplorer::ChannelProvider

    \internal

    The class implements a \c RunWorker to provide
    to provide a set of urls indicating usable connection end
    points for 'server-using' tools (typically one, like plain
    gdbserver and the Qml tooling, but two for mixed debugging).

    Urls can describe local or tcp servers that are directly
    accessible to the host tools.

    The tool implementations can assume that any needed port
    forwarding setup is setup and handled transparently by
    a \c ChannelProvider instance.

    If there are multiple subchannels needed that need to share a
    common set of resources on the remote side, a device implementation
    can provide a "SharedEndpointGatherer" RunWorker.

    If none is provided, it is assumed that the shared resource
    is open TCP ports, provided by the device's PortGatherer i
    implementation.

    FIXME: The current implementation supports only the case
    of "any number of TCP channels that do not need actual
    forwarding.
*/

ChannelProvider::ChannelProvider(RunControl *runControl, int requiredChannels) : RunWorker(runControl)
{
  setId("ChannelProvider");

  auto sharedEndpoints = runControl->createWorker("SharedEndpointGatherer");
  if (!sharedEndpoints) {
    // null is a legit value indicating 'no need to share'.
    sharedEndpoints = new PortsGatherer(runControl);
  }

  for (auto i = 0; i < requiredChannels; ++i) {
    const auto channelProvider = new Internal::SubChannelProvider(runControl, sharedEndpoints);
    m_channelProviders.append(channelProvider);
    addStartDependency(channelProvider);
  }
}

ChannelProvider::~ChannelProvider() = default;

auto ChannelProvider::channel(int i) const -> QUrl
{
  if (const auto provider = m_channelProviders.value(i))
    return provider->channel();
  return QUrl();
}

} // namespace ProjectExplorer
