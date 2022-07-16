// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "desktopdeviceprocess.hpp"

#include "idevice.hpp"
#include "../runcontrol.hpp"

#include <utils/environment.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

DesktopDeviceProcess::DesktopDeviceProcess(const QSharedPointer<const IDevice> &device, QObject *parent) : DeviceProcess(device, ProcessMode::Writer, parent) {}

auto DesktopDeviceProcess::start(const Runnable &runnable) -> void
{
  QTC_ASSERT(state() == QProcess::NotRunning, return);
  if (runnable.environment.size())
    setEnvironment(runnable.environment);
  setWorkingDirectory(runnable.workingDirectory);
  setCommand(runnable.command);
  QtcProcess::start();
}

auto DesktopDeviceProcess::interrupt() -> void
{
  device()->signalOperation()->interruptProcess(processId());
}

} // namespace Internal
} // namespace ProjectExplorer
