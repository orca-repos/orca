// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "vcsannotatetaskhandler.hpp"

#include "task.hpp"

#include <core/iversioncontrol.hpp>
#include <core/vcsmanager.hpp>
#include <utils/qtcassert.hpp>

#include <QAction>
#include <QFileInfo>

using namespace Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

auto VcsAnnotateTaskHandler::canHandle(const Task &task) const -> bool
{
  const auto fi(task.file.toFileInfo());
  if (!fi.exists() || !fi.isFile() || !fi.isReadable())
    return false;
  const auto vc = VcsManager::findVersionControlForDirectory(task.file.absolutePath());
  if (!vc)
    return false;
  return vc->supportsOperation(IVersionControl::AnnotateOperation);
}

auto VcsAnnotateTaskHandler::handle(const Task &task) -> void
{
  const auto vc = VcsManager::findVersionControlForDirectory(task.file.absolutePath());
  QTC_ASSERT(vc, return);
  QTC_ASSERT(vc->supportsOperation(IVersionControl::AnnotateOperation), return);
  vc->vcsAnnotate(task.file.absoluteFilePath(), task.movedLine);
}

auto VcsAnnotateTaskHandler::createAction(QObject *parent) const -> QAction*
{
  const auto vcsannotateAction = new QAction(tr("&Annotate"), parent);
  vcsannotateAction->setToolTip(tr("Annotate using version control system."));
  return vcsannotateAction;
}

} // namespace Internal
} // namespace ProjectExplorer
