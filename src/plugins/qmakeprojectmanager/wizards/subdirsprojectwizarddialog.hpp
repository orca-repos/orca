// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtwizard.hpp"

namespace QmakeProjectManager {
namespace Internal {

struct QtProjectParameters;

class SubdirsProjectWizardDialog : public BaseQmakeProjectWizardDialog {
  Q_OBJECT

public:
  explicit SubdirsProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, const QString &templateName, const QIcon &icon, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);

  auto parameters() const -> QtProjectParameters;
};

} // namespace Internal
} // namespace QmakeProjectManager
