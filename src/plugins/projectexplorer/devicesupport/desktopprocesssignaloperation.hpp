// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <projectexplorer/projectexplorer_export.hpp>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT DesktopProcessSignalOperation : public DeviceProcessSignalOperation {
  Q_OBJECT

public:
  auto killProcess(qint64 pid) -> void override;
  auto killProcess(const QString &filePath) -> void override;
  auto interruptProcess(qint64 pid) -> void override;
  auto interruptProcess(const QString &filePath) -> void override;

private:
  auto killProcessSilently(qint64 pid) -> void;
  auto interruptProcessSilently(qint64 pid) -> void;
  auto appendMsgCannotKill(qint64 pid, const QString &why) -> void;
  auto appendMsgCannotInterrupt(qint64 pid, const QString &why) -> void;

protected:
  DesktopProcessSignalOperation() = default;

  friend class DesktopDevice;
};

} // namespace ProjectExplorer
