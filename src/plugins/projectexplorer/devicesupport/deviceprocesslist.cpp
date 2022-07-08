// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deviceprocesslist.hpp"
#include "localprocesslist.hpp"

#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>
#include <utils/fileutils.hpp>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

enum State {
  Inactive,
  Listing,
  Killing
};

class DeviceProcessTreeItem : public TreeItem {
public:
  DeviceProcessTreeItem(const DeviceProcessItem &p, Qt::ItemFlags f) : process(p), fl(f) {}

  auto data(int column, int role) const -> QVariant final;
  auto flags(int) const -> Qt::ItemFlags final { return fl; }

  DeviceProcessItem process;
  Qt::ItemFlags fl;
};

class DeviceProcessListPrivate {
public:
  DeviceProcessListPrivate(const IDevice::ConstPtr &device) : device(device) { }

  qint64 ownPid = -1;
  const IDevice::ConstPtr device;
  State state = Inactive;
  TreeModel<TypedTreeItem<DeviceProcessTreeItem>, DeviceProcessTreeItem> model;
};

} // namespace Internal

using namespace Internal;

DeviceProcessList::DeviceProcessList(const IDevice::ConstPtr &device, QObject *parent) : QObject(parent), d(std::make_unique<DeviceProcessListPrivate>(device))
{
  d->model.setHeader({tr("Process ID"), tr("Command Line")});
}

DeviceProcessList::~DeviceProcessList() = default;

auto DeviceProcessList::update() -> void
{
  QTC_ASSERT(d->state == Inactive, return);
  QTC_ASSERT(device(), return);

  d->model.clear();
  d->model.rootItem()->appendChild(new DeviceProcessTreeItem({0, tr("Fetching process list. This might take a while."), ""}, Qt::NoItemFlags));
  d->state = Listing;
  doUpdate();
}

auto DeviceProcessList::reportProcessListUpdated(const QList<DeviceProcessItem> &processes) -> void
{
  QTC_ASSERT(d->state == Listing, return);
  setFinished();
  d->model.clear();
  for (const auto &process : processes) {
    Qt::ItemFlags fl;
    if (process.pid != d->ownPid)
      fl = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    d->model.rootItem()->appendChild(new DeviceProcessTreeItem(process, fl));
  }

  emit processListUpdated();
}

auto DeviceProcessList::killProcess(int row) -> void
{
  QTC_ASSERT(row >= 0 && row < d->model.rootItem()->childCount(), return);
  QTC_ASSERT(d->state == Inactive, return);
  QTC_ASSERT(device(), return);

  d->state = Killing;
  doKillProcess(at(row));
}

auto DeviceProcessList::setOwnPid(qint64 pid) -> void
{
  d->ownPid = pid;
}

auto DeviceProcessList::reportProcessKilled() -> void
{
  QTC_ASSERT(d->state == Killing, return);
  setFinished();
  emit processKilled();
}

auto DeviceProcessList::at(int row) const -> DeviceProcessItem
{
  return d->model.rootItem()->childAt(row)->process;
}

auto DeviceProcessList::model() const -> QAbstractItemModel*
{
  return &d->model;
}

auto DeviceProcessTreeItem::data(int column, int role) const -> QVariant
{
  if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
    if (column == 0)
      return process.pid ? process.pid : QVariant();
    else
      return process.cmdLine;
  }
  return QVariant();
}

auto DeviceProcessList::setFinished() -> void
{
  d->state = Inactive;
}

auto DeviceProcessList::device() const -> IDevice::ConstPtr
{
  return d->device;
}

auto DeviceProcessList::reportError(const QString &message) -> void
{
  QTC_ASSERT(d->state != Inactive, return);
  setFinished();
  emit error(message);
}

auto DeviceProcessList::localProcesses() -> QList<DeviceProcessItem>
{
  return LocalProcessList::getLocalProcesses();
}

auto DeviceProcessItem::operator <(const DeviceProcessItem &other) const -> bool
{
  if (pid != other.pid)
    return pid < other.pid;
  if (exe != other.exe)
    return exe < other.exe;
  return cmdLine < other.cmdLine;
}

} // namespace ProjectExplorer
