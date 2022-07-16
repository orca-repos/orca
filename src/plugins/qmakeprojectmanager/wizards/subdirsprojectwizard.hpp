// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtwizard.hpp"

namespace QmakeProjectManager {
namespace Internal {

class SubdirsProjectWizard : public QtWizard {
  Q_OBJECT

public:
  SubdirsProjectWizard();

private:
  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
};

} // namespace Internal
} // namespace QmakeProjectManager
