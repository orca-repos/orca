// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "taskhub.hpp"
#include "projectexplorerconstants.hpp"

#include <core/core-icons.hpp>
#include <core/core-output-pane-interface.hpp>
#include <texteditor/textmark.hpp>
#include <utils/qtcassert.hpp>
#include <utils/theme/theme.hpp>
#include <utils/utilsicons.hpp>

#include <QApplication>
#include <QThread>

using namespace Utils;

namespace ProjectExplorer {

// Task mark categories
constexpr char TASK_MARK_WARNING[] = "Task.Mark.Warning";
constexpr char TASK_MARK_ERROR[] = "Task.Mark.Error";

static TaskHub *m_instance = nullptr;
QVector<Id> TaskHub::m_registeredCategories;

static auto categoryForType(Task::TaskType type) -> Id
{
  switch (type) {
  case Task::Error:
    return TASK_MARK_ERROR;
  case Task::Warning:
    return TASK_MARK_WARNING;
  default:
    return Id();
  }
}

class TaskMark : public TextEditor::TextMark {
public:
  TaskMark(const Task &task) : TextMark(task.file, task.line, categoryForType(task.type)), m_task(task)
  {
    setColor(task.type == Task::Error ? Theme::ProjectExplorer_TaskError_TextMarkColor : Theme::ProjectExplorer_TaskWarn_TextMarkColor);
    setDefaultToolTip(task.type == Task::Error ? QApplication::translate("TaskHub", "Error") : QApplication::translate("TaskHub", "Warning"));
    setPriority(task.type == Task::Error ? NormalPriority : LowPriority);
    if (task.category == Constants::TASK_CATEGORY_COMPILE) {
      setToolTip("<html><body><b>" + QApplication::translate("TaskHub", "Build Issue") + "</b><br/><code style=\"white-space:pre;font-family:monospace\">" + task.description().toHtmlEscaped() + "</code></body></html>");
    } else {
      setToolTip(task.description());
    }
    setIcon(task.icon());
    setVisible(!task.icon().isNull());
  }

  auto isClickable() const -> bool override;
  auto clicked() -> void override;

  auto updateFileName(const FilePath &fileName) -> void override;
  auto updateLineNumber(int lineNumber) -> void override;
  auto removedFromEditor() -> void override;
private:
  const Task m_task;
};

auto TaskMark::updateLineNumber(int lineNumber) -> void
{
  TaskHub::updateTaskLineNumber(m_task, lineNumber);
  TextMark::updateLineNumber(lineNumber);
}

auto TaskMark::updateFileName(const FilePath &fileName) -> void
{
  TaskHub::updateTaskFileName(m_task, fileName.toString());
  TextMark::updateFileName(FilePath::fromString(fileName.toString()));
}

auto TaskMark::removedFromEditor() -> void
{
  TaskHub::updateTaskLineNumber(m_task, -1);
}

auto TaskMark::isClickable() const -> bool
{
  return true;
}

auto TaskMark::clicked() -> void
{
  TaskHub::taskMarkClicked(m_task);
}

TaskHub::TaskHub()
{
  m_instance = this;
  qRegisterMetaType<Task>("ProjectExplorer::Task");
  qRegisterMetaType<Tasks>("Tasks");
}

TaskHub::~TaskHub()
{
  m_instance = nullptr;
}

auto TaskHub::addCategory(Id categoryId, const QString &displayName, bool visible, int priority) -> void
{
  QTC_CHECK(!displayName.isEmpty());
  QTC_ASSERT(!m_registeredCategories.contains(categoryId), return);
  m_registeredCategories.push_back(categoryId);
  emit m_instance->categoryAdded(categoryId, displayName, visible, priority);
}

auto TaskHub::instance() -> TaskHub*
{
  return m_instance;
}

auto TaskHub::addTask(Task::TaskType type, const QString &description, Id category) -> void
{
  addTask(Task(type, description, {}, -1, category));
}

auto TaskHub::addTask(Task task) -> void
{
  QTC_ASSERT(m_registeredCategories.contains(task.category), return);
  QTC_ASSERT(!task.description().isEmpty(), return);
  QTC_ASSERT(!task.isNull(), return);
  QTC_ASSERT(task.m_mark.isNull(), return);
  QTC_ASSERT(QThread::currentThread() == qApp->thread(), return);

  if (task.file.isEmpty() || task.line <= 0)
    task.line = -1;
  task.movedLine = task.line;

  if ((task.options & Task::AddTextMark) && task.line != -1 && task.type != Task::Unknown)
    task.setMark(new TaskMark(task));
  emit m_instance->taskAdded(task);
}

auto TaskHub::clearTasks(Id categoryId) -> void
{
  QTC_ASSERT(!categoryId.isValid() || m_registeredCategories.contains(categoryId), return);
  emit m_instance->tasksCleared(categoryId);
}

auto TaskHub::removeTask(const Task &task) -> void
{
  emit m_instance->taskRemoved(task);
}

auto TaskHub::updateTaskFileName(const Task &task, const QString &fileName) -> void
{
  emit m_instance->taskFileNameUpdated(task, fileName);
}

auto TaskHub::updateTaskLineNumber(const Task &task, int line) -> void
{
  emit m_instance->taskLineNumberUpdated(task, line);
}

auto TaskHub::taskMarkClicked(const Task &task) -> void
{
  emit m_instance->showTask(task);
}

auto TaskHub::showTaskInEditor(const Task &task) -> void
{
  emit m_instance->openTask(task);
}

auto TaskHub::setCategoryVisibility(Id categoryId, bool visible) -> void
{
  QTC_ASSERT(m_registeredCategories.contains(categoryId), return);
  emit m_instance->categoryVisibilityChanged(categoryId, visible);
}

auto TaskHub::requestPopup() -> void
{
  emit m_instance->popupRequested(Orca::Plugin::Core::IOutputPane::NoModeSwitch);
}

} // namespace ProjectExplorer
