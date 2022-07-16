// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <core/core-base-file-wizard.hpp>
#include <core/core-base-file-wizard-factory.hpp>

#include <utils/filepath.hpp>

#include <memory>

namespace Utils {
class ProjectIntroPage;
}

namespace ProjectExplorer {

struct BaseProjectWizardDialogPrivate;

class PROJECTEXPLORER_EXPORT BaseProjectWizardDialog : public Orca::Plugin::Core::BaseFileWizard {
  Q_OBJECT

protected:
  explicit BaseProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, Utils::ProjectIntroPage *introPage, int introId, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);

public:
  explicit BaseProjectWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters);
  ~BaseProjectWizardDialog() override;

  auto projectName() const -> QString;
  auto filePath() const -> Utils::FilePath;

  // Generate a new project name (untitled<n>) in path.
  static auto uniqueProjectName(const Utils::FilePath &path) -> QString;
  auto addExtensionPages(const QList<QWizardPage*> &wizardPageList) -> void;
  auto setIntroDescription(const QString &d) -> void;
  auto setFilePath(const Utils::FilePath &path) -> void;
  auto setProjectName(const QString &name) -> void;
  auto setProjectList(const QStringList &projectList) -> void;
  auto setProjectDirectories(const Utils::FilePaths &directories) -> void;
  auto setForceSubProject(bool force) -> void;

signals:
  auto projectParametersChanged(const QString &projectName, const QString &path) -> void;

protected:
  auto introPage() const -> Utils::ProjectIntroPage*;
  auto selectedPlatform() const -> Utils::Id;
  auto setSelectedPlatform(Utils::Id platform) -> void;
  auto requiredFeatures() const -> QSet<Utils::Id>;
  auto setRequiredFeatures(const QSet<Utils::Id> &featureSet) -> void;

private:
  auto init() -> void;
  auto slotAccepted() -> void;
  auto validateCurrentPage() -> bool override;

  std::unique_ptr<BaseProjectWizardDialogPrivate> d;
};

} // namespace ProjectExplorer
