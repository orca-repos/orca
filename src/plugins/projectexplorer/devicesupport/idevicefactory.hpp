// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "idevice.hpp"
#include <projectexplorer/projectexplorer_export.hpp>

#include <QIcon>
#include <QVariantMap>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT IDeviceFactory {
public:
  virtual ~IDeviceFactory();
  static auto allDeviceFactories() -> const QList<IDeviceFactory*>;

  auto deviceType() const -> Utils::Id { return m_deviceType; }
  auto displayName() const -> QString { return m_displayName; }
  auto icon() const -> QIcon { return m_icon; }
  auto canCreate() const -> bool;
  auto construct() const -> IDevice::Ptr;
  auto create() const -> IDevice::Ptr;

  virtual auto canRestore(const QVariantMap &) const -> bool { return true; }
  static auto find(Utils::Id type) -> IDeviceFactory*;

protected:
  explicit IDeviceFactory(Utils::Id deviceType);
  IDeviceFactory(const IDeviceFactory &) = delete;
  auto operator=(const IDeviceFactory &) -> IDeviceFactory& = delete;
  auto setDisplayName(const QString &displayName) -> void;
  auto setIcon(const QIcon &icon) -> void;
  auto setCombinedIcon(const Utils::FilePath &small, const Utils::FilePath &large) -> void;
  auto setConstructionFunction(const std::function<IDevice::Ptr ()> &constructor) -> void;
  auto setCreator(const std::function<IDevice::Ptr()> &creator) -> void;

private:
  std::function<IDevice::Ptr()> m_creator;
  const Utils::Id m_deviceType;
  QString m_displayName;
  QIcon m_icon;
  std::function<IDevice::Ptr()> m_constructor;
};

} // namespace ProjectExplorer
