// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "currentprojectfilter.hpp"
#include "projecttree.hpp"
#include "project.hpp"

#include <utils/algorithm.hpp>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

CurrentProjectFilter::CurrentProjectFilter() : BaseFileFilter()
{
  setId("Files in current project");
  setDisplayName(tr("Files in Current Project"));
  setDescription(tr("Matches all files from the current document's project. Append \"+<number>\" " "or \":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
  setDefaultShortcutString("p");
  setDefaultIncludedByDefault(false);

  connect(ProjectTree::instance(), &ProjectTree::currentProjectChanged, this, &CurrentProjectFilter::currentProjectChanged);
}

auto CurrentProjectFilter::markFilesAsOutOfDate() -> void
{
  setFileIterator(nullptr);
}

auto CurrentProjectFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  if (!fileIterator()) {
    Utils::FilePaths paths;
    if (m_project)
      paths = m_project->files(Project::SourceFiles);
    setFileIterator(new ListIterator(paths));
  }
  BaseFileFilter::prepareSearch(entry);
}

auto CurrentProjectFilter::currentProjectChanged() -> void
{
  const auto project = ProjectTree::currentProject();
  if (project == m_project)
    return;
  if (m_project)
    disconnect(m_project, &Project::fileListChanged, this, &CurrentProjectFilter::markFilesAsOutOfDate);

  if (project)
    connect(project, &Project::fileListChanged, this, &CurrentProjectFilter::markFilesAsOutOfDate);

  m_project = project;
  markFilesAsOutOfDate();
}

auto CurrentProjectFilter::refresh(QFutureInterface<void> &future) -> void
{
  Q_UNUSED(future)
  QMetaObject::invokeMethod(this, &CurrentProjectFilter::markFilesAsOutOfDate, Qt::QueuedConnection);
}
