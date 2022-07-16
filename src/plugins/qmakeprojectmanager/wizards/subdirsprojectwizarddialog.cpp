// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "subdirsprojectwizarddialog.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/filepath.hpp>

namespace QmakeProjectManager {
namespace Internal {

SubdirsProjectWizardDialog::SubdirsProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, const QString &templateName, const QIcon &icon, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) : BaseQmakeProjectWizardDialog(factory, parent, parameters)
{
  setWindowIcon(icon);
  setWindowTitle(templateName);

  setIntroDescription(tr("This wizard generates a Qt Subdirs project. " "Add subprojects to it later on by using the other wizards."));

  if (!parameters.extraValues().contains(QLatin1String(ProjectExplorer::Constants::PROJECT_KIT_IDS)))
    addTargetSetupPage();

  addExtensionPages(extensionPages());
}

auto SubdirsProjectWizardDialog::parameters() const -> QtProjectParameters
{
  QtProjectParameters rc;
  rc.type = QtProjectParameters::EmptyProject;
  rc.fileName = projectName();
  rc.path = filePath();
  return rc;
}

} // namespace Internal
} // namespace QmakeProjectManager
