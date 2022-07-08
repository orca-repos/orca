// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "desktopdevicefactory.hpp"
#include "desktopdevice.hpp"
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectexplorericons.hpp>

#include <core/coreicons.hpp>
#include <utils/icon.hpp>
#include <utils/qtcassert.hpp>

#include <QStyle>

namespace ProjectExplorer {
namespace Internal {

DesktopDeviceFactory::DesktopDeviceFactory() : IDeviceFactory(Constants::DESKTOP_DEVICE_TYPE)
{
  setConstructionFunction([] { return IDevice::Ptr(new DesktopDevice); });
  setDisplayName(DesktopDevice::tr("Desktop"));
  setIcon(Utils::orcaTheme()->flag(Utils::Theme::FlatSideBarIcons) ? Utils::Icon::combinedIcon({Icons::DESKTOP_DEVICE.icon(), Core::Icons::DESKTOP_DEVICE_SMALL.icon()}) : QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
}

} // namespace Internal
} // namespace ProjectExplorer
