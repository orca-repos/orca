// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-base-file-wizard-factory.hpp>

#include <LIEF/PE/Binary.hpp>

namespace Orca::Plugin::LIEF {
namespace Constants {
constexpr char LIEFPROJECT_ID[] = "LIEF.Project";
} // namespace Constants

struct Windows final : Orca::Plugin::Core::BaseFileWizardFactory {
  Windows() {
    setId("LIEF.NewFileWizard.Windows");
    setIcon(QIcon{":/core/images/orcalogo-big.png"}); // TODO: Use appropriate icons to represent each Projects.
    setDisplayName(tr("Microsoft Windows"));
    setDisplayCategory(QLatin1String("LIEF"));
    setSupportedProjectTypes({Constants::LIEFPROJECT_ID});
  }

  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &params) const -> Orca::Plugin::Core::BaseFileWizard*;
  auto generateFiles(const QWizard *wizard, QString *error_message) const -> Orca::Plugin::Core::GeneratedFiles;
  auto postGenerateFiles(const QWizard *wizard, const Orca::Plugin::Core::GeneratedFiles &files, QString *error_message) const -> bool;

  mutable std::unique_ptr<::LIEF::PE::Binary> binary;
};

} // namespace Orca::Plugin::LIEF
