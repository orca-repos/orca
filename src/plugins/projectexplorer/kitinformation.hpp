// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abi.hpp"
#include "devicesupport/idevice.hpp"
#include "kitmanager.hpp"
#include "kit.hpp"

#include <utils/environment.hpp>

#include <QVariant>

namespace ProjectExplorer {

class OutputTaskParser;
class ToolChain;
class KitAspectWidget;

// --------------------------------------------------------------------------
// SysRootInformation:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT SysRootKitAspect : public KitAspect {
  Q_OBJECT

public:
  SysRootKitAspect();

  auto validate(const Kit *k) const -> Tasks override;
  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto toUserOutput(const Kit *k) const -> ItemList override;
  auto addToMacroExpander(Kit *kit, Utils::MacroExpander *expander) const -> void override;

  static auto id() -> Utils::Id;
  static auto sysRoot(const Kit *k) -> Utils::FilePath;
  static auto setSysRoot(Kit *k, const Utils::FilePath &v) -> void;
};

// --------------------------------------------------------------------------
// ToolChainInformation:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT ToolChainKitAspect : public KitAspect {
  Q_OBJECT

public:
  ToolChainKitAspect();

  auto validate(const Kit *k) const -> Tasks override;
  auto upgrade(Kit *k) -> void override;
  auto fix(Kit *k) -> void override;
  auto setup(Kit *k) -> void override;

  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto displayNamePostfix(const Kit *k) const -> QString override;
  auto toUserOutput(const Kit *k) const -> ItemList override;
  auto addToBuildEnvironment(const Kit *k, Utils::Environment &env) const -> void override;
  auto addToRunEnvironment(const Kit *, Utils::Environment &) const -> void override {}
  auto addToMacroExpander(Kit *kit, Utils::MacroExpander *expander) const -> void override;
  auto createOutputParsers(const Kit *k) const -> QList<Utils::OutputLineParser*> override;
  auto availableFeatures(const Kit *k) const -> QSet<Utils::Id> override;

  static auto id() -> Utils::Id;
  static auto toolChainId(const Kit *k, Utils::Id language) -> QByteArray;
  static auto toolChain(const Kit *k, Utils::Id language) -> ToolChain*;
  static auto cToolChain(const Kit *k) -> ToolChain*;
  static auto cxxToolChain(const Kit *k) -> ToolChain*;
  static auto toolChains(const Kit *k) -> QList<ToolChain*>;
  static auto setToolChain(Kit *k, ToolChain *tc) -> void;
  static auto setAllToolChainsToMatch(Kit *k, ToolChain *tc) -> void;
  static auto clearToolChain(Kit *k, Utils::Id language) -> void;
  static auto targetAbi(const Kit *k) -> Abi;
  static auto msgNoToolChainInTarget() -> QString;

private:
  auto kitsWereLoaded() -> void;
  auto toolChainUpdated(ToolChain *tc) -> void;
  auto toolChainRemoved(ToolChain *tc) -> void;
};

// --------------------------------------------------------------------------
// DeviceTypeInformation:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT DeviceTypeKitAspect : public KitAspect {
  Q_OBJECT

public:
  DeviceTypeKitAspect();

  auto setup(Kit *k) -> void override;
  auto validate(const Kit *k) const -> Tasks override;
  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto toUserOutput(const Kit *k) const -> ItemList override;

  static auto id() -> const Utils::Id;
  static auto deviceTypeId(const Kit *k) -> const Utils::Id;
  static auto setDeviceTypeId(Kit *k, Utils::Id type) -> void;

  auto supportedPlatforms(const Kit *k) const -> QSet<Utils::Id> override;
  auto availableFeatures(const Kit *k) const -> QSet<Utils::Id> override;
};

// --------------------------------------------------------------------------
// DeviceInformation:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT DeviceKitAspect : public KitAspect {
  Q_OBJECT

public:
  DeviceKitAspect();

  auto validate(const Kit *k) const -> Tasks override;
  auto fix(Kit *k) -> void override;
  auto setup(Kit *k) -> void override;

  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto displayNamePostfix(const Kit *k) const -> QString override;
  auto toUserOutput(const Kit *k) const -> ItemList override;
  auto addToMacroExpander(Kit *kit, Utils::MacroExpander *expander) const -> void override;

  static auto id() -> Utils::Id;
  static auto device(const Kit *k) -> IDevice::ConstPtr;
  static auto deviceId(const Kit *k) -> Utils::Id;
  static auto setDevice(Kit *k, IDevice::ConstPtr dev) -> void;
  static auto setDeviceId(Kit *k, Utils::Id dataId) -> void;

private:
  auto defaultValue(const Kit *k) const -> QVariant;

  auto kitsWereLoaded() -> void;
  auto deviceUpdated(Utils::Id dataId) -> void;
  auto devicesChanged() -> void;
  auto kitUpdated(Kit *k) -> void;
};

// --------------------------------------------------------------------------
// BuildDeviceInformation:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT BuildDeviceKitAspect : public KitAspect {
  Q_OBJECT

public:
  BuildDeviceKitAspect();

  auto setup(Kit *k) -> void override;
  auto validate(const Kit *k) const -> Tasks override;
  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto displayNamePostfix(const Kit *k) const -> QString override;
  auto toUserOutput(const Kit *k) const -> ItemList override;
  auto addToMacroExpander(Kit *kit, Utils::MacroExpander *expander) const -> void override;

  static auto id() -> Utils::Id;
  static auto device(const Kit *k) -> IDevice::ConstPtr;
  static auto deviceId(const Kit *k) -> Utils::Id;
  static auto setDevice(Kit *k, IDevice::ConstPtr dev) -> void;
  static auto setDeviceId(Kit *k, Utils::Id dataId) -> void;

private:
  static auto defaultDevice() -> IDevice::ConstPtr;

  auto kitsWereLoaded() -> void;
  auto deviceUpdated(Utils::Id dataId) -> void;
  auto devicesChanged() -> void;
  auto kitUpdated(Kit *k) -> void;
};

// --------------------------------------------------------------------------
// EnvironmentKitAspect:
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT EnvironmentKitAspect : public KitAspect {
  Q_OBJECT

public:
  EnvironmentKitAspect();

  auto validate(const Kit *k) const -> Tasks override;
  auto fix(Kit *k) -> void override;
  auto addToBuildEnvironment(const Kit *k, Utils::Environment &env) const -> void override;
  auto addToRunEnvironment(const Kit *, Utils::Environment &) const -> void override;
  auto createConfigWidget(Kit *k) const -> KitAspectWidget* override;
  auto toUserOutput(const Kit *k) const -> ItemList override;

  static auto id() -> Utils::Id;
  static auto environmentChanges(const Kit *k) -> Utils::EnvironmentItems;
  static auto setEnvironmentChanges(Kit *k, const Utils::EnvironmentItems &changes) -> void;
};

} // namespace ProjectExplorer
