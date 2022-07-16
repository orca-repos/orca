// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectnodes.hpp"

#include <core/core-generated-file.hpp>
#include <core/core-wizard-factory-interface.hpp>

#include <utils/wizardpage.hpp>
#include <utils/treemodel.hpp>

QT_BEGIN_NAMESPACE
class QTreeView;
class QModelIndex;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {
class IVersionControl;
}

namespace ProjectExplorer {
namespace Internal {

class AddNewTree;

namespace Ui {
class WizardPage;
}

class ProjectWizardPage : public Utils::WizardPage {
  Q_OBJECT

public:
  explicit ProjectWizardPage(QWidget *parent = nullptr);
  ~ProjectWizardPage() override;

  auto currentNode() const -> FolderNode*;
  auto setNoneLabel(const QString &label) -> void;
  auto versionControlIndex() const -> int;
  auto setVersionControlIndex(int) -> void;
  auto currentVersionControl() -> Orca::Plugin::Core::IVersionControl*;

  // Returns the common path
  auto setFiles(const QStringList &files) -> void;
  auto runVersionControl(const QList<Orca::Plugin::Core::GeneratedFile> &files, QString *errorMessage) -> bool;
  auto initializeProjectTree(Node *context, const Utils::FilePaths &paths, Orca::Plugin::Core::IWizardFactory::WizardKind kind, ProjectAction action) -> void;
  auto initializeVersionControls() -> void;
  auto setProjectUiVisible(bool visible) -> void;

signals:
  auto projectNodeChanged() -> void;
  auto versionControlChanged(int) -> void;

private:
  auto projectChanged(int) -> void;
  auto manageVcs() -> void;
  auto hideVersionControlUiElements() -> void;
  auto setAdditionalInfo(const QString &text) -> void;
  auto setAddingSubProject(bool addingSubProject) -> void;
  auto setBestNode(AddNewTree *tree) -> void;
  auto setVersionControls(const QStringList &) -> void;
  auto setProjectToolTip(const QString &) -> void;
  auto expandTree(const QModelIndex &root) -> bool;

  Ui::WizardPage *m_ui;
  QStringList m_projectToolTips;
  Utils::TreeModel<> m_model;
  QList<Orca::Plugin::Core::IVersionControl*> m_activeVersionControls;
  QString m_commonDirectory;
  bool m_repositoryExists = false;
};

} // namespace Internal
} // namespace ProjectExplorer
