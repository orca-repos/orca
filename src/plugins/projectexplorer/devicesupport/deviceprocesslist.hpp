// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <QAbstractItemModel>
#include <QList>

#include <memory>

namespace ProjectExplorer {

namespace Internal {
class DeviceProcessListPrivate;
}

class PROJECTEXPLORER_EXPORT DeviceProcessItem {
public:
  auto operator<(const DeviceProcessItem &other) const -> bool;

  qint64 pid = 0;
  QString cmdLine;
  QString exe;
};

class PROJECTEXPLORER_EXPORT DeviceProcessList : public QObject {
  Q_OBJECT public:
  DeviceProcessList(const IDevice::ConstPtr &device, QObject *parent = nullptr);
  ~DeviceProcessList() override;

  auto update() -> void;
  auto killProcess(int row) -> void;
  auto setOwnPid(qint64 pid) -> void;
  auto at(int row) const -> DeviceProcessItem;
  auto model() const -> QAbstractItemModel*;
  static auto localProcesses() -> QList<DeviceProcessItem>;

signals:
  auto processListUpdated() -> void;
  auto error(const QString &errorMsg) -> void;
  auto processKilled() -> void;

protected:
  auto reportError(const QString &message) -> void;
  auto reportProcessKilled() -> void;
  auto reportProcessListUpdated(const QList<DeviceProcessItem> &processes) -> void;
  auto device() const -> IDevice::ConstPtr;

private:
  virtual auto doUpdate() -> void = 0;
  virtual auto doKillProcess(const DeviceProcessItem &process) -> void = 0;
  auto setFinished() -> void;

  const std::unique_ptr<Internal::DeviceProcessListPrivate> d;
};

} // namespace ProjectExplorer
