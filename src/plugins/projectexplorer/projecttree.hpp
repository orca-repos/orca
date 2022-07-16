// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <core/core-context-interface.hpp>

#include <functional>

namespace Utils {
class FilePath;
}

namespace ProjectExplorer {

class BuildSystem;
class FileNode;
class FolderNode;
class Node;
class Project;
class ProjectNode;
class SessionNode;
class Target;

namespace Internal {
class ProjectTreeWidget;
}

class PROJECTEXPLORER_EXPORT ProjectTree : public QObject {
  Q_OBJECT

public:
  explicit ProjectTree(QObject *parent = nullptr);
  ~ProjectTree() override;

  static auto instance() -> ProjectTree*;
  static auto currentProject() -> Project*;
  static auto currentTarget() -> Target*;
  static auto currentBuildSystem() -> BuildSystem*;
  static auto currentNode() -> Node*;
  static auto currentFilePath() -> Utils::FilePath;

  class CurrentNodeKeeper {
  public:
    CurrentNodeKeeper();
    ~CurrentNodeKeeper();

  private:
    const bool m_active = false;
  };

  enum ConstructionPhase {
    AsyncPhase,
    FinalPhase
  };

  // Integration with ProjectTreeWidget
  static auto registerWidget(Internal::ProjectTreeWidget *widget) -> void;
  static auto unregisterWidget(Internal::ProjectTreeWidget *widget) -> void;
  static auto nodeChanged(Internal::ProjectTreeWidget *widget) -> void;
  static auto aboutToShutDown() -> void;
  static auto showContextMenu(Internal::ProjectTreeWidget *focus, const QPoint &globalPos, Node *node) -> void;
  static auto highlightProject(Project *project, const QString &message) -> void;
  using TreeManagerFunction = std::function<void(FolderNode *, ConstructionPhase)>;
  static auto registerTreeManager(const TreeManagerFunction &treeChange) -> void;
  static auto applyTreeManager(FolderNode *folder, ConstructionPhase phase) -> void;
  // Nodes:
  static auto hasNode(const Node *node) -> bool;
  static auto forEachNode(const std::function<void(Node *)> &task) -> void;
  static auto projectForNode(const Node *node) -> Project*;
  static auto nodeForFile(const Utils::FilePath &fileName) -> Node*;
  static auto siblingsWithSameBaseName(const Node *fileNode) -> const QList<Node*>;
  auto expandCurrentNodeRecursively() -> void;
  auto collapseAll() -> void;
  auto expandAll() -> void;
  auto changeProjectRootDirectory() -> void;
  // for nodes to emit signals, do not call unless you are a node
  static auto emitSubtreeChanged(FolderNode *node) -> void;

signals:
  auto currentProjectChanged(Project *project) -> void;
  auto currentNodeChanged(Node *node) -> void;
  auto nodeActionsChanged() -> void;
  // Emitted whenever the model needs to send a update signal.
  auto subtreeChanged(FolderNode *node) -> void;
  auto aboutToShowContextMenu(Node *node) -> void;
  // Emitted on any change to the tree
  auto treeChanged() -> void;

private:
  auto sessionAndTreeChanged() -> void;
  auto sessionChanged() -> void;
  auto update() -> void;
  auto updateFromProjectTreeWidget(Internal::ProjectTreeWidget *widget) -> void;
  auto updateFromDocumentManager() -> void;
  auto updateFromNode(Node *node) -> void;
  auto setCurrent(Node *node, Project *project) -> void;
  auto updateContext() -> void;
  auto updateFromFocus() -> void;
  auto updateExternalFileWarning() -> void;
  static auto hasFocus(Internal::ProjectTreeWidget *widget) -> bool;
  auto currentWidget() const -> Internal::ProjectTreeWidget*;
  auto hideContextMenu() -> void;
  
  static ProjectTree *s_instance;
  QList<QPointer<Internal::ProjectTreeWidget>> m_projectTreeWidgets;
  QVector<TreeManagerFunction> m_treeManagers;
  Node *m_currentNode = nullptr;
  Project *m_currentProject = nullptr;
  Internal::ProjectTreeWidget *m_focusForContextMenu = nullptr;
  int m_keepCurrentNodeRequests = 0;
  Orca::Plugin::Core::Context m_lastProjectContext;
};

} // namespace ProjectExplorer
