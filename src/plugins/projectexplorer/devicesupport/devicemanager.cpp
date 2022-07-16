// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicemanager.hpp"

#include "idevicefactory.hpp"

#include <core/core-interface.hpp>
#include <core/core-message-manager.hpp>

#include <projectexplorer/projectexplorerconstants.hpp>
#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/persistentsettings.hpp>
#include <utils/portlist.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/stringutils.hpp>

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QVariantList>

#include <limits>
#include <memory>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

constexpr char DeviceManagerKey[] = "DeviceManager";
constexpr char DeviceListKey[] = "DeviceList";
constexpr char DefaultDevicesKey[] = "DefaultDevices";

template <class ...Args>
using Continuation = std::function<void(Args ...)>;

class DeviceManagerPrivate {
public:
  DeviceManagerPrivate() = default;

  auto indexForId(Id id) const -> int
  {
    for (auto i = 0; i < devices.count(); ++i) {
      if (devices.at(i)->id() == id)
        return i;
    }
    return -1;
  }

  auto deviceList() const -> QList<IDevice::Ptr>
  {
    QMutexLocker locker(&mutex);
    return devices;
  }

  static DeviceManager *clonedInstance;
  mutable QMutex mutex;
  QList<IDevice::Ptr> devices;
  QHash<Id, Id> defaultDevices;
  PersistentSettingsWriter *writer = nullptr;
};

DeviceManager *DeviceManagerPrivate::clonedInstance = nullptr;

} // namespace Internal

using namespace Internal;

DeviceManager *DeviceManager::m_instance = nullptr;

auto DeviceManager::instance() -> DeviceManager*
{
  return m_instance;
}

auto DeviceManager::deviceCount() const -> int
{
  return d->devices.count();
}

auto DeviceManager::replaceInstance() -> void
{
  const auto newIds = transform(DeviceManagerPrivate::clonedInstance->d->devices, &IDevice::id);

  for (const auto &dev : qAsConst(m_instance->d->devices)) {
    if (!newIds.contains(dev->id()))
      dev->aboutToBeRemoved();
  }

  {
    QMutexLocker locker(&instance()->d->mutex);
    copy(DeviceManagerPrivate::clonedInstance, instance(), false);
  }

  emit instance()->deviceListReplaced();
  emit instance()->updated();
}

auto DeviceManager::removeClonedInstance() -> void
{
  delete DeviceManagerPrivate::clonedInstance;
  DeviceManagerPrivate::clonedInstance = nullptr;
}

auto DeviceManager::cloneInstance() -> DeviceManager*
{
  QTC_ASSERT(!DeviceManagerPrivate::clonedInstance, return nullptr);

  DeviceManagerPrivate::clonedInstance = new DeviceManager(false);
  copy(instance(), DeviceManagerPrivate::clonedInstance, true);
  return DeviceManagerPrivate::clonedInstance;
}

auto DeviceManager::copy(const DeviceManager *source, DeviceManager *target, bool deep) -> void
{
  if (deep) {
    for (const auto &device : qAsConst(source->d->devices))
      target->d->devices << device->clone();
  } else {
    target->d->devices = source->d->devices;
  }
  target->d->defaultDevices = source->d->defaultDevices;
}

auto DeviceManager::save() -> void
{
  if (d->clonedInstance == this || !d->writer)
    return;
  QVariantMap data;
  data.insert(QLatin1String(DeviceManagerKey), toMap());
  d->writer->save(data, Orca::Plugin::Core::ICore::dialogParent());
}

static auto settingsFilePath(const QString &extension) -> FilePath
{
  return Orca::Plugin::Core::ICore::userResourcePath(extension);
}

static auto systemSettingsFilePath(const QString &deviceFileRelativePath) -> FilePath
{
  return Orca::Plugin::Core::ICore::installerResourcePath(deviceFileRelativePath);
}

