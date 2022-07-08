// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <utils/id.hpp>
#include <utils/filepath.hpp>
#include <utils/hostosinfo.hpp>

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QUrl>
#include <QVariantMap>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace QSsh {
class SshConnectionParameters;
}

namespace Utils {
class CommandLine;
class Environment;
class Icon;
class PortList;
class Port;
class QtcProcess;
} // Utils

namespace ProjectExplorer {

class DeviceProcess;
class DeviceProcessList;
class Kit;
class Task;

namespace Internal {
class IDevicePrivate;
}

class IDeviceWidget;
class DeviceTester;

class PROJECTEXPLORER_EXPORT DeviceProcessSignalOperation : public QObject {
  Q_OBJECT

public:
  using Ptr = QSharedPointer<DeviceProcessSignalOperation>;

  virtual auto killProcess(qint64 pid) -> void = 0;
  virtual auto killProcess(const QString &filePath) -> void = 0;
  virtual auto interruptProcess(qint64 pid) -> void = 0;
  virtual auto interruptProcess(const QString &filePath) -> void = 0;

  auto setDebuggerCommand(const Utils::FilePath &cmd) -> void;

signals:
  // If the error message is empty the operation was successful
  auto finished(const QString &errorMessage) -> void;

protected:
  explicit DeviceProcessSignalOperation();

  Utils::FilePath m_debuggerCommand;
  QString m_errorMessage;
};

class PROJECTEXPLORER_EXPORT DeviceEnvironmentFetcher : public QObject {
  Q_OBJECT

public:
  using Ptr = QSharedPointer<DeviceEnvironmentFetcher>;

  virtual auto start() -> void = 0;

signals:
  auto finished(const Utils::Environment &env, bool success) -> void;

protected:
  explicit DeviceEnvironmentFetcher();
};

class PROJECTEXPLORER_EXPORT PortsGatheringMethod {
public:
  using Ptr = QSharedPointer<const PortsGatheringMethod>;

  virtual ~PortsGatheringMethod() = default;
  virtual auto commandLine(QAbstractSocket::NetworkLayerProtocol protocol) const -> Utils::CommandLine = 0;
  virtual auto usedPorts(const QByteArray &commandOutput) const -> QList<Utils::Port> = 0;
};

// See cpp file for documentation.
class PROJECTEXPLORER_EXPORT IDevice : public QEnableSharedFromThis<IDevice> {
  friend class Internal::IDevicePrivate;

public:
  using Ptr = QSharedPointer<IDevice>;
  using ConstPtr = QSharedPointer<const IDevice>;
  template <class ...Args>
  using Continuation = std::function<void(Args ...)>;

  enum Origin {
    ManuallyAdded,
    AutoDetected
  };

  enum MachineType {
    Hardware,
    Emulator
  };

  virtual ~IDevice();

  auto clone() const -> Ptr;
  auto displayName() const -> QString;
  auto setDisplayName(const QString &name) -> void;
  auto setDefaultDisplayName(const QString &name) -> void;

  // Provide some information on the device suitable for formated
  // output, e.g. in tool tips. Get a list of name value pairs.
  class DeviceInfoItem {
  public:
    DeviceInfoItem(const QString &k, const QString &v) : key(k), value(v) { }

    QString key;
    QString value;
  };

  using DeviceInfo = QList<DeviceInfoItem>;
  virtual auto deviceInformation() const -> DeviceInfo;

  auto type() const -> Utils::Id;
  auto setType(Utils::Id type) -> void;
  auto isAutoDetected() const -> bool;
  auto id() const -> Utils::Id;
  virtual auto isCompatibleWith(const Kit *k) const -> bool;
  virtual auto validate() const -> QList<Task>;
  auto displayType() const -> QString;
  auto osType() const -> Utils::OsType;
  virtual auto createWidget() -> IDeviceWidget* = 0;

  struct DeviceAction {
    QString display;
    std::function<void(const Ptr &device, QWidget *parent)> execute;
  };

  auto addDeviceAction(const DeviceAction &deviceAction) -> void;
  auto deviceActions() const -> const QList<DeviceAction>;

  // Devices that can auto detect ports need not return a ports gathering method. Such devices can
  // obtain a free port on demand. eg: Desktop device.
  virtual auto canAutoDetectPorts() const -> bool { return false; }
  virtual auto portsGatheringMethod() const -> PortsGatheringMethod::Ptr;
  virtual auto canCreateProcessModel() const -> bool { return false; }
  virtual auto createProcessListModel(QObject *parent = nullptr) const -> DeviceProcessList*;
  virtual auto hasDeviceTester() const -> bool { return false; }
  virtual auto createDeviceTester() const -> DeviceTester*;
  virtual auto canCreateProcess() const -> bool { return false; }
  virtual auto createProcess(QObject *parent) const -> DeviceProcess*;
  virtual auto signalOperation() const -> DeviceProcessSignalOperation::Ptr = 0;
  virtual auto environmentFetcher() const -> DeviceEnvironmentFetcher::Ptr;

  enum DeviceState {
    DeviceReadyToUse,
    DeviceConnected,
    DeviceDisconnected,
    DeviceStateUnknown
  };

  auto deviceState() const -> DeviceState;
  auto setDeviceState(const DeviceState state) -> void;
  auto deviceStateToString() const -> QString;
  static auto typeFromMap(const QVariantMap &map) -> Utils::Id;
  static auto idFromMap(const QVariantMap &map) -> Utils::Id;
  static auto defaultPrivateKeyFilePath() -> QString;
  static auto defaultPublicKeyFilePath() -> QString;
  auto sshParameters() const -> QSsh::SshConnectionParameters;
  auto setSshParameters(const QSsh::SshConnectionParameters &sshParameters) -> void;

