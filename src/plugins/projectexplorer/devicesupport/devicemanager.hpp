// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <projectexplorer/projectexplorer_export.hpp>

#include <QObject>

#include <memory>

namespace Utils {
class FilePath;
}

namespace ProjectExplorer {
class ProjectExplorerPlugin;

namespace Internal {
class DeviceManagerPrivate;
class DeviceSettingsWidget;
} // namespace Internal

class PROJECTEXPLORER_EXPORT DeviceManager : public QObject {
  Q_OBJECT
  friend class Internal::DeviceSettingsWidget;
  friend class IDevice;

public:
  ~DeviceManager() override;

  static auto instance() -> DeviceManager*;
  auto deviceCount() const -> int;
  auto deviceAt(int index) const -> IDevice::ConstPtr;
  auto find(Utils::Id id) const -> IDevice::ConstPtr;
  auto defaultDevice(Utils::Id deviceType) const -> IDevice::ConstPtr;
  auto hasDevice(const QString &name) const -> bool;
  auto addDevice(const IDevice::ConstPtr &device) -> void;
  auto removeDevice(Utils::Id id) -> void;
  auto setDeviceState(Utils::Id deviceId, IDevice::DeviceState deviceState) -> void;
  auto isLoaded() const -> bool;
  static auto deviceForPath(const Utils::FilePath &path) -> IDevice::ConstPtr;
  static auto defaultDesktopDevice() -> IDevice::ConstPtr;

signals:
  auto deviceAdded(Utils::Id id) -> void;
  auto deviceRemoved(Utils::Id id) -> void;
  auto deviceUpdated(Utils::Id id) -> void;
  auto deviceListReplaced() -> void; // For bulk changes via the settings dialog.
  auto updated() -> void;            // Emitted for all of the above.
  auto devicesLoaded() -> void; // Emitted once load() is done

private:

  DeviceManager(bool isInstance = true);

  auto save() -> void;
  auto load() -> void;
  auto fromMap(const QVariantMap &map, QHash<Utils::Id, Utils::Id> *defaultDevices) -> QList<IDevice::Ptr>;
  auto toMap() const -> QVariantMap;

  // For SettingsWidget.
  auto mutableDevice(Utils::Id id) const -> IDevice::Ptr;
  auto setDefaultDevice(Utils::Id id) -> void;
  static auto cloneInstance() -> DeviceManager*;
  static auto replaceInstance() -> void;
  static auto removeClonedInstance() -> void;
  static auto copy(const DeviceManager *source, DeviceManager *target, bool deep) -> void;

  const std::unique_ptr<Internal::DeviceManagerPrivate> d;
  static DeviceManager *m_instance;

  friend class Internal::DeviceManagerPrivate;
  friend class ProjectExplorerPlugin;
  friend class ProjectExplorerPluginPrivate;
};

} // namespace ProjectExplorer
