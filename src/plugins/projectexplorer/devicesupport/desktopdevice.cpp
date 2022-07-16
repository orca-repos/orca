// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "desktopdevice.hpp"
#include "desktopdeviceprocess.hpp"
#include "deviceprocesslist.hpp"
#include "localprocesslist.hpp"
#include "desktopprocesssignaloperation.hpp"

#include <core/core-file-utils.hpp>

#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/runcontrol.hpp>

#include <ssh/sshconnection.h>

#include <utils/environment.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/portlist.hpp>
#include <utils/stringutils.hpp>
#include <utils/url.hpp>

#include <QCoreApplication>
#include <QDateTime>

using namespace ProjectExplorer::Constants;
using namespace Utils;

namespace ProjectExplorer {

DesktopDevice::DesktopDevice()
{
  setupId(AutoDetected, DESKTOP_DEVICE_ID);
  setType(DESKTOP_DEVICE_TYPE);
  setDefaultDisplayName(tr("Local PC"));
  setDisplayType(QCoreApplication::translate("ProjectExplorer::DesktopDevice", "Desktop"));

  setDeviceState(DeviceStateUnknown);
  setMachineType(Hardware);
  setOsType(HostOsInfo::hostOs());

  const auto portRange = QString::fromLatin1("%1-%2").arg(DESKTOP_PORT_START).arg(DESKTOP_PORT_END);
  setFreePorts(PortList::fromString(portRange));
  setOpenTerminal([](const Environment &env, const FilePath &workingDir) {
    Orca::Plugin::Core::FileUtils::openTerminal(workingDir, env);
  });
}

auto DesktopDevice::deviceInformation() const -> DeviceInfo
{
  return DeviceInfo();
}

auto DesktopDevice::createWidget() -> IDeviceWidget*
{
  return nullptr;
  // DesktopDeviceConfigurationWidget currently has just one editable field viz. free ports.
  // Querying for an available port is quite straightforward. Having a field for the port
  // range can be confusing to the user. Hence, disabling the widget for now.
}

auto DesktopDevice::canAutoDetectPorts() const -> bool
{
  return true;
}

auto DesktopDevice::canCreateProcessModel() const -> bool
{
  return true;
}

auto DesktopDevice::createProcessListModel(QObject *parent) const -> DeviceProcessList*
{
  return new Internal::LocalProcessList(sharedFromThis(), parent);
}

auto DesktopDevice::createProcess(QObject *parent) const -> DeviceProcess*
{
  return new Internal::DesktopDeviceProcess(sharedFromThis(), parent);
}

auto DesktopDevice::signalOperation() const -> DeviceProcessSignalOperation::Ptr
{
  return DeviceProcessSignalOperation::Ptr(new DesktopProcessSignalOperation());
}

class DesktopDeviceEnvironmentFetcher : public DeviceEnvironmentFetcher {
public:
  DesktopDeviceEnvironmentFetcher() = default;

  auto start() -> void override
  {
    emit finished(Environment::systemEnvironment(), true);
  }
};

auto DesktopDevice::environmentFetcher() const -> DeviceEnvironmentFetcher::Ptr
{
  return DeviceEnvironmentFetcher::Ptr(new DesktopDeviceEnvironmentFetcher());
}

class DesktopPortsGatheringMethod : public PortsGatheringMethod {
  auto commandLine(QAbstractSocket::NetworkLayerProtocol protocol) const -> CommandLine override
  {
    // We might encounter the situation that protocol is given IPv6
    // but the consumer of the free port information decides to open
    // an IPv4(only) port. As a result the next IPv6 scan will
    // report the port again as open (in IPv6 namespace), while the
    // same port in IPv4 namespace might still be blocked, and
    // re-use of this port fails.
    // GDBserver behaves exactly like this.

    Q_UNUSED(protocol)

    if (HostOsInfo::isWindowsHost() || HostOsInfo::isMacHost())
      return {"netstat", {"-a", "-n"}};
    if (HostOsInfo::isLinuxHost())
      return {"/bin/sh", {"-c", "cat /proc/net/tcp*"}};
    return {};
  }

  auto usedPorts(const QByteArray &output) const -> QList<Port> override
  {
    QList<Port> ports;
    const auto lines = output.split('\n');
    for (const auto &line : lines) {
      const Port port(parseUsedPortFromNetstatOutput(line));
      if (port.isValid() && !ports.contains(port))
        ports.append(port);
    }
    return ports;
  }
};

auto DesktopDevice::portsGatheringMethod() const -> PortsGatheringMethod::Ptr
{
  return DesktopPortsGatheringMethod::Ptr(new DesktopPortsGatheringMethod);
}

auto DesktopDevice::toolControlChannel(const ControlChannelHint &) const -> QUrl
{
  QUrl url;
  url.setScheme(urlTcpScheme());
  url.setHost("localhost");
  return url;
}

auto DesktopDevice::handlesFile(const FilePath &filePath) const -> bool
{
  return !filePath.needsDevice();
}

auto DesktopDevice::iterateDirectory(const FilePath &filePath, const std::function<bool(const FilePath &)> &callBack, const FileFilter &filter) const -> void
{
  QTC_CHECK(!filePath.needsDevice());
  filePath.iterateDirectory(callBack, filter);
}

auto DesktopDevice::fileSize(const FilePath &filePath) const -> qint64
{
  QTC_ASSERT(handlesFile(filePath), return -1);
  return filePath.fileSize();
}

auto DesktopDevice::permissions(const FilePath &filePath) const -> QFile::Permissions
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.permissions();
}

auto DesktopDevice::setPermissions(const FilePath &filePath, QFile::Permissions permissions) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.setPermissions(permissions);
}

auto DesktopDevice::systemEnvironment() const -> Environment
{
  return Environment::systemEnvironment();
}

auto DesktopDevice::isExecutableFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isExecutableFile();
}

auto DesktopDevice::isReadableFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isReadableFile();
}

auto DesktopDevice::isWritableFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isWritableFile();
}

auto DesktopDevice::isReadableDirectory(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isReadableDir();
}

auto DesktopDevice::isWritableDirectory(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isWritableDir();
}

auto DesktopDevice::isFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isFile();
}

auto DesktopDevice::isDirectory(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.isDir();
}

auto DesktopDevice::createDirectory(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.createDir();
}

auto DesktopDevice::exists(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.exists();
}

auto DesktopDevice::ensureExistingFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.ensureExistingFile();
}

auto DesktopDevice::removeFile(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.removeFile();
}

auto DesktopDevice::removeRecursively(const FilePath &filePath) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.removeRecursively();
}

auto DesktopDevice::copyFile(const FilePath &filePath, const FilePath &target) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  return filePath.copyFile(target);
}

auto DesktopDevice::renameFile(const FilePath &filePath, const FilePath &target) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return false);
  QTC_ASSERT(handlesFile(target), return false);
  return filePath.renameFile(target);
}

auto DesktopDevice::lastModified(const FilePath &filePath) const -> QDateTime
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.lastModified();
}

auto DesktopDevice::symLinkTarget(const FilePath &filePath) const -> FilePath
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.symLinkTarget();
}

auto DesktopDevice::fileContents(const FilePath &filePath, qint64 limit, qint64 offset) const -> QByteArray
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.fileContents(limit, offset);
}

auto DesktopDevice::writeFileContents(const FilePath &filePath, const QByteArray &data) const -> bool
{
  QTC_ASSERT(handlesFile(filePath), return {});
  return filePath.writeFileContents(data);
}

} // namespace ProjectExplorer
