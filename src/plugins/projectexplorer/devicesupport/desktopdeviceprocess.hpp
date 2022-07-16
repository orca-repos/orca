// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "deviceprocess.hpp"

#include <utils/qtcprocess.hpp>

namespace ProjectExplorer {
namespace Internal {

class DesktopDeviceProcess : public DeviceProcess {
  Q_OBJECT

public:
  DesktopDeviceProcess(const QSharedPointer<const IDevice> &device, QObject *parent = nullptr);

  auto start(const Runnable &runnable) -> void override;
  auto interrupt() -> void override;
};

} // namespace Internal
} // namespace ProjectExplorer
