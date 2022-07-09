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
  auto create(QWidget *parent, const Core::WizardDialogParameters &parameters) const -> Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *, const Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
};

} // namespace Internal
} // namespace QmakeProjectManager
