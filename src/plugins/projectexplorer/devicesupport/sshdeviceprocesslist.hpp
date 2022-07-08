// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "deviceprocesslist.hpp"

#include <memory>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT SshDeviceProcessList : public DeviceProcessList {
  Q_OBJECT

public:
  explicit SshDeviceProcessList(const IDevice::ConstPtr &device, QObject *parent = nullptr);
  ~SshDeviceProcessList() override;

private:
  auto handleConnectionError() -> void;
  auto handleListProcessFinished(const QString &error) -> void;
  auto handleKillProcessFinished(const QString &errorString) -> void;

  virtual auto listProcessesCommandLine() const -> QString = 0;
  virtual auto buildProcessList(const QString &listProcessesReply) const -> QList<DeviceProcessItem> = 0;

  auto doUpdate() -> void override;
  auto doKillProcess(const DeviceProcessItem &process) -> void override;
  auto handleProcessError(const QString &errorMessage) -> void;
  auto setFinished() -> void;

  class SshDeviceProcessListPrivate;
  const std::unique_ptr<SshDeviceProcessListPrivate> d;
};

} // namespace ProjectExplorer