auto DeviceManager::load() -> void
{
  QTC_ASSERT(!d->writer, return);

  // Only create writer now: We do not want to save before the settings were read!
  d->writer = new PersistentSettingsWriter(settingsFilePath("devices.xml"), "QtCreatorDevices");

  PersistentSettingsReader reader;
  // read devices file from global settings path
  QHash<Id, Id> defaultDevices;
  QList<IDevice::Ptr> sdkDevices;
  if (reader.load(systemSettingsFilePath("devices.xml")))
    sdkDevices = fromMap(reader.restoreValues().value(DeviceManagerKey).toMap(), &defaultDevices);
  // read devices file from user settings path
  QList<IDevice::Ptr> userDevices;
  if (reader.load(settingsFilePath("devices.xml")))
    userDevices = fromMap(reader.restoreValues().value(DeviceManagerKey).toMap(), &defaultDevices);
  // Insert devices into the model. Prefer the higher device version when there are multiple
  // devices with the same id.
  for (IDevice::ConstPtr device : qAsConst(userDevices)) {
    for (const auto &sdkDevice : qAsConst(sdkDevices)) {
      if (device->id() == sdkDevice->id()) {
        if (device->version() < sdkDevice->version())
          device = sdkDevice;
        sdkDevices.removeOne(sdkDevice);
        break;
      }
    }
    addDevice(device);
  }
  // Append the new SDK devices to the model.
  for (const auto &sdkDevice : qAsConst(sdkDevices))
    addDevice(sdkDevice);

  // Overwrite with the saved default devices.
  for (auto itr = defaultDevices.constBegin(); itr != defaultDevices.constEnd(); ++itr) {
    auto device = find(itr.value());
    if (device)
      d->defaultDevices[device->type()] = device->id();
  }

  emit devicesLoaded();
}

static auto restoreFactory(const QVariantMap &map) -> const IDeviceFactory*
{
  const auto deviceType = IDevice::typeFromMap(map);
  const auto factory = findOrDefault(IDeviceFactory::allDeviceFactories(), [&map, deviceType](IDeviceFactory *factory) {
    return factory->canRestore(map) && factory->deviceType() == deviceType;
  });

  if (!factory)
    qWarning("Warning: No factory found for device '%s' of type '%s'.", qPrintable(IDevice::idFromMap(map).toString()), qPrintable(IDevice::typeFromMap(map).toString()));
  return factory;
}

auto DeviceManager::fromMap(const QVariantMap &map, QHash<Id, Id> *defaultDevices) -> QList<IDevice::Ptr>
{
  QList<IDevice::Ptr> devices;

  if (defaultDevices) {
    const auto defaultDevsMap = map.value(DefaultDevicesKey).toMap();
    for (auto it = defaultDevsMap.constBegin(); it != defaultDevsMap.constEnd(); ++it)
      defaultDevices->insert(Id::fromString(it.key()), Id::fromSetting(it.value()));
  }
  const auto deviceList = map.value(QLatin1String(DeviceListKey)).toList();
  for (const auto &v : deviceList) {
    const auto map = v.toMap();
    const auto factory = restoreFactory(map);
    if (!factory)
      continue;
    const auto device = factory->construct();
    QTC_ASSERT(device, continue);
    device->fromMap(map);
    devices << device;
  }
  return devices;
}

auto DeviceManager::toMap() const -> QVariantMap
{
  QVariantMap map;
  QVariantMap defaultDeviceMap;
  using TypeIdHash = QHash<Id, Id>;
  for (auto it = d->defaultDevices.constBegin(); it != d->defaultDevices.constEnd(); ++it) {
    defaultDeviceMap.insert(it.key().toString(), it.value().toSetting());
  }
  map.insert(QLatin1String(DefaultDevicesKey), defaultDeviceMap);
  QVariantList deviceList;
  for (const auto &device : qAsConst(d->devices))
    deviceList << device->toMap();
  map.insert(QLatin1String(DeviceListKey), deviceList);
  return map;
}

auto DeviceManager::addDevice(const IDevice::ConstPtr &_device) -> void
{
  const auto device = _device->clone();

  QStringList names;
  for (const auto &tmp : qAsConst(d->devices)) {
    if (tmp->id() != device->id())
      names << tmp->displayName();
  }

  // TODO: make it thread safe?
  device->setDisplayName(makeUniquelyNumbered(device->displayName(), names));

  const auto pos = d->indexForId(device->id());

  if (!defaultDevice(device->type()))
    d->defaultDevices.insert(device->type(), device->id());
  if (this == instance() && d->clonedInstance)
    d->clonedInstance->addDevice(device->clone());

  if (pos >= 0) {
    {
      QMutexLocker locker(&d->mutex);
      d->devices[pos] = device;
    }
    emit deviceUpdated(device->id());
  } else {
    {
      QMutexLocker locker(&d->mutex);
      d->devices << device;
    }
    emit deviceAdded(device->id());
  }

  emit updated();
}

