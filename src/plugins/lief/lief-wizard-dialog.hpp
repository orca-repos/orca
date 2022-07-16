// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "lief.hpp"

#include <core/core-base-file-wizard.hpp>
#include <core/core-base-file-wizard-factory.hpp>

namespace Orca::Plugin::LIEF {

class WizardDialog : public Core::BaseFileWizard {
  Q_OBJECT

public:
  explicit WizardDialog(const Core::BaseFileWizardFactory *factory, QWidget *parent = nullptr);
  auto setPath(const QString &path) const -> void;

private:
  LIEF *m_lief;
};

} // namespace Orca::Plugin::LIEF
