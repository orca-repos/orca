// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace Orca::Plugin::LIEF {

class Plugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "lief.json")

public:
  auto initialize(const QStringList &arguments, QString *error_string) -> bool override;
  auto extensionsInitialized() -> void override;
};

} // namespace Orca::Plugin::LIEF
