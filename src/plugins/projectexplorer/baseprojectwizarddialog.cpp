// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "baseprojectwizarddialog.hpp"

#include <core/documentmanager.hpp>
#include <utils/projectintropage.hpp>

#include <QDir>

/*!
    \class ProjectExplorer::BaseProjectWizardDialog

    \brief The BaseProjectWizardDialog class is the base class for project
    wizards.

    Presents the introductory page and takes care of setting the folder chosen
    as default projects' folder should the user wish to do that.
*/

using namespace Utils;

namespace ProjectExplorer {

struct BaseProjectWizardDialogPrivate {
  explicit BaseProjectWizardDialogPrivate(ProjectIntroPage *page, int id = -1) : desiredIntroPageId(id), introPage(page) {}

  const int desiredIntroPageId;
  ProjectIntroPage *introPage;
  int introPageId = -1;
  Id selectedPlatform;
  QSet<Id> requiredFeatureSet;
};

BaseProjectWizardDialog::BaseProjectWizardDialog(const Core::BaseFileWizardFactory *factory, QWidget *parent, const Core::WizardDialogParameters &parameters) : BaseFileWizard(factory, parameters.extraValues(), parent), d(std::make_unique<BaseProjectWizardDialogPrivate>(new ProjectIntroPage))
{
  setFilePath(parameters.defaultPath());
  setSelectedPlatform(parameters.selectedPlatform());
  setRequiredFeatures(parameters.requiredFeatures());
  init();
}

BaseProjectWizardDialog::BaseProjectWizardDialog(const Core::BaseFileWizardFactory *factory, ProjectIntroPage *introPage, int introId, QWidget *parent, const Core::WizardDialogParameters &parameters) : BaseFileWizard(factory, parameters.extraValues(), parent), d(std::make_unique<BaseProjectWizardDialogPrivate>(introPage, introId))
{
  setFilePath(parameters.defaultPath());
  setSelectedPlatform(parameters.selectedPlatform());
  setRequiredFeatures(parameters.requiredFeatures());
  init();
}

auto BaseProjectWizardDialog::init() -> void
{
  if (d->introPageId == -1) {
    d->introPageId = addPage(d->introPage);
  } else {
    d->introPageId = d->desiredIntroPageId;
    setPage(d->desiredIntroPageId, d->introPage);
  }
  connect(this, &QDialog::accepted, this, &BaseProjectWizardDialog::slotAccepted);
}

BaseProjectWizardDialog::~BaseProjectWizardDialog() = default;

auto BaseProjectWizardDialog::projectName() const -> QString
{
  return d->introPage->projectName();
}

auto BaseProjectWizardDialog::filePath() const -> FilePath
{
  return d->introPage->filePath();
}

auto BaseProjectWizardDialog::setIntroDescription(const QString &des) -> void
{
  d->introPage->setDescription(des);
}

auto BaseProjectWizardDialog::setFilePath(const FilePath &path) -> void
{
  d->introPage->setFilePath(path);
}

auto BaseProjectWizardDialog::setProjectName(const QString &name) -> void
{
  d->introPage->setProjectName(name);
}

auto BaseProjectWizardDialog::setProjectList(const QStringList &projectList) -> void
{
  d->introPage->setProjectList(projectList);
}

auto BaseProjectWizardDialog::setProjectDirectories(const FilePaths &directories) -> void
{
  d->introPage->setProjectDirectories(directories);
}

auto BaseProjectWizardDialog::setForceSubProject(bool force) -> void
{
  introPage()->setForceSubProject(force);
}

auto BaseProjectWizardDialog::slotAccepted() -> void
{
  if (d->introPage->useAsDefaultPath()) {
    // Store the path as default path for new projects if desired.
    Core::DocumentManager::setProjectsDirectory(filePath());
    Core::DocumentManager::setUseProjectsDirectory(true);
  }
}

auto BaseProjectWizardDialog::validateCurrentPage() -> bool
{
  if (currentId() == d->introPageId) emit projectParametersChanged(d->introPage->projectName(), d->introPage->filePath().toString());
  return BaseFileWizard::validateCurrentPage();
}

auto BaseProjectWizardDialog::introPage() const -> ProjectIntroPage*
{
  return d->introPage;
}

auto BaseProjectWizardDialog::uniqueProjectName(const FilePath &path) -> QString
{
  const QDir pathDir(path.toString());
  //: File path suggestion for a new project. If you choose
  //: to translate it, make sure it is a valid path name without blanks
  //: and using only ascii chars.
  const auto prefix = tr("untitled");
  for (unsigned i = 0; ; ++i) {
    auto name = prefix;
    if (i)
      name += QString::number(i);
    if (!pathDir.exists(name))
      return name;
  }
  return prefix;
}

auto BaseProjectWizardDialog::addExtensionPages(const QList<QWizardPage*> &wizardPageList) -> void
{
  for (const auto p : wizardPageList)
    addPage(p);
}

auto BaseProjectWizardDialog::selectedPlatform() const -> Id
{
  return d->selectedPlatform;
}

auto BaseProjectWizardDialog::setSelectedPlatform(Id platform) -> void
{
  d->selectedPlatform = platform;
}

auto BaseProjectWizardDialog::requiredFeatures() const -> QSet<Id>
{
  return d->requiredFeatureSet;
}

auto BaseProjectWizardDialog::setRequiredFeatures(const QSet<Id> &featureSet) -> void
{
  d->requiredFeatureSet = featureSet;
}

} // namespace ProjectExplorer
