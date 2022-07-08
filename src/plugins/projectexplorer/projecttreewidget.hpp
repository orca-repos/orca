// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "expanddata.hpp"

#include <core/inavigationwidgetfactory.hpp>

#include <utils/fileutils.hpp>

#include <QWidget>
#include <QModelIndex>
#include <QSet>

QT_FORWARD_DECLARE_CLASS(QTreeView)

namespace ProjectExplorer {

class Node;

namespace Internal {

class FlatModel;

class ProjectTreeWidget : public QWidget {
  Q_OBJECT

public:
  explicit ProjectTreeWidget(QWidget *parent = nullptr);
  ~ProjectTreeWidget() override;

  auto autoSynchronization() const -> bool;
  auto setAutoSynchronization(bool sync) -> void;
  auto projectFilter() -> bool;
  auto generatedFilesFilter() -> bool;
  auto disabledFilesFilter() -> bool;
  auto trimEmptyDirectoriesFilter() -> bool;
  auto hideSourceGroups() -> bool;
  auto currentNode() -> Node*;
  auto sync(Node *node) -> void;
  auto showMessage(Node *node, const QString &message) -> void;
  static auto nodeForFile(const Utils::FilePath &fileName) -> Node*;
  auto toggleAutoSynchronization() -> void;
  auto editCurrentItem() -> void;
  auto expandCurrentNodeRecursively() -> void;
  auto collapseAll() -> void;
  auto expandAll() -> void;
  auto createToolButtons() -> QList<QToolButton*>;

private:
  auto setProjectFilter(bool filter) -> void;
  auto setGeneratedFilesFilter(bool filter) -> void;
  auto setDisabledFilesFilter(bool filter) -> void;
  auto setTrimEmptyDirectories(bool filter) -> void;
  auto setHideSourceGroups(bool filter) -> void;
  auto handleCurrentItemChange(const QModelIndex &current) -> void;
  auto showContextMenu(const QPoint &pos) -> void;
  auto openItem(const QModelIndex &mainIndex) -> void;
  auto setCurrentItem(Node *node) -> void;
  static auto expandedCount(Node *node) -> int;
  auto rowsInserted(const QModelIndex &parent, int start, int end) -> void;
  auto renamed(const Utils::FilePath &oldPath, const Utils::FilePath &newPath) -> void;
  auto syncFromDocumentManager() -> void;
  auto expandNodeRecursively(const QModelIndex &index) -> void;

  QTreeView *m_view = nullptr;
  FlatModel *m_model = nullptr;
  QAction *m_filterProjectsAction = nullptr;
  QAction *m_filterGeneratedFilesAction = nullptr;
  QAction *m_filterDisabledFilesAction = nullptr;
  QAction *m_trimEmptyDirectoriesAction = nullptr;
  QAction *m_toggleSync = nullptr;
  QAction *m_hideSourceGroupsAction = nullptr;
  QString m_modelId;
  bool m_autoSync = true;
  QList<Utils::FilePath> m_delayedRename;

  static QList<ProjectTreeWidget*> m_projectTreeWidgets;
  friend class ProjectTreeWidgetFactory;
};

class ProjectTreeWidgetFactory : public Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  ProjectTreeWidgetFactory();

  auto createWidget() -> Core::NavigationView override;
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
};

} // namespace Internal
} // namespace ProjectExplorer
