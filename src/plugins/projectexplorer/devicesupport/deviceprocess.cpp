// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deviceprocess.hpp"

#include "idevice.hpp"

#include <utils/qtcassert.hpp>
#include <utils/fileutils.hpp>

using namespace Utils;

namespace ProjectExplorer {

DeviceProcess::DeviceProcess(const IDevice::ConstPtr &device, const Setup &setup, QObject *parent) : QtcProcess(setup, parent), m_device(device) {}

auto DeviceProcess::device() const -> IDevice::ConstPtr
{
  return m_device;
}

} // namespace ProjectExplorer
