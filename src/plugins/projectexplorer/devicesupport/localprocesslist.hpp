// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "deviceprocesslist.hpp"

namespace ProjectExplorer {
namespace Internal {

class LocalProcessList : public DeviceProcessList {
  Q_OBJECT

public:
  explicit LocalProcessList(const IDevice::ConstPtr &device, QObject *parent = nullptr);

  static auto getLocalProcesses() -> QList<DeviceProcessItem>;

private:
  auto doUpdate() -> void override;
  auto doKillProcess(const DeviceProcessItem &process) -> void override;
  auto handleUpdate() -> void;
  auto reportDelayedKillStatus(const QString &errorMessage) -> void;
};

} // namespace Internal
} // namespace ProjectExplorer
