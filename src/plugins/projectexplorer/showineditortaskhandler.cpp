// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "showineditortaskhandler.hpp"

#include "task.hpp"

#include <core/core-editor-manager.hpp>

#include <QAction>
#include <QFileInfo>

namespace ProjectExplorer {
namespace Internal {

auto ShowInEditorTaskHandler::canHandle(const Task &task) const -> bool
{
  if (task.file.isEmpty())
    return false;
  const auto fi(task.file.toFileInfo());
  return fi.exists() && fi.isFile() && fi.isReadable();
}

auto ShowInEditorTaskHandler::handle(const Task &task) -> void
{
  const auto column = task.column ? task.column - 1 : 0;
  Orca::Plugin::Core::EditorManager::openEditorAt({task.file, task.movedLine, column}, {}, Orca::Plugin::Core::EditorManager::SwitchSplitIfAlreadyVisible);
}

auto ShowInEditorTaskHandler::createAction(QObject *parent) const -> QAction*
{
  const auto showAction = new QAction(tr("Show in Editor"), parent);
  showAction->setToolTip(tr("Show task location in an editor."));
  showAction->setShortcut(QKeySequence(Qt::Key_Return));
  showAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  return showAction;
}

} // namespace Internal
} // namespace ProjectExplorer
