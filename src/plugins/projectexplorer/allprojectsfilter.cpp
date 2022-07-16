// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "allprojectsfilter.hpp"
#include "projectexplorer.hpp"
#include "session.hpp"
#include "project.hpp"

#include <utils/algorithm.hpp>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;

AllProjectsFilter::AllProjectsFilter()
{
  setId("Files in any project");
  setDisplayName(tr("Files in Any Project"));
  setDescription(tr("Matches all files of all open projects. Append \"+<number>\" or " "\":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
  setDefaultShortcutString("a");
  setDefaultIncludedByDefault(true);

  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::fileListChanged, this, &AllProjectsFilter::markFilesAsOutOfDate);
}

auto AllProjectsFilter::markFilesAsOutOfDate() -> void
{
  setFileIterator(nullptr);
}

auto AllProjectsFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  if (!fileIterator()) {
    Utils::FilePaths paths;
    for (const auto project : SessionManager::projects())
      paths.append(project->files(Project::SourceFiles));
    sort(paths);
    setFileIterator(new ListIterator(paths));
  }
  BaseFileFilter::prepareSearch(entry);
}

auto AllProjectsFilter::refresh(QFutureInterface<void> &future) -> void
{
  Q_UNUSED(future)
  QMetaObject::invokeMethod(this, &AllProjectsFilter::markFilesAsOutOfDate, Qt::QueuedConnection);
}
