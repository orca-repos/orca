// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QCoreApplication>

namespace Orca::Plugin::Core {

class PluginInstallWizard {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::PluginInstallWizard)

public:
  static auto exec() -> bool;
};

} // namespace Orca::Plugin::Core
