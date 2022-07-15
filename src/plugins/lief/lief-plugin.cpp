// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief-plugin.hpp"

#include "lief-system-windows.hpp"

namespace Orca::Plugin::LIEF {

auto Plugin::initialize(const QStringList &, QString *) -> bool
{
  return true;
}

auto Plugin::extensionsInitialized() -> void
{
  Core::IWizardFactory::registerFactoryCreator([] {
    return QList<Core::IWizardFactory*>{new Windows};
  });
}

} // namespace Orca::Plugin::LIEF