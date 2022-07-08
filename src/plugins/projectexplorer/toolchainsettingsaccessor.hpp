// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/settingsaccessor.hpp>

#include <QList>

#include <memory>

namespace ProjectExplorer {

class ToolChain;

namespace Internal {

class ToolChainSettingsAccessor : public Utils::UpgradingSettingsAccessor {
public:
  ToolChainSettingsAccessor();

  auto restoreToolChains(QWidget *parent) const -> QList<ToolChain*>;
  auto saveToolChains(const QList<ToolChain*> &toolchains, QWidget *parent) -> void;

private:
  auto toolChains(const QVariantMap &data) const -> QList<ToolChain*>;
};

} // namespace Internal
} // namespace ProjectExplorer
