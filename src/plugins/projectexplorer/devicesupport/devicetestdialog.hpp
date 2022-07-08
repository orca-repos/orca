// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"

#include <utils/theme/theme.hpp>

#include <QDialog>

#include <memory>

namespace ProjectExplorer {
namespace Internal {

class DeviceTestDialog : public QDialog {
  Q_OBJECT

public:
  DeviceTestDialog(const IDevice::Ptr &deviceConfiguration, QWidget *parent = nullptr);
  ~DeviceTestDialog() override;

  auto reject() -> void override;

private:
  auto handleProgressMessage(const QString &message) -> void;
  auto handleErrorMessage(const QString &message) -> void;
  auto handleTestFinished(DeviceTester::TestResult result) -> void;
  auto addText(const QString &text, Utils::Theme::Color color, bool bold) -> void;

  class DeviceTestDialogPrivate;
  const std::unique_ptr<DeviceTestDialogPrivate> d;
};

} // namespace Internal
} // namespace ProjectExplorer
