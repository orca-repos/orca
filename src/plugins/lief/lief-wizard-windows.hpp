// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "lief-wizard.hpp"

namespace Orca::Plugin::LIEF {

namespace Constants {
constexpr char LIEFPROJECT_ID[] = "LIEF.Project";
} // namespace Constants

class Windows : public Wizard {
public:
  Windows();

  auto generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool override;
};

} // namespace Orca::Plugin::LIEF
