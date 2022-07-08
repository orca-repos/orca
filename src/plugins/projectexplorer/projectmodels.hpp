// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "expanddata.hpp"
#include "projectnodes.hpp"

#include <utils/fileutils.hpp>
#include <utils/treemodel.hpp>

#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QTreeView>

namespace ProjectExplorer {

class Node;
class FolderNode;
class Project;
class ProjectNode;

namespace Internal {

auto compareNodes(const Node *n1, const Node *n2) -> bool;

class WrapperNode : public Utils::TypedTreeItem<WrapperNode> {
public:
  explicit WrapperNode(Node *node) : m_node(node) {}
  Node *m_node = nullptr;

  auto appendClone(const WrapperNode &node) -> void;
};

class FlatModel : public Utils::TreeModel<WrapperNode, WrapperNode> {
  Q_OBJECT

public:
  FlatModel(QObject *parent);

  // QAbstractItemModel
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool override;
  auto supportedDragActions() const -> Qt::DropActions override;
  auto mimeTypes() const -> QStringList override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const -> bool override;
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool override;
  auto nodeForIndex(const QModelIndex &index) const -> Node*;
  auto wrapperForNode(const Node *node) const -> WrapperNode*;
  auto indexForNode(const Node *node) const -> QModelIndex;
  auto projectFilterEnabled() -> bool;
  auto generatedFilesFilterEnabled() -> bool;
  auto disabledFilesFilterEnabled() const -> bool { return m_filterDisabledFiles; }
  auto trimEmptyDirectoriesEnabled() -> bool;
  auto hideSourceGroups() -> bool { return m_hideSourceGroups; }
  auto setProjectFilterEnabled(bool filter) -> void;
  auto setGeneratedFilesFilterEnabled(bool filter) -> void;
  auto setDisabledFilesFilterEnabled(bool filter) -> void;
  auto setTrimEmptyDirectories(bool filter) -> void;
  auto setHideSourceGroups(bool filter) -> void;
  auto onExpanded(const QModelIndex &idx) -> void;
  auto onCollapsed(const QModelIndex &idx) -> void;

signals:
  auto renamed(const Utils::FilePath &oldName, const Utils::FilePath &newName) -> void;

private:
  bool m_filterProjects = false;
  bool m_filterGeneratedFiles = true;
  bool m_filterDisabledFiles = false;
  bool m_trimEmptyDirectories = true;
  bool m_hideSourceGroups = true;

  static auto logger() -> const QLoggingCategory&;
  auto updateSubtree(FolderNode *node) -> void;
  auto rebuildModel() -> void;
  auto addFolderNode(WrapperNode *parent, FolderNode *folderNode, QSet<Node*> *seen) -> void;
  auto trimEmptyDirectories(WrapperNode *parent) -> bool;
  auto expandDataForNode(const Node *node) const -> ExpandData;
  auto loadExpandData() -> void;
  auto saveExpandData() -> void;
  auto handleProjectAdded(Project *project) -> void;
  auto handleProjectRemoved(Project *project) -> void;
  auto nodeForProject(const Project *project) const -> WrapperNode*;
  auto addOrRebuildProjectModel(Project *project) -> void;
  auto parsingStateChanged(Project *project) -> void;

  QTimer m_timer;
  QSet<ExpandData> m_toExpand;
};

} // namespace Internal
} // namespace ProjectExplorer
