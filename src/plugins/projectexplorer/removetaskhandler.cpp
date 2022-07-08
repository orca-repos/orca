// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "removetaskhandler.hpp"

#include "task.hpp"
#include "taskhub.hpp"

#include <QAction>

namespace ProjectExplorer {
namespace Internal {

auto RemoveTaskHandler::handle(const Tasks &tasks) -> void
{
  for (const auto &task : tasks)
    TaskHub::removeTask(task);
}

auto RemoveTaskHandler::createAction(QObject *parent) const -> QAction*
{
  const auto removeAction = new QAction(tr("Remove", "Name of the action triggering the removetaskhandler"), parent);
  removeAction->setToolTip(tr("Remove task from the task list."));
  removeAction->setShortcuts({QKeySequence::Delete, QKeySequence::Backspace});
  removeAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  return removeAction;
}

} // namespace Internal
} // namespace ProjectExplorer