auto DeviceManager::removeDevice(Id id) -> void
{
  const auto device = mutableDevice(id);
  QTC_ASSERT(device, return);
  QTC_ASSERT(this != instance() || device->isAutoDetected(), return);

  const auto wasDefault = d->defaultDevices.value(device->type()) == device->id();
  const auto deviceType = device->type();
  {
    QMutexLocker locker(&d->mutex);
    d->devices.removeAt(d->indexForId(id));
  }
  emit deviceRemoved(device->id());

  if (wasDefault) {
    for (auto i = 0; i < d->devices.count(); ++i) {
      if (deviceAt(i)->type() == deviceType) {
        d->defaultDevices.insert(deviceAt(i)->type(), deviceAt(i)->id());
        emit deviceUpdated(deviceAt(i)->id());
        break;
      }
    }
  }
  if (this == instance() && d->clonedInstance)
    d->clonedInstance->removeDevice(id);

  emit updated();
}

auto DeviceManager::setDeviceState(Id deviceId, IDevice::DeviceState deviceState) -> void
{
  // To see the state change in the DeviceSettingsWidget. This has to happen before
  // the pos check below, in case the device is only present in the cloned instance.
  if (this == instance() && d->clonedInstance)
    d->clonedInstance->setDeviceState(deviceId, deviceState);

  const auto pos = d->indexForId(deviceId);
  if (pos < 0)
    return;
  const auto &device = d->devices[pos];
  if (device->deviceState() == deviceState)
    return;

  // TODO: make it thread safe?
  device->setDeviceState(deviceState);
  emit deviceUpdated(deviceId);
  emit updated();
}

auto DeviceManager::isLoaded() const -> bool
{
  return d->writer;
}

// Thread safe
auto DeviceManager::deviceForPath(const FilePath &path) -> IDevice::ConstPtr
{
  const auto devices = instance()->d->deviceList();

  if (path.scheme() == "device") {
    for (const auto &dev : devices) {
      if (path.host() == dev->id().toString())
        return dev;
    }
    return {};
  }

  for (const auto &dev : devices) {
    // TODO: ensure handlesFile is thread safe
    if (dev->handlesFile(path))
      return dev;
  }
  return {};
}

auto DeviceManager::defaultDesktopDevice() -> IDevice::ConstPtr
{
  return m_instance->defaultDevice(Constants::DESKTOP_DEVICE_TYPE);
}

auto DeviceManager::setDefaultDevice(Id id) -> void
{
  QTC_ASSERT(this != instance(), return);

  const auto &device = find(id);
  QTC_ASSERT(device, return);
  const auto &oldDefaultDevice = defaultDevice(device->type());
  if (device == oldDefaultDevice)
    return;
  d->defaultDevices.insert(device->type(), device->id());
  emit deviceUpdated(device->id());
  emit deviceUpdated(oldDefaultDevice->id());

  emit updated();
}

