// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-base-file-wizard-factory.hpp>

namespace ProjectExplorer {
namespace Internal {

class SimpleProjectWizard : public Orca::Plugin::Core::BaseFileWizardFactory {
  Q_OBJECT

public:
  SimpleProjectWizard();

private:
  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *w, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
};

} // namespace Internal
} // namespace ProjectExplorer
