// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectnodes.hpp"

#include <core/iversioncontrol.hpp>
#include <core/vcsmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>

namespace ProjectExplorer {

template<typename Result>
auto scanForFiles(QFutureInterface<Result> &future, const Utils::FilePath &directory, const std::function<FileNode *(const Utils::FilePath &)> factory) -> QList<FileNode*>;

// IMPLEMENTATION:

namespace Internal {
template<typename Result>
auto scanForFilesRecursively(QFutureInterface<Result> &future, double progressStart, double progressRange, const Utils::FilePath &directory, const std::function<FileNode *(const Utils::FilePath &)> factory, QSet<QString> &visited, const QList<Core::IVersionControl*> &versionControls) -> QList<FileNode*>
{
  QList<FileNode*> result;

  const auto baseDir = QDir(directory.toString());

  // Do not follow directory loops:
  const int visitedCount = visited.count();
  visited.insert(baseDir.canonicalPath());
  if (visitedCount == visited.count())
    return result;

  const auto entries = baseDir.entryInfoList(QStringList(), QDir::AllEntries | QDir::NoDotAndDotDot);
  double progress = 0;
  const auto progressIncrement = progressRange / static_cast<double>(entries.count());
  auto lastIntProgress = 0;
  for (const auto &entry : entries) {
    if (future.isCanceled())
      return result;

    const auto entryName = Utils::FilePath::fromString(entry.absoluteFilePath());
    if (!Utils::contains(versionControls, [&entryName](const Core::IVersionControl *vc) {
      return vc->isVcsFileOrDirectory(entryName);
    })) {
      if (entry.isDir())
        result.append(scanForFilesRecursively(future, progress, progressIncrement, entryName, factory, visited, versionControls));
      else if (const auto node = factory(entryName))
        result.append(node);
    }
    progress += progressIncrement;
    const int intProgress = std::min(static_cast<int>(progressStart + progress), future.progressMaximum());
    if (lastIntProgress < intProgress) {
      future.setProgressValue(intProgress);
      lastIntProgress = intProgress;
    }
  }
  future.setProgressValue(std::min(static_cast<int>(progressStart + progressRange), future.progressMaximum()));
  return result;
}
} // namespace Internal

template <typename Result>
auto scanForFiles(QFutureInterface<Result> &future, const Utils::FilePath &directory, const std::function<FileNode *(const Utils::FilePath &)> factory) -> QList<FileNode*>
{
  QSet<QString> visited;
  future.setProgressRange(0, 1000000);
  return Internal::scanForFilesRecursively(future, 0.0, 1000000.0, directory, factory, visited, Core::VcsManager::versionControls());
}

} // namespace ProjectExplorer
