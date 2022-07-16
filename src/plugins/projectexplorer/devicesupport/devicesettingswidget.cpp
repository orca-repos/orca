// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicesettingswidget.hpp"
#include "ui_devicesettingswidget.h"

#include "devicefactoryselectiondialog.hpp"
#include "devicemanager.hpp"
#include "devicemanagermodel.hpp"
#include "deviceprocessesdialog.hpp"
#include "devicetestdialog.hpp"
#include "idevice.hpp"
#include "idevicefactory.hpp"
#include "idevicewidget.hpp"
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectexplorericons.hpp>

#include <core/core-interface.hpp>

#include <utils/qtcassert.hpp>
#include <utils/algorithm.hpp>

#include <QPixmap>
#include <QPushButton>
#include <QTextStream>

#include <algorithm>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

constexpr char LastDeviceIndexKey[] = "LastDisplayedMaemoDeviceConfig";

class NameValidator : public QValidator {
public:
  NameValidator(const DeviceManager *deviceManager, QWidget *parent = nullptr) : QValidator(parent), m_deviceManager(deviceManager) { }

  auto setDisplayName(const QString &name) -> void { m_oldName = name; }

  auto validate(QString &input, int & /* pos */) const -> State override
  {
    if (input.trimmed().isEmpty() || (input != m_oldName && m_deviceManager->hasDevice(input)))
      return Intermediate;
    return Acceptable;
  }

  auto fixup(QString &input) const -> void override
  {
    auto dummy = 0;
    if (validate(input, dummy) != Acceptable)
      input = m_oldName;
  }

private:
  QString m_oldName;
  const DeviceManager *const m_deviceManager;
};

DeviceSettingsWidget::DeviceSettingsWidget() : m_ui(new Ui::DeviceSettingsWidget), m_deviceManager(DeviceManager::cloneInstance()), m_deviceManagerModel(new DeviceManagerModel(m_deviceManager, this)), m_nameValidator(new NameValidator(m_deviceManager, this)), m_configWidget(nullptr)
{
  initGui();
  connect(m_deviceManager, &DeviceManager::deviceUpdated, this, &DeviceSettingsWidget::handleDeviceUpdated);
}

DeviceSettingsWidget::~DeviceSettingsWidget()
{
  DeviceManager::removeClonedInstance();
  delete m_configWidget;
  delete m_ui;
}

auto DeviceSettingsWidget::initGui() -> void
{
  m_ui->setupUi(this);
  m_ui->configurationComboBox->setModel(m_deviceManagerModel);
  m_ui->nameLineEdit->setValidator(m_nameValidator);

  auto hasDeviceFactories = anyOf(IDeviceFactory::allDeviceFactories(), &IDeviceFactory::canCreate);

  m_ui->addConfigButton->setEnabled(hasDeviceFactories);

  auto lastIndex = ICore::settings()->value(QLatin1String(LastDeviceIndexKey), 0).toInt();
  if (lastIndex == -1)
    lastIndex = 0;
  if (lastIndex < m_ui->configurationComboBox->count())
    m_ui->configurationComboBox->setCurrentIndex(lastIndex);
  connect(m_ui->configurationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DeviceSettingsWidget::currentDeviceChanged);
  currentDeviceChanged(currentIndex());
  connect(m_ui->defaultDeviceButton, &QAbstractButton::clicked, this, &DeviceSettingsWidget::setDefaultDevice);
  connect(m_ui->removeConfigButton, &QAbstractButton::clicked, this, &DeviceSettingsWidget::removeDevice);
  connect(m_ui->nameLineEdit, &QLineEdit::editingFinished, this, &DeviceSettingsWidget::deviceNameEditingFinished);
  connect(m_ui->addConfigButton, &QAbstractButton::clicked, this, &DeviceSettingsWidget::addDevice);
}

