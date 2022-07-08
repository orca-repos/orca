// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "idevice.hpp"

#include "devicemanager.hpp"
#include "deviceprocesslist.hpp"
#include "idevicefactory.hpp"

#include "../kit.hpp"
#include "../kitinformation.hpp"
#include "../runconfiguration.hpp"

#include <core/icore.hpp>
#include <ssh/sshconnection.h>
#include <utils/displayname.hpp>
#include <utils/icon.hpp>
#include <utils/portlist.hpp>
#include <utils/qtcassert.hpp>
#include <utils/url.hpp>

#include <QCoreApplication>
#include <QStandardPaths>

#include <QDateTime>
#include <QString>
#include <QUuid>

/*!
 * \class ProjectExplorer::IDevice::DeviceAction
 * \brief The DeviceAction class describes an action that can be run on a device.
 *
 * The description consists of a human-readable string that will be displayed
 * on a button which, when clicked, executes a functor, and the functor itself.
 * This is typically some sort of dialog or wizard, so \a parent widget is provided.
 */

/*!
 * \class ProjectExplorer::IDevice
 * \brief The IDevice class is the base class for all devices.
 *
 * The term \e device refers to some host to which files can be deployed or on
 * which an application can run, for example.
 * In the typical case, this would be some sort of embedded computer connected in some way to
 * the PC on which \QC runs. This class itself does not specify a connection
 * protocol; that
 * kind of detail is to be added by subclasses.
 * Devices are managed by a \c DeviceManager.
 * \sa ProjectExplorer::DeviceManager
 */

/*!
 * \fn Utils::Id ProjectExplorer::IDevice::invalidId()
 * A value that no device can ever have as its internal id.
 */

/*!
 * \fn QString ProjectExplorer::IDevice::displayType() const
 * Prints a representation of the device's type suitable for displaying to a
 * user.
 */

/*!
 * \fn ProjectExplorer::IDeviceWidget *ProjectExplorer::IDevice::createWidget()
 * Creates a widget that displays device information not part of the IDevice base class.
 *        The widget can also be used to let the user change these attributes.
 */

/*!
 * \fn void ProjectExplorer::IDevice::addDeviceAction(const DeviceAction &deviceAction)
 * Adds an actions that can be run on this device.
 * These actions will be available in the \gui Devices options page.
 */

/*!
 * \fn ProjectExplorer::IDevice::Ptr ProjectExplorer::IDevice::clone() const
 * Creates an identical copy of a device object.
 */

using namespace Utils;

namespace ProjectExplorer {

static auto newId() -> Id
{
    return Id::fromString(QUuid::createUuid().toString());
}

constexpr char DisplayNameKey[] = "Name";
constexpr char TypeKey[] = "OsType";
constexpr char IdKey[] = "InternalId";
constexpr char OriginKey[] = "Origin";
constexpr char MachineTypeKey[] = "Type";
constexpr char VersionKey[] = "Version";
constexpr char ExtraDataKey[] = "ExtraData";

// Connection
constexpr char HostKey[] = "Host";
constexpr char SshPortKey[] = "SshPort";
constexpr char PortsSpecKey[] = "FreePortsSpec";
constexpr char UserNameKey[] = "Uname";
constexpr char AuthKey[] = "Authentication";
constexpr char KeyFileKey[] = "KeyFile";
constexpr char TimeoutKey[] = "Timeout";
constexpr char HostKeyCheckingKey[] = "HostKeyChecking";
constexpr char DebugServerKey[] = "DebugServerKey";
constexpr char QmlRuntimeKey[] = "QmlsceneKey";

using AuthType = QSsh::SshConnectionParameters::AuthenticationType;
const AuthType DefaultAuthType = QSsh::SshConnectionParameters::AuthenticationTypeAll;
const IDevice::MachineType DefaultMachineType = IDevice::Hardware;

constexpr int DefaultTimeout = 10;

namespace Internal {

class IDevicePrivate {
public:
  IDevicePrivate() = default;

