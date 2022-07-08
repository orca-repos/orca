// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicesettingspage.hpp"

#include "devicesettingswidget.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>

#include <QCoreApplication>

namespace ProjectExplorer {
namespace Internal {

DeviceSettingsPage::DeviceSettingsPage()
{
  setId(Constants::DEVICE_SETTINGS_PAGE_ID);
  setDisplayName(DeviceSettingsWidget::tr("Devices"));
  setCategory(Constants::DEVICE_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", "Devices"));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_devices.png");
  setWidgetCreator([] { return new DeviceSettingsWidget; });
}

} // namespace Internal
} // namespace ProjectExplorer
