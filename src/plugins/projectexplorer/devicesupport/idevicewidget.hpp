// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"
#include <projectexplorer/projectexplorer_export.hpp>

#include <QWidget>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT IDeviceWidget : public QWidget {
  Q_OBJECT

public:
  virtual auto updateDeviceFromUi() -> void = 0;

protected:
  explicit IDeviceWidget(const IDevice::Ptr &device) : m_device(device) { }
  auto device() const -> IDevice::Ptr { return m_device; }

private:
  IDevice::Ptr m_device;
};

} // namespace ProjectExplorer