  DisplayName displayName;
  QString displayType;
  Id type;
  IDevice::Origin origin = IDevice::AutoDetected;
  Id id;
  IDevice::DeviceState deviceState = IDevice::DeviceStateUnknown;
  IDevice::MachineType machineType = IDevice::Hardware;
  OsType osType = OsTypeOther;
  int version = 0; // This is used by devices that have been added by the SDK.
  QSsh::SshConnectionParameters sshParameters;
  PortList freePorts;
  FilePath debugServerPath;
  FilePath debugDumperPath = Core::ICore::resourcePath("debugger/");
  FilePath qmlRunCommand;
  bool emptyCommandAllowed = false;
  QList<Icon> deviceIcons;
  QList<IDevice::DeviceAction> deviceActions;
  QVariantMap extraData;
  IDevice::OpenTerminal openTerminal;
};

} // namespace Internal

DeviceTester::DeviceTester(QObject *parent) : QObject(parent) { }

IDevice::IDevice() : d(new Internal::IDevicePrivate) {}

auto IDevice::setOpenTerminal(const OpenTerminal &openTerminal) -> void
{
  d->openTerminal = openTerminal;
}

auto IDevice::setupId(Origin origin, Id id) -> void
{
  d->origin = origin;
  QTC_CHECK(origin == ManuallyAdded || id.isValid());
  d->id = id.isValid() ? id : newId();
}

auto IDevice::canOpenTerminal() const -> bool
{
  return bool(d->openTerminal);
}

auto IDevice::openTerminal(const Environment &env, const FilePath &workingDir) const -> void
{
  QTC_ASSERT(canOpenTerminal(), return);
  d->openTerminal(env, workingDir);
}

auto IDevice::isEmptyCommandAllowed() const -> bool
{
  return d->emptyCommandAllowed;
}

auto IDevice::setAllowEmptyCommand(bool allow) -> void
{
  d->emptyCommandAllowed = allow;
}

auto IDevice::isAnyUnixDevice() const -> bool
{
  return d->osType == OsTypeLinux || d->osType == OsTypeMac || d->osType == OsTypeOtherUnix;
}

auto IDevice::mapToGlobalPath(const FilePath &pathOnDevice) const -> FilePath
{
  return pathOnDevice;
}

auto IDevice::mapToDevicePath(const FilePath &globalPath) const -> QString
{
  return globalPath.path();
}

auto IDevice::handlesFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  return false;
}

