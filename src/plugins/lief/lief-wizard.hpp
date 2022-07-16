// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "lief-wizard-dialog.hpp"

#include <core/core-base-file-wizard-factory.hpp>

#include <LIEF/Abstract/Binary.hpp>

namespace Orca::Plugin::LIEF {

class Wizard : public Core::BaseFileWizardFactory {
  Q_OBJECT

public:
  Wizard();

  auto create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard* override;

private:
  mutable std::unique_ptr<::LIEF::Binary> binary;
  mutable WizardDialog *m_wizard_dialog{};
};

} // namespace Orca::Plugin::LIEF