DeviceManager::DeviceManager(bool isInstance) : d(std::make_unique<DeviceManagerPrivate>())
{
  QTC_ASSERT(isInstance == !m_instance, return);

  if (!isInstance)
    return;

  m_instance = this;
  connect(Orca::Plugin::Core::ICore::instance(), &Orca::Plugin::Core::ICore::saveSettingsRequested, this, &DeviceManager::save);

  DeviceFileHooks deviceHooks;

  deviceHooks.isExecutableFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isExecutableFile(filePath);
  };

  deviceHooks.isReadableFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isReadableFile(filePath);
  };

  deviceHooks.isReadableDir = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isReadableDirectory(filePath);
  };

  deviceHooks.isWritableDir = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isWritableDirectory(filePath);
  };

  deviceHooks.isWritableFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isWritableFile(filePath);
  };

  deviceHooks.isFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isFile(filePath);
  };

  deviceHooks.isDir = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->isDirectory(filePath);
  };

  deviceHooks.ensureWritableDir = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->ensureWritableDirectory(filePath);
  };

  deviceHooks.ensureExistingFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->ensureExistingFile(filePath);
  };

  deviceHooks.createDir = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->createDirectory(filePath);
  };

  deviceHooks.exists = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->exists(filePath);
  };

  deviceHooks.removeFile = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->removeFile(filePath);
  };

  deviceHooks.removeRecursively = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->removeRecursively(filePath);
  };

  deviceHooks.copyFile = [](const FilePath &filePath, const FilePath &target) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->copyFile(filePath, target);
  };

  deviceHooks.renameFile = [](const FilePath &filePath, const FilePath &target) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->renameFile(filePath, target);
  };

  deviceHooks.searchInPath = [](const FilePath &filePath, const FilePaths &dirs) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return FilePath{});
    return device->searchExecutable(filePath.path(), dirs);
  };

  deviceHooks.symLinkTarget = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return FilePath{});
    return device->symLinkTarget(filePath);
  };

  deviceHooks.mapToGlobalPath = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return FilePath{});
    return device->mapToGlobalPath(filePath);
  };

  deviceHooks.mapToDevicePath = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return QString{});
    return device->mapToDevicePath(filePath);
  };

  deviceHooks.iterateDirectory = [](const FilePath &filePath, const std::function<bool(const FilePath &)> &callBack, const FileFilter &filter) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return);
    device->iterateDirectory(filePath, callBack, filter);
  };

  deviceHooks.fileContents = [](const FilePath &filePath, qint64 maxSize, qint64 offset) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return QByteArray());
    return device->fileContents(filePath, maxSize, offset);
  };

  deviceHooks.asyncFileContents = [](const Continuation<QByteArray> &cont, const FilePath &filePath, qint64 maxSize, qint64 offset) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return);
    device->asyncFileContents(cont, filePath, maxSize, offset);
  };

  deviceHooks.writeFileContents = [](const FilePath &filePath, const QByteArray &data) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->writeFileContents(filePath, data);
  };

  deviceHooks.lastModified = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return QDateTime());
    return device->lastModified(filePath);
  };

  deviceHooks.permissions = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return QFile::Permissions());
    return device->permissions(filePath);
  };

  deviceHooks.setPermissions = [](const FilePath &filePath, QFile::Permissions permissions) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return false);
    return device->setPermissions(filePath, permissions);
  };

  deviceHooks.osType = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return OsTypeOther);
    return device->osType();
  };

  deviceHooks.environment = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return Environment{});
    return device->systemEnvironment();
  };

  deviceHooks.fileSize = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return qint64(-1));
    return device->fileSize(filePath);
  };

  deviceHooks.bytesAvailable = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return qint64(-1));
    return device->bytesAvailable(filePath);
  };

  FileUtils::setDeviceFileHooks(deviceHooks);

  DeviceProcessHooks processHooks;

  processHooks.startProcessHook = [](QtcProcess &process) {
    const auto filePath = process.commandLine().executable();
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return);
    device->runProcess(process);
  };

  processHooks.systemEnvironmentForBinary = [](const FilePath &filePath) {
    const auto device = deviceForPath(filePath);
    QTC_ASSERT(device, return Environment());
    return device->systemEnvironment();
  };

  QtcProcess::setRemoteProcessHooks(processHooks);
}

DeviceManager::~DeviceManager()
{
  if (d->clonedInstance != this)
    delete d->writer;
  if (m_instance == this)
    m_instance = nullptr;
}

auto DeviceManager::deviceAt(int idx) const -> IDevice::ConstPtr
{
  QTC_ASSERT(idx >= 0 && idx < deviceCount(), return IDevice::ConstPtr());
  return d->devices.at(idx);
}

auto DeviceManager::mutableDevice(Id id) const -> IDevice::Ptr
{
  const auto index = d->indexForId(id);
  return index == -1 ? IDevice::Ptr() : d->devices.at(index);
}

auto DeviceManager::hasDevice(const QString &name) const -> bool
{
  return anyOf(d->devices, [&name](const IDevice::Ptr &device) {
    return device->displayName() == name;
  });
}

auto DeviceManager::find(Id id) const -> IDevice::ConstPtr
{
  const auto index = d->indexForId(id);
  return index == -1 ? IDevice::ConstPtr() : deviceAt(index);
}

auto DeviceManager::defaultDevice(Id deviceType) const -> IDevice::ConstPtr
{
  const auto id = d->defaultDevices.value(deviceType);
  return id.isValid() ? find(id) : IDevice::ConstPtr();
}

} // namespace ProjectExplorer

#ifdef WITH_TESTS
#include <projectexplorer/projectexplorer.hpp>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

namespace ProjectExplorer {

class TestDevice : public IDevice
{
public:
    TestDevice()
    {
        setupId(AutoDetected, Utils::Id::fromString(QUuid::createUuid().toString()));
        setType(testTypeId());
        setMachineType(Hardware);
        setOsType(HostOsInfo::hostOs());
        setDisplayType("blubb");
    }