auto IDevice::isExecutableFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isReadableFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isWritableFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isReadableDirectory(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isWritableDirectory(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::isDirectory(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::ensureWritableDirectory(const FilePath &filePath) const -> bool
{
  if (isWritableDirectory(filePath))
    return true;
  return createDirectory(filePath);
}

auto IDevice::ensureExistingFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::createDirectory(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::exists(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::removeFile(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::removeRecursively(const FilePath &filePath) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::copyFile(const FilePath &filePath, const FilePath &target) const -> bool
{
  Q_UNUSED(filePath);
  Q_UNUSED(target);
  QTC_CHECK(false);
  return false;
}

auto IDevice::renameFile(const FilePath &filePath, const FilePath &target) const -> bool
{
  Q_UNUSED(filePath);
  Q_UNUSED(target);
  QTC_CHECK(false);
  return false;
}

auto IDevice::searchExecutableInPath(const QString &fileName) const -> FilePath
{
  FilePaths paths;
  for (const auto &path : systemEnvironment().path())
    paths.append(mapToGlobalPath(path));
  return searchExecutable(fileName, paths);
}

auto IDevice::searchExecutable(const QString &fileName, const FilePaths &dirs) const -> FilePath
{
  for (auto dir : dirs) {
    if (!handlesFile(dir)) // Allow device-local dirs to be used.
      dir = mapToGlobalPath(dir);
    QTC_CHECK(handlesFile(dir));
    const auto candidate = dir / fileName;
    if (isExecutableFile(candidate))
      return candidate;
  }

  return {};
}

auto IDevice::symLinkTarget(const FilePath &filePath) const -> FilePath
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return {};
}

auto IDevice::iterateDirectory(const FilePath &filePath, const std::function<bool(const FilePath &)> &callBack, const FileFilter &filter) const -> void
{
  Q_UNUSED(filePath);
  Q_UNUSED(callBack);
  Q_UNUSED(filter);
  QTC_CHECK(false);
}

auto IDevice::fileContents(const FilePath &filePath, qint64 limit, qint64 offset) const -> QByteArray
{
  Q_UNUSED(filePath);
  Q_UNUSED(limit);
  Q_UNUSED(offset);
  QTC_CHECK(false);
  return {};
}

auto IDevice::asyncFileContents(const Continuation<QByteArray> &cont, const FilePath &filePath, qint64 limit, qint64 offset) const -> void
{
  cont(fileContents(filePath, limit, offset));
}

auto IDevice::writeFileContents(const FilePath &filePath, const QByteArray &data) const -> bool
{
  Q_UNUSED(filePath);
  Q_UNUSED(data);
  QTC_CHECK(false);
  return {};
}

auto IDevice::asyncWriteFileContents(const Continuation<bool> &cont, const FilePath &filePath, const QByteArray &data) const -> void
{
  cont(writeFileContents(filePath, data));
}

auto IDevice::lastModified(const FilePath &filePath) const -> QDateTime
{
  Q_UNUSED(filePath);
  return {};
}

auto IDevice::permissions(const FilePath &filePath) const -> QFileDevice::Permissions
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return {};
}

auto IDevice::setPermissions(const FilePath &filePath, QFile::Permissions) const -> bool
{
  Q_UNUSED(filePath);
  QTC_CHECK(false);
  return false;
}

auto IDevice::runProcess(QtcProcess &process) const -> void
{
  Q_UNUSED(process);
  QTC_CHECK(false);
}

auto IDevice::systemEnvironment() const -> Environment
{
  QTC_CHECK(false);
  return Environment::systemEnvironment();
}

auto IDevice::fileSize(const FilePath &filePath) const -> qint64
{
  Q_UNUSED(filePath)
  QTC_CHECK(false);
  return -1;
}

auto IDevice::bytesAvailable(const FilePath &filePath) const -> qint64
{
  Q_UNUSED(filePath)
  QTC_CHECK(false);
  return -1;
}

IDevice::~IDevice() = default;

/*!
    Specifies a free-text name for the device to be displayed in GUI elements.
*/

auto IDevice::displayName() const -> QString
{
  return d->displayName.value();
}

auto IDevice::setDisplayName(const QString &name) -> void
{
  d->displayName.setValue(name);
}

auto IDevice::setDefaultDisplayName(const QString &name) -> void
{
  d->displayName.setDefaultValue(name);
}

auto IDevice::displayType() const -> QString
{
  return d->displayType;
}

auto IDevice::setDisplayType(const QString &type) -> void
{
  d->displayType = type;
}

auto IDevice::setOsType(OsType osType) -> void
{
  d->osType = osType;
}

auto IDevice::deviceInformation() const -> DeviceInfo
{
  const auto key = QCoreApplication::translate("ProjectExplorer::IDevice", "Device");
  return DeviceInfo() << DeviceInfoItem(key, deviceStateToString());
}

/*!
    Identifies the type of the device. Devices with the same type share certain
    abilities. This attribute is immutable.

    \sa ProjectExplorer::IDeviceFactory
 */

auto IDevice::type() const -> Id
{
  return d->type;
}

auto IDevice::setType(Id type) -> void
{
  d->type = type;
}

/*!
    Returns \c true if the device has been added via some sort of auto-detection
    mechanism. Devices that are not auto-detected can only ever be created
    interactively from the \gui Options page. This attribute is immutable.

    \sa DeviceSettingsWidget
*/

auto IDevice::isAutoDetected() const -> bool
{
  return d->origin == AutoDetected;
}

/*!
    Identifies the device. If an id is given when constructing a device then
    this id is used. Otherwise, a UUID is generated and used to identity the
    device.

    \sa ProjectExplorer::DeviceManager::findInactiveAutoDetectedDevice()
*/

auto IDevice::id() const -> Id
{
  return d->id;
}

/*!
    Tests whether a device can be compatible with the given kit. The default
    implementation will match the device type specified in the kit against
    the device's own type.
*/
auto IDevice::isCompatibleWith(const Kit *k) const -> bool
{
  return DeviceTypeKitAspect::deviceTypeId(k) == type();
}

auto IDevice::validate() const -> QList<Task>
{
  return {};
}

auto IDevice::addDeviceAction(const DeviceAction &deviceAction) -> void
{
  d->deviceActions.append(deviceAction);
}

auto IDevice::deviceActions() const -> const QList<DeviceAction>
{
  return d->deviceActions;
}

auto IDevice::portsGatheringMethod() const -> PortsGatheringMethod::Ptr
{
  return PortsGatheringMethod::Ptr();
}

auto IDevice::createProcessListModel(QObject *parent) const -> DeviceProcessList*
{
  Q_UNUSED(parent)
  QTC_ASSERT(false, qDebug("This should not have been called..."); return nullptr);
  return nullptr;
}

auto IDevice::createDeviceTester() const -> DeviceTester*
{
  QTC_ASSERT(false, qDebug("This should not have been called..."));
  return nullptr;
}

auto IDevice::osType() const -> OsType
{
  return d->osType;
}

auto IDevice::createProcess(QObject * /* parent */) const -> DeviceProcess*
{
  QTC_CHECK(false);
  return nullptr;
}

auto IDevice::environmentFetcher() const -> DeviceEnvironmentFetcher::Ptr
{
  return DeviceEnvironmentFetcher::Ptr();
}

auto IDevice::deviceState() const -> DeviceState
{
  return d->deviceState;
}

auto IDevice::setDeviceState(const DeviceState state) -> void
{
  if (d->deviceState == state)
    return;
  d->deviceState = state;
}

auto IDevice::typeFromMap(const QVariantMap &map) -> Id
{
  return Id::fromSetting(map.value(QLatin1String(TypeKey)));
}

auto IDevice::idFromMap(const QVariantMap &map) -> Id
{
  return Id::fromSetting(map.value(QLatin1String(IdKey)));
}

/*!
    Restores a device object from a serialized state as written by toMap().
    If subclasses override this to restore additional state, they must call the
    base class implementation.
*/

auto IDevice::fromMap(const QVariantMap &map) -> void
{
  d->type = typeFromMap(map);
  d->displayName.fromMap(map, DisplayNameKey);
  d->id = Id::fromSetting(map.value(QLatin1String(IdKey)));
  if (!d->id.isValid())
    d->id = newId();
  d->origin = static_cast<Origin>(map.value(QLatin1String(OriginKey), ManuallyAdded).toInt());

  d->sshParameters.setHost(map.value(QLatin1String(HostKey)).toString());
  d->sshParameters.setPort(map.value(QLatin1String(SshPortKey), 22).toInt());
  d->sshParameters.setUserName(map.value(QLatin1String(UserNameKey)).toString());

  // Pre-4.9, the authentication enum used to have more values
  const int storedAuthType = map.value(QLatin1String(AuthKey), DefaultAuthType).toInt();
  const bool outdatedAuthType = storedAuthType > QSsh::SshConnectionParameters::AuthenticationTypeSpecificKey;
  d->sshParameters.authenticationType = outdatedAuthType ? QSsh::SshConnectionParameters::AuthenticationTypeAll : static_cast<AuthType>(storedAuthType);

  d->sshParameters.privateKeyFile = FilePath::fromVariant(map.value(QLatin1String(KeyFileKey), defaultPrivateKeyFilePath()));
  d->sshParameters.timeout = map.value(QLatin1String(TimeoutKey), DefaultTimeout).toInt();
  d->sshParameters.hostKeyCheckingMode = static_cast<QSsh::SshHostKeyCheckingMode>(map.value(QLatin1String(HostKeyCheckingKey), QSsh::SshHostKeyCheckingNone).toInt());

  auto portsSpec = map.value(PortsSpecKey).toString();
  if (portsSpec.isEmpty())
    portsSpec = "10000-10100";
  d->freePorts = PortList::fromString(portsSpec);
  d->machineType = static_cast<MachineType>(map.value(QLatin1String(MachineTypeKey), DefaultMachineType).toInt());
  d->version = map.value(QLatin1String(VersionKey), 0).toInt();

  d->debugServerPath = FilePath::fromVariant(map.value(QLatin1String(DebugServerKey)));
  d->qmlRunCommand = FilePath::fromVariant(map.value(QLatin1String(QmlRuntimeKey)));
  d->extraData = map.value(ExtraDataKey).toMap();
}

/*!
    Serializes a device object, for example to save it to a file.
    If subclasses override this function to save additional state, they must
    call the base class implementation.
*/

auto IDevice::toMap() const -> QVariantMap
{
  QVariantMap map;
  d->displayName.toMap(map, DisplayNameKey);
  map.insert(QLatin1String(TypeKey), d->type.toString());
  map.insert(QLatin1String(IdKey), d->id.toSetting());
  map.insert(QLatin1String(OriginKey), d->origin);

  map.insert(QLatin1String(MachineTypeKey), d->machineType);
  map.insert(QLatin1String(HostKey), d->sshParameters.host());
  map.insert(QLatin1String(SshPortKey), d->sshParameters.port());
  map.insert(QLatin1String(UserNameKey), d->sshParameters.userName());
  map.insert(QLatin1String(AuthKey), d->sshParameters.authenticationType);
  map.insert(QLatin1String(KeyFileKey), d->sshParameters.privateKeyFile.toVariant());
  map.insert(QLatin1String(TimeoutKey), d->sshParameters.timeout);
  map.insert(QLatin1String(HostKeyCheckingKey), d->sshParameters.hostKeyCheckingMode);

  map.insert(QLatin1String(PortsSpecKey), d->freePorts.toString());
  map.insert(QLatin1String(VersionKey), d->version);

  map.insert(QLatin1String(DebugServerKey), d->debugServerPath.toVariant());
  map.insert(QLatin1String(QmlRuntimeKey), d->qmlRunCommand.toVariant());
  map.insert(ExtraDataKey, d->extraData);

  return map;
}

auto IDevice::clone() const -> Ptr
{
  const auto factory = IDeviceFactory::find(d->type);
  QTC_ASSERT(factory, return {});
  auto device = factory->construct();
  QTC_ASSERT(device, return {});
  device->d->deviceState = d->deviceState;
  device->d->deviceActions = d->deviceActions;
  device->d->deviceIcons = d->deviceIcons;
  // Os type is only set in the constructor, always to the same value.
  // But make sure we notice if that changes in the future (which it shouldn't).
  QTC_CHECK(device->d->osType == d->osType);
  device->d->osType = d->osType;
  device->fromMap(toMap());
  return device;
}

auto IDevice::deviceStateToString() const -> QString
{
  const char context[] = "ProjectExplorer::IDevice";
  switch (d->deviceState) {
  case DeviceReadyToUse:
    return QCoreApplication::translate(context, "Ready to use");
  case DeviceConnected:
    return QCoreApplication::translate(context, "Connected");
  case DeviceDisconnected:
    return QCoreApplication::translate(context, "Disconnected");
  case DeviceStateUnknown:
    return QCoreApplication::translate(context, "Unknown");
  default:
    return QCoreApplication::translate(context, "Invalid");
  }
}

auto IDevice::sshParameters() const -> QSsh::SshConnectionParameters
{
  return d->sshParameters;
}

auto IDevice::setSshParameters(const QSsh::SshConnectionParameters &sshParameters) -> void
{
  d->sshParameters = sshParameters;
}

auto IDevice::toolControlChannel(const ControlChannelHint &) const -> QUrl
{
  QUrl url;
  url.setScheme(urlTcpScheme());
  url.setHost(d->sshParameters.host());
  return url;
}

auto IDevice::setFreePorts(const PortList &freePorts) -> void
{
  d->freePorts = freePorts;
}

auto IDevice::freePorts() const -> PortList
{
  return d->freePorts;
}

auto IDevice::machineType() const -> MachineType
{
  return d->machineType;
}

auto IDevice::setMachineType(MachineType machineType) -> void
{
  d->machineType = machineType;
}

auto IDevice::debugServerPath() const -> FilePath
{
  return d->debugServerPath;
}

auto IDevice::setDebugServerPath(const FilePath &path) -> void
{
  d->debugServerPath = path;
}

auto IDevice::debugDumperPath() const -> FilePath
{
  return d->debugDumperPath;
}

auto IDevice::setDebugDumperPath(const FilePath &path) -> void
{
  d->debugDumperPath = path;
}

auto IDevice::qmlRunCommand() const -> FilePath
{
  return d->qmlRunCommand;
}

auto IDevice::setQmlRunCommand(const FilePath &path) -> void
{
  d->qmlRunCommand = path;
}

auto IDevice::setExtraData(Id kind, const QVariant &data) -> void
{
  d->extraData.insert(kind.toString(), data);
}

auto IDevice::extraData(Id kind) const -> QVariant
{
  return d->extraData.value(kind.toString());
}

auto IDevice::version() const -> int
{
  return d->version;
}

auto IDevice::defaultPrivateKeyFilePath() -> QString
{
  return QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QLatin1String("/.ssh/id_rsa");
}

auto IDevice::defaultPublicKeyFilePath() -> QString
{
  return defaultPrivateKeyFilePath() + QLatin1String(".pub");
}

auto DeviceProcessSignalOperation::setDebuggerCommand(const FilePath &cmd) -> void
{
  m_debuggerCommand = cmd;
}

DeviceProcessSignalOperation::DeviceProcessSignalOperation() = default;

DeviceEnvironmentFetcher::DeviceEnvironmentFetcher() = default;

} // namespace ProjectExplorer
