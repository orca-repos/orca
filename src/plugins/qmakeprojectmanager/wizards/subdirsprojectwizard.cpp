// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "subdirsprojectwizard.hpp"

#include "subdirsprojectwizarddialog.hpp"
#include "../qmakeprojectmanagerconstants.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>
#include <core/icore.hpp>
#include <qtsupport/qtsupportconstants.hpp>

#include <utils/algorithm.hpp>

#include <QCoreApplication>

using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

SubdirsProjectWizard::SubdirsProjectWizard()
{
  setId("U.Qt4Subdirs");
  setCategory(QLatin1String(ProjectExplorer::Constants::QT_PROJECT_WIZARD_CATEGORY));
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", ProjectExplorer::Constants::QT_PROJECT_WIZARD_CATEGORY_DISPLAY));
  setDisplayName(tr("Subdirs Project"));
  setDescription(tr("Creates a qmake-based subdirs project. This allows you to group " "your projects in a tree structure."));
  setIcon(themedIcon(":/wizards/images/gui.png"));
  setRequiredFeatures({QtSupport::Constants::FEATURE_QT_PREFIX});
}

auto SubdirsProjectWizard::create(QWidget *parent, const Core::WizardDialogParameters &parameters) const -> Core::BaseFileWizard*
{
  auto dialog = new SubdirsProjectWizardDialog(this, displayName(), icon(), parent, parameters);

  dialog->setProjectName(SubdirsProjectWizardDialog::uniqueProjectName(parameters.defaultPath()));
  const auto buttonText = dialog->wizardStyle() == QWizard::MacStyle ? tr("Done && Add Subproject") : tr("Finish && Add Subproject");
  dialog->setButtonText(QWizard::FinishButton, buttonText);
  return dialog;
}

auto SubdirsProjectWizard::generateFiles(const QWizard *w, QString * /*errorMessage*/) const -> Core::GeneratedFiles
{
  const auto *wizard = qobject_cast<const SubdirsProjectWizardDialog*>(w);
  const auto params = wizard->parameters();
  const auto projectPath = params.projectPath();
  const auto profileName = Core::BaseFileWizardFactory::buildFileName(projectPath, params.fileName, profileSuffix());

  Core::GeneratedFile profile(profileName);
  profile.setAttributes(Core::GeneratedFile::OpenProjectAttribute | Core::GeneratedFile::OpenEditorAttribute);
  profile.setContents(QLatin1String("TEMPLATE = subdirs\n"));
  return Core::GeneratedFiles() << profile;
}

auto SubdirsProjectWizard::postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &files, QString *errorMessage) const -> bool
{
  const auto *wizard = qobject_cast<const SubdirsProjectWizardDialog*>(w);
  if (QtWizard::qt4ProjectPostGenerateFiles(wizard, files, errorMessage)) {
    const auto params = wizard->parameters();
    const auto projectPath = params.projectPath();
    const auto profileName = Core::BaseFileWizardFactory::buildFileName(projectPath, params.fileName, profileSuffix());
    QVariantMap map;
    map.insert(QLatin1String(ProjectExplorer::Constants::PREFERRED_PROJECT_NODE), profileName.toVariant());
    map.insert(QLatin1String(ProjectExplorer::Constants::PROJECT_KIT_IDS), Utils::transform<QStringList>(wizard->selectedKits(), &Utils::Id::toString));
    IWizardFactory::requestNewItemDialog(tr("New Subproject", "Title of dialog"), Utils::filtered(Core::IWizardFactory::allWizardFactories(), [](Core::IWizardFactory *f) {
      return f->supportedProjectTypes().contains(Constants::QMAKEPROJECT_ID);
    }), wizard->parameters().projectPath(), map);
  } else {
    return false;
  }
  return true;
}

} // namespace Internal
} // namespace QmakeProjectManager
