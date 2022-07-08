// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/ioutputpane.hpp>

#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QModelIndex;
class QPoint;
QT_END_NAMESPACE

namespace ProjectExplorer {
class TaskHub;
class Task;

namespace Internal {
class TaskWindowPrivate;

// Show issues (warnings or errors) and open the editor on click.
class TaskWindow final : public Core::IOutputPane {
  Q_OBJECT

public:
  TaskWindow();
  ~TaskWindow() override;

  auto delayedInitialization() -> void;
  auto taskCount(Utils::Id category = Utils::Id()) const -> int;
  auto warningTaskCount(Utils::Id category = Utils::Id()) const -> int;
  auto errorTaskCount(Utils::Id category = Utils::Id()) const -> int;

  // IOutputPane
  auto outputWidget(QWidget *) -> QWidget* override;
  auto toolBarWidgets() const -> QList<QWidget*> override;
  auto displayName() const -> QString override { return tr("Issues"); }
  auto priorityInStatusBar() const -> int override;
  auto clearContents() -> void override;
  auto visibilityChanged(bool visible) -> void override;
  auto canFocus() const -> bool override;
  auto hasFocus() const -> bool override;
  auto setFocus() -> void override;
  auto canNavigate() const -> bool override;
  auto canNext() const -> bool override;
  auto canPrevious() const -> bool override;
  auto goToNext() -> void override;
  auto goToPrev() -> void override;

signals:
  auto tasksChanged() -> void;

private:
  auto updateFilter() -> void override;
  auto addCategory(Utils::Id categoryId, const QString &displayName, bool visible, int priority) -> void;
  auto addTask(const Task &task) -> void;
  auto removeTask(const Task &task) -> void;
  auto updatedTaskFileName(const Task &task, const QString &fileName) -> void;
  auto updatedTaskLineNumber(const Task &task, int line) -> void;
  auto showTask(const Task &task) -> void;
  auto openTask(const Task &task) -> void;
  auto clearTasks(Utils::Id categoryId) -> void;
  auto setCategoryVisibility(Utils::Id categoryId, bool visible) -> void;
  auto saveSettings() -> void;
  auto loadSettings() -> void;
  auto triggerDefaultHandler(const QModelIndex &index) -> void;
  auto actionTriggered() -> void;
  auto setShowWarnings(bool) -> void;
  auto updateCategoriesMenu() -> void;
  auto sizeHintForColumn(int column) const -> int;

  const std::unique_ptr<TaskWindowPrivate> d;
};

} // namespace Internal
} // namespace ProjectExplorer
