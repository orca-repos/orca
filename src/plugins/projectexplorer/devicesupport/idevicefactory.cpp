// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "idevicefactory.hpp"

#include <utils/algorithm.hpp>
#include <utils/icon.hpp>
#include <utils/qtcassert.hpp>

using namespace Utils;

namespace ProjectExplorer {

/*!
    \class ProjectExplorer::IDeviceFactory

    \brief The IDeviceFactory class implements an interface for classes that
    provide services related to a certain type of device.

    The factory objects have to be added to the global object pool via
    \c ExtensionSystem::PluginManager::addObject().

    \sa ExtensionSystem::PluginManager::addObject()
*/

/*!
    \fn virtual QString displayName() const = 0

    Returns a short, one-line description of the device type this factory
    can create.
*/

/*!
    \fn virtual IDevice::Ptr create() const
    Creates a new device. This may or may not open a wizard.
*/

/*!
    \fn virtual bool canRestore(const QVariantMap &map) const = 0

    Checks whether this factory can restore a device from the serialized state
    specified by \a map.
*/

/*!
    \fn virtual IDevice::Ptr restore(const QVariantMap &map) const = 0

    Loads a device from a serialized state. Only called if \c canRestore()
    returns true for \a map.
*/

/*!
    Checks whether this factory can create new devices. This function is used
    to hide auto-detect-only factories from the listing of possible devices
    to create.
*/

auto IDeviceFactory::canCreate() const -> bool
{
  return bool(m_creator);
}

auto IDeviceFactory::create() const -> IDevice::Ptr
{
  return m_creator ? m_creator() : IDevice::Ptr();
}

auto IDeviceFactory::construct() const -> IDevice::Ptr
{
  return m_constructor ? m_constructor() : IDevice::Ptr();
}

static QList<IDeviceFactory*> g_deviceFactories;

auto IDeviceFactory::find(Id type) -> IDeviceFactory*
{
  return findOrDefault(g_deviceFactories, [&type](IDeviceFactory *factory) {
    return factory->deviceType() == type;
  });
}

IDeviceFactory::IDeviceFactory(Id deviceType) : m_deviceType(deviceType)
{
  g_deviceFactories.append(this);
}

auto IDeviceFactory::setIcon(const QIcon &icon) -> void
{
  m_icon = icon;
}

auto IDeviceFactory::setCombinedIcon(const FilePath &small, const FilePath &large) -> void
{
  using namespace Utils;
  m_icon = Icon::combinedIcon({Icon({{small, Theme::PanelTextColorDark}}, Icon::Tint), Icon({{large, Theme::IconsBaseColor}})});
}

auto IDeviceFactory::setCreator(const std::function<IDevice::Ptr ()> &creator) -> void
{
  QTC_ASSERT(creator, return);
  m_creator = creator;
}

auto IDeviceFactory::setConstructionFunction(const std::function<IDevice::Ptr ()> &constructor) -> void
{
  m_constructor = constructor;
}

auto IDeviceFactory::setDisplayName(const QString &displayName) -> void
{
  m_displayName = displayName;
}

IDeviceFactory::~IDeviceFactory()
{
  g_deviceFactories.removeOne(this);
}

auto IDeviceFactory::allDeviceFactories() -> const QList<IDeviceFactory*>
{
  return g_deviceFactories;
}

} // namespace ProjectExplorer