    static Utils::Id testTypeId() { return "TestType"; }
private:
    IDeviceWidget *createWidget() override { return nullptr; }
    DeviceProcessSignalOperation::Ptr signalOperation() const override
    {
        return DeviceProcessSignalOperation::Ptr();
    }
};

class TestDeviceFactory final : public IDeviceFactory
{
public:
    TestDeviceFactory() : IDeviceFactory(TestDevice::testTypeId())
    {
        setConstructionFunction([] { return IDevice::Ptr(new TestDevice); });
    }
};

void ProjectExplorerPlugin::testDeviceManager()
{
    TestDeviceFactory factory;

    TestDevice::Ptr dev = IDevice::Ptr(new TestDevice);
    dev->setDisplayName(QLatin1String("blubbdiblubbfurz!"));
    QVERIFY(dev->isAutoDetected());
    QCOMPARE(dev->deviceState(), IDevice::DeviceStateUnknown);
    QCOMPARE(dev->type(), TestDevice::testTypeId());

    TestDevice::Ptr dev2 = dev->clone();
    QCOMPARE(dev->id(), dev2->id());

    DeviceManager * const mgr = DeviceManager::instance();
    QVERIFY(!mgr->find(dev->id()));
    const int oldDeviceCount = mgr->deviceCount();

    QSignalSpy deviceAddedSpy(mgr, &DeviceManager::deviceAdded);
    QSignalSpy deviceRemovedSpy(mgr, &DeviceManager::deviceRemoved);
    QSignalSpy deviceUpdatedSpy(mgr, &DeviceManager::deviceUpdated);
    QSignalSpy deviceListReplacedSpy(mgr, &DeviceManager::deviceListReplaced);
    QSignalSpy updatedSpy(mgr, &DeviceManager::updated);

    mgr->addDevice(dev);
    QCOMPARE(mgr->deviceCount(), oldDeviceCount + 1);
    QVERIFY(mgr->find(dev->id()));
    QVERIFY(mgr->hasDevice(dev->displayName()));
    QCOMPARE(deviceAddedSpy.count(), 1);
    QCOMPARE(deviceRemovedSpy.count(), 0);
    QCOMPARE(deviceUpdatedSpy.count(), 0);
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 1);
    deviceAddedSpy.clear();
    updatedSpy.clear();

    mgr->setDeviceState(dev->id(), IDevice::DeviceStateUnknown);
    QCOMPARE(deviceAddedSpy.count(), 0);
    QCOMPARE(deviceRemovedSpy.count(), 0);
    QCOMPARE(deviceUpdatedSpy.count(), 0);
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 0);

    mgr->setDeviceState(dev->id(), IDevice::DeviceReadyToUse);
    QCOMPARE(mgr->find(dev->id())->deviceState(), IDevice::DeviceReadyToUse);
    QCOMPARE(deviceAddedSpy.count(), 0);
    QCOMPARE(deviceRemovedSpy.count(), 0);
    QCOMPARE(deviceUpdatedSpy.count(), 1);
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 1);
    deviceUpdatedSpy.clear();
    updatedSpy.clear();

    mgr->addDevice(dev2);
    QCOMPARE(mgr->deviceCount(), oldDeviceCount + 1);
    QVERIFY(mgr->find(dev->id()));
    QCOMPARE(deviceAddedSpy.count(), 0);
    QCOMPARE(deviceRemovedSpy.count(), 0);
    QCOMPARE(deviceUpdatedSpy.count(), 1);
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 1);
    deviceUpdatedSpy.clear();
    updatedSpy.clear();

    TestDevice::Ptr dev3 = IDevice::Ptr(new TestDevice);
    QVERIFY(dev->id() != dev3->id());

    dev3->setDisplayName(dev->displayName());
    mgr->addDevice(dev3);
    QCOMPARE(mgr->deviceAt(mgr->deviceCount() - 1)->displayName(),
             QString(dev3->displayName() + QLatin1Char('2')));
    QCOMPARE(deviceAddedSpy.count(), 1);
    QCOMPARE(deviceRemovedSpy.count(), 0);
    QCOMPARE(deviceUpdatedSpy.count(), 0);
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 1);
    deviceAddedSpy.clear();
    updatedSpy.clear();

    mgr->removeDevice(dev->id());
    mgr->removeDevice(dev3->id());
    QCOMPARE(mgr->deviceCount(), oldDeviceCount);
    QVERIFY(!mgr->find(dev->id()));
    QVERIFY(!mgr->find(dev3->id()));
    QCOMPARE(deviceAddedSpy.count(), 0);
    QCOMPARE(deviceRemovedSpy.count(), 2);
//    QCOMPARE(deviceUpdatedSpy.count(), 0); Uncomment once the "default" stuff is gone.
    QCOMPARE(deviceListReplacedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 2);
}

} // namespace ProjectExplorer

#endif // WITH_TESTS
