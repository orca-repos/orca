// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QCoreApplication>

namespace Core {
namespace Internal {

class PluginInstallWizard {
  Q_DECLARE_TR_FUNCTIONS(Core::Internal::PluginInstallWizard)

public:
  static auto exec() -> bool;
};

} // namespace Internal
} // namespace Core
