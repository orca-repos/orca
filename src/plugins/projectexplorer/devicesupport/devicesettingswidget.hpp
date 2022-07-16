// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <QList>
#include <QString>
#include <QWidget>

#include <core/core-options-page-interface.hpp>

QT_BEGIN_NAMESPACE
class QPushButton;
QT_END_NAMESPACE

namespace ProjectExplorer {

class IDevice;
class DeviceManager;
class DeviceManagerModel;
class IDeviceWidget;

namespace Internal {

namespace Ui {
class DeviceSettingsWidget;
}

class NameValidator;

class DeviceSettingsWidget final : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_OBJECT

public:
  DeviceSettingsWidget();
  ~DeviceSettingsWidget() override;

private:
  auto apply() -> void override { saveSettings(); }
  auto saveSettings() -> void;
  auto handleDeviceUpdated(Utils::Id id) -> void;
  auto currentDeviceChanged(int index) -> void;
  auto addDevice() -> void;
  auto removeDevice() -> void;
  auto deviceNameEditingFinished() -> void;
  auto setDefaultDevice() -> void;
  auto testDevice() -> void;
  auto handleProcessListRequested() -> void;
  auto initGui() -> void;
  auto displayCurrent() -> void;
  auto setDeviceInfoWidgetsEnabled(bool enable) -> void;
  auto currentDevice() const -> IDevice::ConstPtr;
  auto currentIndex() const -> int;
  auto clearDetails() -> void;
  auto parseTestOutput() -> QString;
  auto fillInValues() -> void;
  auto updateDeviceFromUi() -> void;

  Ui::DeviceSettingsWidget *m_ui;
  DeviceManager *const m_deviceManager;
  DeviceManagerModel *const m_deviceManagerModel;
  NameValidator *const m_nameValidator;
  QList<QPushButton*> m_additionalActionButtons;
  IDeviceWidget *m_configWidget;
};

} // namespace Internal
} // namespace ProjectExplorer
