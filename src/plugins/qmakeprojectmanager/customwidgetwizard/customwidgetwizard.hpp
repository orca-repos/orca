// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../wizards/qtwizard.hpp"

namespace QmakeProjectManager {
namespace Internal {

class CustomWidgetWizard : public QtWizard {
  Q_OBJECT

public:
  CustomWidgetWizard();

protected:
  auto create(QWidget *parent, const Core::WizardDialogParameters &parameters) const -> Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *w, QString *errorMessage) const -> Core::GeneratedFiles override;
};

} // namespace Internal
} // namespace QmakeProjectManager
