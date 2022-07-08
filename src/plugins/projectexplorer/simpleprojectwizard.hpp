// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/basefilewizardfactory.hpp>

namespace ProjectExplorer {
namespace Internal {

class SimpleProjectWizard : public Core::BaseFileWizardFactory {
  Q_OBJECT

public:
  SimpleProjectWizard();

private:
  auto create(QWidget *parent, const Core::WizardDialogParameters &parameters) const -> Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
};

} // namespace Internal
} // namespace ProjectExplorer
