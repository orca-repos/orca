// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileinsessionfinder.hpp"

#include "project.hpp"
#include "session.hpp"

#include <utils/fileinprojectfinder.hpp>

#include <QUrl>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class FileInSessionFinder : public QObject {
public:
  FileInSessionFinder();

  auto doFindFile(const FilePath &filePath) -> FilePaths;
  auto invalidateFinder() -> void { m_finderIsUpToDate = false; }

private:
  FileInProjectFinder m_finder;
  bool m_finderIsUpToDate = false;
};

FileInSessionFinder::FileInSessionFinder()
{
  connect(SessionManager::instance(), &SessionManager::projectAdded, this, [this](const Project *p) {
    invalidateFinder();
    connect(p, &Project::fileListChanged, this, &FileInSessionFinder::invalidateFinder);
  });
  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, [this](const Project *p) {
    invalidateFinder();
    p->disconnect(this);
  });
}

auto FileInSessionFinder::doFindFile(const FilePath &filePath) -> FilePaths
{
  if (!m_finderIsUpToDate) {
    m_finder.setProjectDirectory(SessionManager::startupProject() ? SessionManager::startupProject()->projectDirectory() : FilePath());
    FilePaths allFiles;
    for (const Project *const p : SessionManager::projects())
      allFiles << p->files(Project::SourceFiles);
    m_finder.setProjectFiles(allFiles);
    m_finderIsUpToDate = true;
  }
  return m_finder.findFile(QUrl::fromLocalFile(filePath.toString()));
}

} // namespace Internal

auto findFileInSession(const FilePath &filePath) -> FilePaths
{
  static Internal::FileInSessionFinder finder;
  return finder.doFindFile(filePath);
}

} // namespace ProjectExplorer
