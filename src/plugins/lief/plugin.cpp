// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "plugin.hpp"

namespace LIEF {

auto newFileWizardFactory() -> void;
auto newProjectWizardFactory() -> void;

auto Plugin::initialize(const QStringList &, QString *) -> bool
{
  return true;
}

auto Plugin::extensionsInitialized() -> void
{
  newFileWizardFactory();
  newProjectWizardFactory();
}

} // namespace LIEF
