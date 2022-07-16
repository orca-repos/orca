// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicemanagermodel.hpp"

#include "devicemanager.hpp"

#include <utils/qtcassert.hpp>
#include <utils/fileutils.hpp>

#include <QString>

namespace ProjectExplorer {
namespace Internal {

class DeviceManagerModelPrivate {
public:
  const DeviceManager *deviceManager;
  QList<IDevice::ConstPtr> devices;
  QList<Utils::Id> filter;
  Utils::Id typeToKeep;
};
} // namespace Internal

DeviceManagerModel::DeviceManagerModel(const DeviceManager *deviceManager, QObject *parent) : QAbstractListModel(parent), d(std::make_unique<Internal::DeviceManagerModelPrivate>())
{
  d->deviceManager = deviceManager;
  handleDeviceListChanged();
  connect(deviceManager, &DeviceManager::deviceAdded, this, &DeviceManagerModel::handleDeviceAdded);
  connect(deviceManager, &DeviceManager::deviceRemoved, this, &DeviceManagerModel::handleDeviceRemoved);
  connect(deviceManager, &DeviceManager::deviceUpdated, this, &DeviceManagerModel::handleDeviceUpdated);
  connect(deviceManager, &DeviceManager::deviceListReplaced, this, &DeviceManagerModel::handleDeviceListChanged);
}

DeviceManagerModel::~DeviceManagerModel() = default;

auto DeviceManagerModel::setFilter(const QList<Utils::Id> &filter) -> void
{
  d->filter = filter;
  handleDeviceListChanged();
}

auto DeviceManagerModel::setTypeFilter(Utils::Id type) -> void
{
  if (d->typeToKeep == type)
    return;
  d->typeToKeep = type;
  handleDeviceListChanged();
}

auto DeviceManagerModel::updateDevice(Utils::Id id) -> void
{
  handleDeviceUpdated(id);
}

auto DeviceManagerModel::device(int pos) const -> IDevice::ConstPtr
{
  if (pos < 0 || pos >= d->devices.count())
    return IDevice::ConstPtr();
  return d->devices.at(pos);
}

auto DeviceManagerModel::deviceId(int pos) const -> Utils::Id
{
  const auto dev = device(pos);
  return dev ? dev->id() : Utils::Id();
}

auto DeviceManagerModel::indexOf(IDevice::ConstPtr dev) const -> int
{
  if (dev.isNull())
    return -1;
  for (auto i = 0; i < d->devices.count(); ++i) {
    const auto current = d->devices.at(i);
    if (current->id() == dev->id())
      return i;
  }
  return -1;
}

auto DeviceManagerModel::handleDeviceAdded(Utils::Id id) -> void
{
  if (d->filter.contains(id))
    return;
  const auto dev = d->deviceManager->find(id);
  if (!matchesTypeFilter(dev))
    return;

  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  d->devices << dev;
  endInsertRows();
}

auto DeviceManagerModel::handleDeviceRemoved(Utils::Id id) -> void
{
  const auto idx = indexForId(id);
  QTC_ASSERT(idx != -1, return);
  beginRemoveRows(QModelIndex(), idx, idx);
  d->devices.removeAt(idx);
  endRemoveRows();
}

auto DeviceManagerModel::handleDeviceUpdated(Utils::Id id) -> void
{
  const auto idx = indexForId(id);
  if (idx < 0) // This occurs when a device not matching the type filter is updated
    return;
  d->devices[idx] = d->deviceManager->find(id);
  const auto changedIndex = index(idx, 0);
  emit dataChanged(changedIndex, changedIndex);
}

auto DeviceManagerModel::handleDeviceListChanged() -> void
{
  beginResetModel();
  d->devices.clear();

  for (auto i = 0; i < d->deviceManager->deviceCount(); ++i) {
    auto dev = d->deviceManager->deviceAt(i);
    if (d->filter.contains(dev->id()))
      continue;
    if (!matchesTypeFilter(dev))
      continue;
    d->devices << dev;
  }
  endResetModel();
}

auto DeviceManagerModel::rowCount(const QModelIndex &parent) const -> int
{
  Q_UNUSED(parent)
  return d->devices.count();
}

auto DeviceManagerModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid() || index.row() >= rowCount())
    return QVariant();
  if (role != Qt::DisplayRole && role != Qt::UserRole)
    return QVariant();
  const auto dev = device(index.row());
  if (role == Qt::UserRole)
    return dev->id().toSetting();
  QString name;
  if (d->deviceManager->defaultDevice(dev->type()) == dev)
    name = tr("%1 (default for %2)").arg(dev->displayName(), dev->displayType());
  else
    name = dev->displayName();
  return name;
}

auto DeviceManagerModel::matchesTypeFilter(const IDevice::ConstPtr &dev) const -> bool
{
  return !d->typeToKeep.isValid() || dev->type() == d->typeToKeep;
}

auto DeviceManagerModel::indexForId(Utils::Id id) const -> int
{
  for (auto i = 0; i < d->devices.count(); ++i) {
    if (d->devices.at(i)->id() == id)
      return i;
  }

  return -1;
}

} // namespace ProjectExplorer
