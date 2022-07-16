// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <projectexplorer/devicesupport/idevice.hpp>

#include <QDialog>

#include <memory>

namespace ProjectExplorer {

class DeviceProcessItem;
class KitChooser;

namespace Internal {
class DeviceProcessesDialogPrivate;
}

class PROJECTEXPLORER_EXPORT DeviceProcessesDialog : public QDialog {
  Q_OBJECT

public:
  explicit DeviceProcessesDialog(QWidget *parent = nullptr);
  ~DeviceProcessesDialog() override;

  auto addAcceptButton(const QString &label) -> void;
  auto addCloseButton() -> void;
  auto setDevice(const IDevice::ConstPtr &device) -> void;
  auto showAllDevices() -> void;
  auto currentProcess() const -> DeviceProcessItem;
  auto kitChooser() const -> KitChooser*;
  auto logMessage(const QString &line) -> void;

  DeviceProcessesDialog(KitChooser *chooser, QWidget *parent);

private:
  auto setKitVisible(bool) -> void;
  const std::unique_ptr<Internal::DeviceProcessesDialogPrivate> d;
};

} // namespace ProjectExplorer
