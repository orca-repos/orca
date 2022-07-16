// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicecheckbuildstep.hpp"

#include "../kitinformation.hpp"
#include "../target.hpp"
#include "devicemanager.hpp"
#include "idevice.hpp"
#include "idevicefactory.hpp"

#include <QMessageBox>

using namespace ProjectExplorer;

DeviceCheckBuildStep::DeviceCheckBuildStep(BuildStepList *bsl, Utils::Id id) : BuildStep(bsl, id)
{
  setWidgetExpandedByDefault(false);
}

auto DeviceCheckBuildStep::init() -> bool
{
  const auto device = DeviceKitAspect::device(kit());
  if (!device) {
    const auto deviceTypeId = DeviceTypeKitAspect::deviceTypeId(kit());
    const auto factory = IDeviceFactory::find(deviceTypeId);
    if (!factory || !factory->canCreate()) {
      emit addOutput(tr("No device configured."), OutputFormat::ErrorMessage);
      return false;
    }

    QMessageBox msgBox(QMessageBox::Question, tr("Set Up Device"), tr("There is no device set up for this kit. Do you want to add a device?"), QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    if (msgBox.exec() == QMessageBox::No) {
      emit addOutput(tr("No device configured."), OutputFormat::ErrorMessage);
      return false;
    }

    const auto newDevice = factory->create();
    if (newDevice.isNull()) {
      emit addOutput(tr("No device configured."), OutputFormat::ErrorMessage);
      return false;
    }

    const auto dm = DeviceManager::instance();
    dm->addDevice(newDevice);

    DeviceKitAspect::setDevice(kit(), newDevice);
  }

  return true;
}

auto DeviceCheckBuildStep::doRun() -> void
{
  emit finished(true);
}

auto DeviceCheckBuildStep::stepId() -> Utils::Id
{
  return "ProjectExplorer.DeviceCheckBuildStep";
}

auto DeviceCheckBuildStep::displayName() -> QString
{
  return tr("Check for a configured device");
}