auto DeviceSettingsWidget::addDevice() -> void
{
  DeviceFactorySelectionDialog d;
  if (d.exec() != QDialog::Accepted)
    return;

  const auto toCreate = d.selectedId();
  if (!toCreate.isValid())
    return;
  const auto factory = IDeviceFactory::find(toCreate);
  if (!factory)
    return;
  const auto device = factory->create();
  if (device.isNull())
    return;

  m_deviceManager->addDevice(device);
  m_ui->removeConfigButton->setEnabled(true);
  m_ui->configurationComboBox->setCurrentIndex(m_deviceManagerModel->indexOf(device));
  if (device->hasDeviceTester())
    testDevice();
  saveSettings();
}

auto DeviceSettingsWidget::removeDevice() -> void
{
  m_deviceManager->removeDevice(currentDevice()->id());
  if (m_deviceManager->deviceCount() == 0)
    currentDeviceChanged(-1);
}

auto DeviceSettingsWidget::displayCurrent() -> void
{
  const auto &current = currentDevice();
  m_ui->defaultDeviceButton->setEnabled(m_deviceManager->defaultDevice(current->type()) != current);
  m_ui->osTypeValueLabel->setText(current->displayType());
  m_ui->autoDetectionValueLabel->setText(current->isAutoDetected() ? tr("Yes (id is \"%1\")").arg(current->id().toString()) : tr("No"));
  m_nameValidator->setDisplayName(current->displayName());
  m_ui->deviceStateValueIconLabel->show();
  switch (current->deviceState()) {
  case IDevice::DeviceReadyToUse:
    m_ui->deviceStateValueIconLabel->setPixmap(Icons::DEVICE_READY_INDICATOR.pixmap());
    break;
  case IDevice::DeviceConnected:
    m_ui->deviceStateValueIconLabel->setPixmap(Icons::DEVICE_CONNECTED_INDICATOR.pixmap());
    break;
  case IDevice::DeviceDisconnected:
    m_ui->deviceStateValueIconLabel->setPixmap(Icons::DEVICE_DISCONNECTED_INDICATOR.pixmap());
    break;
  case IDevice::DeviceStateUnknown:
    m_ui->deviceStateValueIconLabel->hide();
    break;
  }
  m_ui->deviceStateValueTextLabel->setText(current->deviceStateToString());

  m_ui->removeConfigButton->setEnabled(!current->isAutoDetected() || current->deviceState() == IDevice::DeviceDisconnected);
  fillInValues();
}

auto DeviceSettingsWidget::setDeviceInfoWidgetsEnabled(bool enable) -> void
{
  m_ui->configurationLabel->setEnabled(enable);
  m_ui->configurationComboBox->setEnabled(enable);
  m_ui->generalGroupBox->setEnabled(enable);
  m_ui->osSpecificGroupBox->setEnabled(enable);
}

auto DeviceSettingsWidget::fillInValues() -> void
{
  const auto &current = currentDevice();
  m_ui->nameLineEdit->setText(current->displayName());
}

auto DeviceSettingsWidget::updateDeviceFromUi() -> void
{
  deviceNameEditingFinished();
  if (m_configWidget)
    m_configWidget->updateDeviceFromUi();
}

auto DeviceSettingsWidget::saveSettings() -> void
{
  ICore::settings()->setValueWithDefault(LastDeviceIndexKey, currentIndex(), 0);
  DeviceManager::replaceInstance();
}

auto DeviceSettingsWidget::currentIndex() const -> int
{
  return m_ui->configurationComboBox->currentIndex();
}

auto DeviceSettingsWidget::currentDevice() const -> IDevice::ConstPtr
{
  Q_ASSERT(currentIndex() != -1);
  return m_deviceManagerModel->device(currentIndex());
}

auto DeviceSettingsWidget::deviceNameEditingFinished() -> void
{
  if (m_ui->configurationComboBox->count() == 0)
    return;

  const QString &newName = m_ui->nameLineEdit->text();
  m_deviceManager->mutableDevice(currentDevice()->id())->setDisplayName(newName);
  m_nameValidator->setDisplayName(newName);
  m_deviceManagerModel->updateDevice(currentDevice()->id());
}

