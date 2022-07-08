// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "task.hpp"

#include <QObject>
#include <QIcon>

namespace ProjectExplorer {

class ProjectExplorerPlugin;

class PROJECTEXPLORER_EXPORT TaskHub : public QObject {
  Q_OBJECT

public:
  static auto instance() -> TaskHub*;

  // Convenience overload
  static auto addTask(Task::TaskType type, const QString &description, Utils::Id category) -> void;

public slots:
  static auto addTask(Task task) -> void;
  static auto clearTasks(Utils::Id categoryId = Utils::Id()) -> void;
  static auto removeTask(const Task &task) -> void;

public:
  static auto addCategory(Utils::Id categoryId, const QString &displayName, bool visible = true, int priority = 0) -> void;
  static auto updateTaskFileName(const Task &task, const QString &fileName) -> void;
  static auto updateTaskLineNumber(const Task &task, int line) -> void;
  static auto taskMarkClicked(const Task &task) -> void;
  static auto showTaskInEditor(const Task &task) -> void;
  static auto setCategoryVisibility(Utils::Id categoryId, bool visible) -> void;
  static auto requestPopup() -> void;

signals:
  auto categoryAdded(Utils::Id categoryId, const QString &displayName, bool visible, int priority) -> void;
  auto taskAdded(const Task &task) -> void;
  auto taskRemoved(const Task &task) -> void;
  auto tasksCleared(Utils::Id categoryId) -> void;
  auto taskFileNameUpdated(const Task &task, const QString &fileName) -> void;
  auto taskLineNumberUpdated(const Task &task, int line) -> void;
  auto categoryVisibilityChanged(Utils::Id categoryId, bool visible) -> void;
  auto popupRequested(int) -> void;
  auto showTask(const Task &task) -> void;
  auto openTask(const Task &task) -> void;

private:
  TaskHub();
  ~TaskHub() override;

  static QVector<Utils::Id> m_registeredCategories;
  friend class ProjectExplorerPluginPrivate;
};

} // namespace ProjectExplorer
