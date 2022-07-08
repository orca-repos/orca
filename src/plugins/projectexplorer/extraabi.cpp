// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "extraabi.hpp"

#include "abi.hpp"

#include <core/icore.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/settingsaccessor.hpp>

#include <app/app_version.hpp>

#include <QDebug>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

class AbiFlavorUpgraderV0 : public VersionUpgrader {
public:
  AbiFlavorUpgraderV0() : VersionUpgrader(0, "") { }

  auto upgrade(const QVariantMap &data) -> QVariantMap override { return data; }
};

class AbiFlavorAccessor : public UpgradingSettingsAccessor {
public:
  AbiFlavorAccessor();
};

AbiFlavorAccessor::AbiFlavorAccessor() : UpgradingSettingsAccessor("QtCreatorExtraAbi", QCoreApplication::translate("ProjectExplorer::ToolChainManager", "ABI"), Core::Constants::IDE_DISPLAY_NAME)
{
  setBaseFilePath(Core::ICore::installerResourcePath("abi.xml"));

  addVersionUpgrader(std::make_unique<AbiFlavorUpgraderV0>());
}

// --------------------------------------------------------------------
// ExtraAbi:
// --------------------------------------------------------------------

auto ExtraAbi::load() -> void
{
  const AbiFlavorAccessor accessor;
  const auto data = accessor.restoreSettings(Core::ICore::dialogParent()).value("Flavors").toMap();
  for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
    const auto flavor = it.key();
    if (flavor.isEmpty())
      continue;

    const auto osNames = it.value().toStringList();
    std::vector<Abi::OS> oses;
    for (const auto &osName : osNames) {
      auto os = Abi::osFromString(osName);
      if (Abi::toString(os) != osName)
        qWarning() << "Invalid OS found when registering extra abi flavor" << it.key();
      else
        oses.push_back(os);
    }

    Abi::registerOsFlavor(oses, flavor);
  }
}

} // namespace Internal
} // namespace ProjectExplorer
