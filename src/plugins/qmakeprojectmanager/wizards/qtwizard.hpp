// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtprojectparameters.hpp"
#include <projectexplorer/baseprojectwizarddialog.hpp>
#include <projectexplorer/customwizard/customwizard.hpp>

namespace ProjectExplorer {
class Kit;
class TargetSetupPage;
} // namespace ProjectExplorer

namespace QmakeProjectManager {

class QmakeProject;

namespace Internal {

/* Base class for wizard creating Qt projects using QtProjectParameters.
 * To implement a project wizard, overwrite:
 * - createWizardDialog() to create up the dialog
 * - generateFiles() to set their contents
 * The base implementation provides the wizard parameters and opens
 * the finished project in postGenerateFiles().
 * The pro-file must be the last one of the generated files. */

class QtWizard : public Orca::Plugin::Core::BaseFileWizardFactory {
  Q_OBJECT

protected:
  QtWizard();

public:
  static auto templateDir() -> QString;
  static auto sourceSuffix() -> QString;
  static auto headerSuffix() -> QString;
  static auto formSuffix() -> QString;
  static auto profileSuffix() -> QString;

  // Query CppEditor settings for the class wizard settings
  static auto lowerCaseFiles() -> bool;
  static auto qt4ProjectPostGenerateFiles(const QWizard *w, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) -> bool;

private:
  auto postGenerateFiles(const QWizard *w, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;
};

// A custom wizard with an additional Qt 4 target page
class CustomQmakeProjectWizard : public ProjectExplorer::CustomProjectWizard {
  Q_OBJECT

public:
  CustomQmakeProjectWizard();

private:
  auto create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard* override;
  auto postGenerateFiles(const QWizard *, const Orca::Plugin::Core::GeneratedFiles &l, QString *errorMessage) const -> bool override;

  enum { targetPageId = 1 };
};

/* BaseQmakeProjectWizardDialog: Additionally offers modules page
 * and getter/setter for blank-delimited modules list, transparently
 * handling the visibility of the modules page list as well as a page
 * to select targets and Qt versions.
 */

class BaseQmakeProjectWizardDialog : public ProjectExplorer::BaseProjectWizardDialog {
  Q_OBJECT
protected:
  explicit BaseQmakeProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, Utils::ProjectIntroPage *introPage, int introId, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);

public:
  explicit BaseQmakeProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);
  ~BaseQmakeProjectWizardDialog() override;

  auto addTargetSetupPage(int id = -1) -> int;
  auto writeUserFile(const QString &proFileName) const -> bool;
  auto selectedKits() const -> QList<Utils::Id>;

private:
  auto generateProfileName(const QString &name, const QString &path) -> void;

  ProjectExplorer::TargetSetupPage *m_targetSetupPage = nullptr;
  QList<Utils::Id> m_profileIds;
};

} // namespace Internal
} // namespace QmakeProjectManager