  enum ControlChannelHint {
    QmlControlChannel
  };

  virtual auto toolControlChannel(const ControlChannelHint &) const -> QUrl;

  auto freePorts() const -> Utils::PortList;
  auto setFreePorts(const Utils::PortList &freePorts) -> void;
  auto machineType() const -> MachineType;
  auto setMachineType(MachineType machineType) -> void;
  auto debugServerPath() const -> Utils::FilePath;
  auto setDebugServerPath(const Utils::FilePath &path) -> void;
  auto debugDumperPath() const -> Utils::FilePath;
  auto setDebugDumperPath(const Utils::FilePath &path) -> void;
  auto qmlRunCommand() const -> Utils::FilePath;
  auto setQmlRunCommand(const Utils::FilePath &path) -> void;
  auto setExtraData(Utils::Id kind, const QVariant &data) -> void;
  auto extraData(Utils::Id kind) const -> QVariant;
  auto setupId(Origin origin, Utils::Id id = Utils::Id()) -> void;
  auto canOpenTerminal() const -> bool;
  auto openTerminal(const Utils::Environment &env, const Utils::FilePath &workingDir) const -> void;
  auto isEmptyCommandAllowed() const -> bool;
  auto setAllowEmptyCommand(bool allow) -> void;
  auto isWindowsDevice() const -> bool { return osType() == Utils::OsTypeWindows; }
  auto isLinuxDevice() const -> bool { return osType() == Utils::OsTypeLinux; }
  auto isMacDevice() const -> bool { return osType() == Utils::OsTypeMac; }
  auto isAnyUnixDevice() const -> bool;

  virtual auto mapToGlobalPath(const Utils::FilePath &pathOnDevice) const -> Utils::FilePath;
  virtual auto mapToDevicePath(const Utils::FilePath &globalPath) const -> QString;
  virtual auto handlesFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto isExecutableFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto isReadableFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto isWritableFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto isReadableDirectory(const Utils::FilePath &filePath) const -> bool;
  virtual auto isWritableDirectory(const Utils::FilePath &filePath) const -> bool;
  virtual auto isFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto isDirectory(const Utils::FilePath &filePath) const -> bool;
  virtual auto ensureWritableDirectory(const Utils::FilePath &filePath) const -> bool;
  virtual auto ensureExistingFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto createDirectory(const Utils::FilePath &filePath) const -> bool;
  virtual auto exists(const Utils::FilePath &filePath) const -> bool;
  virtual auto removeFile(const Utils::FilePath &filePath) const -> bool;
  virtual auto removeRecursively(const Utils::FilePath &filePath) const -> bool;
  virtual auto copyFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const -> bool;
  virtual auto renameFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const -> bool;
  virtual auto searchExecutableInPath(const QString &fileName) const -> Utils::FilePath;
  virtual auto searchExecutable(const QString &fileName, const Utils::FilePaths &dirs) const -> Utils::FilePath;
  virtual auto symLinkTarget(const Utils::FilePath &filePath) const -> Utils::FilePath;
  virtual auto iterateDirectory(const Utils::FilePath &filePath, const std::function<bool(const Utils::FilePath &)> &callBack, const Utils::FileFilter &filter) const -> void;
  virtual auto fileContents(const Utils::FilePath &filePath, qint64 limit, qint64 offset) const -> QByteArray;
  virtual auto writeFileContents(const Utils::FilePath &filePath, const QByteArray &data) const -> bool;
  virtual auto lastModified(const Utils::FilePath &filePath) const -> QDateTime;
  virtual auto permissions(const Utils::FilePath &filePath) const -> QFile::Permissions;
  virtual auto setPermissions(const Utils::FilePath &filePath, QFile::Permissions) const -> bool;
  virtual auto runProcess(Utils::QtcProcess &process) const -> void;
  virtual auto systemEnvironment() const -> Utils::Environment;
  virtual auto fileSize(const Utils::FilePath &filePath) const -> qint64;
  virtual auto bytesAvailable(const Utils::FilePath &filePath) const -> qint64;
  virtual auto aboutToBeRemoved() const -> void {}
  virtual auto asyncFileContents(const Continuation<QByteArray> &cont, const Utils::FilePath &filePath, qint64 limit, qint64 offset) const -> void;
  virtual auto asyncWriteFileContents(const Continuation<bool> &cont, const Utils::FilePath &filePath, const QByteArray &data) const -> void;

protected:
  IDevice();

  virtual auto fromMap(const QVariantMap &map) -> void;
  virtual auto toMap() const -> QVariantMap;

  using OpenTerminal = std::function<void(const Utils::Environment &, const Utils::FilePath &)>;

  auto setOpenTerminal(const OpenTerminal &openTerminal) -> void;
  auto setDisplayType(const QString &type) -> void;
  auto setOsType(Utils::OsType osType) -> void;

private:
  IDevice(const IDevice &) = delete;

  auto operator=(const IDevice &) -> IDevice& = delete;
  auto version() const -> int;

  const std::unique_ptr<Internal::IDevicePrivate> d;
  friend class DeviceManager;
};

class PROJECTEXPLORER_EXPORT DeviceTester : public QObject {
  Q_OBJECT

public:
  enum TestResult {
    TestSuccess,
    TestFailure
  };

  virtual auto testDevice(const IDevice::Ptr &deviceConfiguration) -> void = 0;
  virtual auto stopTest() -> void = 0;

signals:
  auto progressMessage(const QString &message) -> void;
  auto errorMessage(const QString &message) -> void;
  auto finished(TestResult result) -> void;

protected:
  explicit DeviceTester(QObject *parent = nullptr);
};

} // namespace ProjectExplorer
