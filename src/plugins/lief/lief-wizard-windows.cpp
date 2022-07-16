// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief-wizard-windows.hpp"

namespace Orca::Plugin::LIEF {

Windows::Windows()
{
  setId("LIEF.Wizard.Windows");
  setIcon(QIcon{":/lief/images/logo/Windows.png"});
  setDisplayName(tr("Microsoft Windows"));
  setDisplayCategory(QLatin1String("LIEF"));
  setSupportedProjectTypes({Constants::LIEFPROJECT_ID});
}

auto Windows::generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles
{
  return {};
}

auto Windows::postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool
{
  return {};
}

} // namespace Orca::Plugin::LIEF
