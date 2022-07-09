// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtwizard.hpp"

#include <qmakeprojectmanager/qmakeproject.hpp>
#include <qmakeprojectmanager/qmakeprojectmanagerconstants.hpp>

#include <core/icore.hpp>

#include <cppeditor/cppeditorconstants.hpp>

#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/targetsetuppage.hpp>
#include <projectexplorer/task.hpp>

#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtsupportconstants.hpp>

#include <utils/algorithm.hpp>

#include <QCoreApplication>
#include <QVariant>

using namespace ProjectExplorer;
using namespace QtSupport;

namespace QmakeProjectManager {
namespace Internal {

// -------------------- QtWizard
QtWizard::QtWizard()
{
  setSupportedProjectTypes({Constants::QMAKEPROJECT_ID});
}

auto QtWizard::sourceSuffix() -> QString
{
  return preferredSuffix(QLatin1String(ProjectExplorer::Constants::CPP_SOURCE_MIMETYPE));
}

auto QtWizard::headerSuffix() -> QString
{
  return preferredSuffix(QLatin1String(ProjectExplorer::Constants::CPP_HEADER_MIMETYPE));
}

auto QtWizard::formSuffix() -> QString
{
  return preferredSuffix(QLatin1String(ProjectExplorer::Constants::FORM_MIMETYPE));
}

auto QtWizard::profileSuffix() -> QString
{
  return preferredSuffix(QLatin1String(Constants::PROFILE_MIMETYPE));
}

auto QtWizard::postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &l, QString *errorMessage) const -> bool
{
  return QtWizard::qt4ProjectPostGenerateFiles(w, l, errorMessage);
}

auto QtWizard::qt4ProjectPostGenerateFiles(const QWizard *w, const Core::GeneratedFiles &generatedFiles, QString *errorMessage) -> bool
{
  const auto *dialog = qobject_cast<const BaseQmakeProjectWizardDialog*>(w);

  // Generate user settings
  for (const auto &file : generatedFiles)
    if (file.attributes() & Core::GeneratedFile::OpenProjectAttribute) {
      dialog->writeUserFile(file.path());
      break;
    }

  // Post-Generate: Open the projects/editors
  return ProjectExplorer::CustomProjectWizard::postGenerateOpen(generatedFiles, errorMessage);
}

auto QtWizard::templateDir() -> QString
{
  return Core::ICore::resourcePath("templates/qt4project").toString();
}

auto QtWizard::lowerCaseFiles() -> bool
{
  QString lowerCaseSettingsKey = QLatin1String(CppEditor::Constants::CPPEDITOR_SETTINGSGROUP);
  lowerCaseSettingsKey += QLatin1Char('/');
  lowerCaseSettingsKey += QLatin1String(CppEditor::Constants::LOWERCASE_CPPFILES_KEY);
  const auto lowerCaseDefault = CppEditor::Constants::LOWERCASE_CPPFILES_DEFAULT;
  return Core::ICore::settings()->value(lowerCaseSettingsKey, QVariant(lowerCaseDefault)).toBool();
}

// ------------ CustomQmakeProjectWizard
CustomQmakeProjectWizard::CustomQmakeProjectWizard() = default;

auto CustomQmakeProjectWizard::create(QWidget *parent, const Core::WizardDialogParameters &parameters) const -> Core::BaseFileWizard*
{
  auto *wizard = new BaseQmakeProjectWizardDialog(this, parent, parameters);

  if (!parameters.extraValues().contains(QLatin1String(ProjectExplorer::Constants::PROJECT_KIT_IDS)))
    wizard->addTargetSetupPage(targetPageId);

  initProjectWizardDialog(wizard, parameters.defaultPath(), wizard->extensionPages());
  return wizard;
}

auto CustomQmakeProjectWizard::postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &l, QString *errorMessage) const -> bool
{
  return QtWizard::qt4ProjectPostGenerateFiles(w, l, errorMessage);
}

// ----------------- BaseQmakeProjectWizardDialog
BaseQmakeProjectWizardDialog::BaseQmakeProjectWizardDialog(const Core::BaseFileWizardFactory *factory, QWidget *parent, const Core::WizardDialogParameters &parameters) : ProjectExplorer::BaseProjectWizardDialog(factory, parent, parameters)
{
  m_profileIds = Utils::transform(parameters.extraValues().value(ProjectExplorer::Constants::PROJECT_KIT_IDS).toStringList(), &Utils::Id::fromString);

  connect(this, &BaseProjectWizardDialog::projectParametersChanged, this, &BaseQmakeProjectWizardDialog::generateProfileName);
}

BaseQmakeProjectWizardDialog::BaseQmakeProjectWizardDialog(const Core::BaseFileWizardFactory *factory, Utils::ProjectIntroPage *introPage, int introId, QWidget *parent, const Core::WizardDialogParameters &parameters) : ProjectExplorer::BaseProjectWizardDialog(factory, introPage, introId, parent, parameters)
{
  m_profileIds = Utils::transform(parameters.extraValues().value(ProjectExplorer::Constants::PROJECT_KIT_IDS).toStringList(), &Utils::Id::fromString);
  connect(this, &BaseProjectWizardDialog::projectParametersChanged, this, &BaseQmakeProjectWizardDialog::generateProfileName);
}

BaseQmakeProjectWizardDialog::~BaseQmakeProjectWizardDialog()
{
  if (m_targetSetupPage && !m_targetSetupPage->parent())
    delete m_targetSetupPage;
}

auto BaseQmakeProjectWizardDialog::addTargetSetupPage(int id) -> int
{
  m_targetSetupPage = new ProjectExplorer::TargetSetupPage;

  m_targetSetupPage->setTasksGenerator([this](const Kit *k) -> Tasks {
    if (!QtKitAspect::qtVersionPredicate(requiredFeatures())(k))
      return {ProjectExplorer::CompileTask(Task::Error, tr("Required Qt features not present."))};

    const auto platform = selectedPlatform();
    if (platform.isValid() && !QtKitAspect::platformPredicate(platform)(k))
      return {ProjectExplorer::CompileTask(ProjectExplorer::Task::Warning, tr("Qt version does not target the expected platform."))};
    QSet<Utils::Id> features = {QtSupport::Constants::FEATURE_DESKTOP};
    if (!QtKitAspect::qtVersionPredicate(features)(k))
      return {ProjectExplorer::CompileTask(ProjectExplorer::Task::Unknown, tr("Qt version does not provide all features."))};
    return {};
  });

  resize(900, 450);
  if (id >= 0)
    setPage(id, m_targetSetupPage);
  else
    id = addPage(m_targetSetupPage);

  return id;
}

auto BaseQmakeProjectWizardDialog::writeUserFile(const QString &proFileName) const -> bool
{
  if (!m_targetSetupPage)
    return false;

  auto pro = new QmakeProject(Utils::FilePath::fromString(proFileName));
  auto success = m_targetSetupPage->setupProject(pro);
  if (success)
    pro->saveSettings();
  delete pro;
  return success;
}

auto BaseQmakeProjectWizardDialog::selectedKits() const -> QList<Utils::Id>
{
  if (!m_targetSetupPage)
    return m_profileIds;
  return m_targetSetupPage->selectedKits();
}

auto BaseQmakeProjectWizardDialog::generateProfileName(const QString &name, const QString &path) -> void
{
  if (!m_targetSetupPage)
    return;

  const auto proFile = QDir::cleanPath(path + '/' + name + '/' + name + ".pro");

  m_targetSetupPage->setProjectPath(Utils::FilePath::fromString(proFile));
}

} // Internal
} // QmakeProjectManager
