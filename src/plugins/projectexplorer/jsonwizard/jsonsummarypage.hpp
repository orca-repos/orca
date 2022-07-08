// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectwizardpage.hpp"
#include "jsonwizard.hpp"

#include <QWizardPage>

namespace ProjectExplorer {

class FolderNode;
class Node;

class JsonSummaryPage : public Internal::ProjectWizardPage {
  Q_OBJECT

public:
  JsonSummaryPage(QWidget *parent = nullptr);

  auto setHideProjectUiValue(const QVariant &hideProjectUiValue) -> void;
  auto initializePage() -> void override;
  auto validatePage() -> bool override;
  auto cleanupPage() -> void override;
  auto triggerCommit(const JsonWizard::GeneratorFiles &files) -> void;
  auto addToProject(const JsonWizard::GeneratorFiles &files) -> void;
  auto summarySettingsHaveChanged() -> void;

private:
  auto findWizardContextNode(Node *contextNode) const -> Node*;
  auto updateFileList() -> void;
  auto updateProjectData(FolderNode *node) -> void;

  JsonWizard *m_wizard;
  JsonWizard::GeneratorFiles m_fileList;
  QVariant m_hideProjectUiValue;
};

} // namespace ProjectExplorer
