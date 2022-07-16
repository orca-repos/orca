// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicetestdialog.hpp"
#include "ui_devicetestdialog.h"

#include <utils/fileutils.hpp>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPushButton>
#include <QTextCharFormat>

namespace ProjectExplorer {
namespace Internal {

class DeviceTestDialog::DeviceTestDialogPrivate {
public:
  DeviceTestDialogPrivate(DeviceTester *tester) : deviceTester(tester), finished(false) { }

  Ui::DeviceTestDialog ui;
  DeviceTester *const deviceTester;
  bool finished;
};

DeviceTestDialog::DeviceTestDialog(const IDevice::Ptr &deviceConfiguration, QWidget *parent) : QDialog(parent), d(std::make_unique<DeviceTestDialogPrivate>(deviceConfiguration->createDeviceTester()))
{
  d->ui.setupUi(this);

  d->deviceTester->setParent(this);
  connect(d->deviceTester, &DeviceTester::progressMessage, this, &DeviceTestDialog::handleProgressMessage);
  connect(d->deviceTester, &DeviceTester::errorMessage, this, &DeviceTestDialog::handleErrorMessage);
  connect(d->deviceTester, &DeviceTester::finished, this, &DeviceTestDialog::handleTestFinished);
  d->deviceTester->testDevice(deviceConfiguration);
}

DeviceTestDialog::~DeviceTestDialog() = default;

auto DeviceTestDialog::reject() -> void
{
  if (!d->finished) {
    d->deviceTester->disconnect(this);
    d->deviceTester->stopTest();
  }
  QDialog::reject();
}

auto DeviceTestDialog::handleProgressMessage(const QString &message) -> void
{
  addText(message, Utils::Theme::OutputPanes_NormalMessageTextColor, false);
}

auto DeviceTestDialog::handleErrorMessage(const QString &message) -> void
{
  addText(message, Utils::Theme::OutputPanes_ErrorMessageTextColor, false);
}

auto DeviceTestDialog::handleTestFinished(DeviceTester::TestResult result) -> void
{
  d->finished = true;
  d->ui.buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Close"));

  if (result == DeviceTester::TestSuccess)
    addText(tr("Device test finished successfully."), Utils::Theme::OutputPanes_NormalMessageTextColor, true);
  else
    addText(tr("Device test failed."), Utils::Theme::OutputPanes_ErrorMessageTextColor, true);
}

auto DeviceTestDialog::addText(const QString &text, Utils::Theme::Color color, bool bold) -> void
{
  const Utils::Theme *theme = Utils::orcaTheme();

  QTextCharFormat format = d->ui.textEdit->currentCharFormat();
  format.setForeground(QBrush(theme->color(color)));
  QFont font = format.font();
  font.setBold(bold);
  format.setFont(font);
  d->ui.textEdit->setCurrentCharFormat(format);
  d->ui.textEdit->appendPlainText(text);
}

} // namespace Internal
} // namespace ProjectExplorer
