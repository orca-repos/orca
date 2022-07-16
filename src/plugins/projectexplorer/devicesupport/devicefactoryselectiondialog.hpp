// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.hpp>

#include <QDialog>

namespace ProjectExplorer {
class IDeviceFactory;

namespace Internal {
namespace Ui {
class DeviceFactorySelectionDialog;
}

class DeviceFactorySelectionDialog : public QDialog {
  Q_OBJECT

public:
  explicit DeviceFactorySelectionDialog(QWidget *parent = nullptr);
  ~DeviceFactorySelectionDialog() override;

  auto selectedId() const -> Utils::Id;

private:
  auto handleItemSelectionChanged() -> void;
  auto handleItemDoubleClicked() -> void;
  Ui::DeviceFactorySelectionDialog *ui;
};

} // namespace Internal
} // namespace ProjectExplorer