auto DeviceSettingsWidget::setDefaultDevice() -> void
{
  m_deviceManager->setDefaultDevice(currentDevice()->id());
  m_ui->defaultDeviceButton->setEnabled(false);
}

auto DeviceSettingsWidget::testDevice() -> void
{
  const auto &device = currentDevice();
  QTC_ASSERT(device && device->hasDeviceTester(), return);
  const auto dlg = new DeviceTestDialog(m_deviceManager->mutableDevice(device->id()), this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setModal(true);
  dlg->show();
}

auto DeviceSettingsWidget::handleDeviceUpdated(Id id) -> void
{
  const auto index = m_deviceManagerModel->indexForId(id);
  if (index == currentIndex())
    currentDeviceChanged(index);
}

auto DeviceSettingsWidget::currentDeviceChanged(int index) -> void
{
  qDeleteAll(m_additionalActionButtons);
  delete m_configWidget;
  m_configWidget = nullptr;
  m_additionalActionButtons.clear();
  const auto device = m_deviceManagerModel->device(index);
  if (device.isNull()) {
    setDeviceInfoWidgetsEnabled(false);
    m_ui->removeConfigButton->setEnabled(false);
    clearDetails();
    m_ui->defaultDeviceButton->setEnabled(false);
    return;
  }
  setDeviceInfoWidgetsEnabled(true);
  m_ui->removeConfigButton->setEnabled(true);

  if (device->hasDeviceTester()) {
    const auto button = new QPushButton(tr("Test"));
    m_additionalActionButtons << button;
    connect(button, &QAbstractButton::clicked, this, &DeviceSettingsWidget::testDevice);
    m_ui->buttonsLayout->insertWidget(m_ui->buttonsLayout->count() - 1, button);
  }

  if (device->canCreateProcessModel()) {
    const auto button = new QPushButton(tr("Show Running Processes..."));
    m_additionalActionButtons << button;
    connect(button, &QAbstractButton::clicked, this, &DeviceSettingsWidget::handleProcessListRequested);
    m_ui->buttonsLayout->insertWidget(m_ui->buttonsLayout->count() - 1, button);
  }

  for (const auto &deviceAction : device->deviceActions()) {
    const auto button = new QPushButton(deviceAction.display);
    m_additionalActionButtons << button;
    connect(button, &QAbstractButton::clicked, this, [this, deviceAction] {
      const auto device = m_deviceManager->mutableDevice(currentDevice()->id());
      QTC_ASSERT(device, return);
      updateDeviceFromUi();
      deviceAction.execute(device, this);
      // Widget must be set up from scratch, because the action could have
      // changed random attributes.
      currentDeviceChanged(currentIndex());
    });

    m_ui->buttonsLayout->insertWidget(m_ui->buttonsLayout->count() - 1, button);
  }

  if (!m_ui->osSpecificGroupBox->layout())
    new QVBoxLayout(m_ui->osSpecificGroupBox);
  m_configWidget = m_deviceManager->mutableDevice(device->id())->createWidget();
  if (m_configWidget)
    m_ui->osSpecificGroupBox->layout()->addWidget(m_configWidget);
  displayCurrent();
}

auto DeviceSettingsWidget::clearDetails() -> void
{
  m_ui->nameLineEdit->clear();
  m_ui->osTypeValueLabel->clear();
  m_ui->autoDetectionValueLabel->clear();
}

auto DeviceSettingsWidget::handleProcessListRequested() -> void
{
  QTC_ASSERT(currentDevice()->canCreateProcessModel(), return);
  updateDeviceFromUi();
  DeviceProcessesDialog dlg;
  dlg.addCloseButton();
  dlg.setDevice(currentDevice());
  dlg.exec();
}

} // namespace Internal
} // namespace ProjectExplorer
