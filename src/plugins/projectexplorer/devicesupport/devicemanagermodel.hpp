// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"
#include "idevice.hpp"

#include <QAbstractListModel>

#include <memory>

namespace ProjectExplorer {

namespace Internal {
class DeviceManagerModelPrivate;
}

class IDevice;
class DeviceManager;

class PROJECTEXPLORER_EXPORT DeviceManagerModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit DeviceManagerModel(const DeviceManager *deviceManager, QObject *parent = nullptr);
  ~DeviceManagerModel() override;

  auto setFilter(const QList<Utils::Id> &filter) -> void;
  auto setTypeFilter(Utils::Id type) -> void;
  auto device(int pos) const -> IDevice::ConstPtr;
  auto deviceId(int pos) const -> Utils::Id;
  auto indexOf(IDevice::ConstPtr dev) const -> int;
  auto indexForId(Utils::Id id) const -> int;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto updateDevice(Utils::Id id) -> void;

private:
  auto handleDeviceAdded(Utils::Id id) -> void;
  auto handleDeviceRemoved(Utils::Id id) -> void;
  auto handleDeviceUpdated(Utils::Id id) -> void;
  auto handleDeviceListChanged() -> void;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto matchesTypeFilter(const IDevice::ConstPtr &dev) const -> bool;

  const std::unique_ptr<Internal::DeviceManagerModelPrivate> d;
};

} // namespace ProjectExplorer
