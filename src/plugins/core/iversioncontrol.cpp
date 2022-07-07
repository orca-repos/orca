// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "iversioncontrol.hpp"
#include "vcsmanager.hpp"

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QRegularExpression>
#include <QStringList>

/*!
    \class Core::IVersionControl::TopicCache
    \inheaderfile coreplugin/iversioncontrol.h
    \inmodule Orca

    \brief The TopicCache class stores a cache which maps a directory to a topic.

    A VCS topic is typically the current active branch name, but it can also have other
    values (for example the latest tag) when there is no active branch.

    It is displayed:
    \list
    \li In the project tree, next to each root project - corresponding to the project.
    \li In the main window title - corresponding to the current active editor.
    \endlist

    In order to enable topic display, an IVersionControl subclass needs to create
    an instance of the TopicCache subclass with appropriate overrides for its
    pure virtual functions, and pass this instance to IVersionControl's constructor.

    The cache tracks a file in the repository, which is expected to change when the
    topic changes. When the file is modified, the cache is refreshed.
    For example: for Git this file is typically <repository>/.git/HEAD
 */

/*!
    \fn Utils::FilePath Core::IVersionControl::TopicCache::trackFile(const Utils::FilePath &repository)
    Returns the path to the file that invalidates the cache for \a repository when
    the file is modified.

    \fn QString Core::IVersionControl::TopicCache::refreshTopic(const Utils::FilePath &repository)
    Returns the current topic for \a repository.
 */

using namespace Utils;

namespace Core {

auto IVersionControl::vcsOpenText() const -> QString
{
  return tr("Open with VCS (%1)").arg(displayName());
}

auto IVersionControl::vcsMakeWritableText() const -> QString
{
  return {};
}

auto IVersionControl::additionalToolsPath() const -> FilePaths
{
  return {};
}

auto IVersionControl::createInitialCheckoutCommand(const QString &url, const FilePath &base_directory, const QString &local_name, const QStringList &extra_args) -> ShellCommand*
{
  Q_UNUSED(url)
  Q_UNUSED(base_directory)
  Q_UNUSED(local_name)
  Q_UNUSED(extra_args)
  return nullptr;
}

IVersionControl::RepoUrl::RepoUrl(const QString &location)
{
  if (location.isEmpty())
    return;

  // Check for local remotes (refer to the root or relative path)
  // On Windows, local paths typically starts with <drive>:
  if (auto location_is_on_windows_drive = [&location] {
    if constexpr (!HostOsInfo::isWindowsHost())
      if (location.size() < 2)
        return false;
    const auto drive = location.at(0).toLower();
    return drive >= 'a' && drive <= 'z' && location.at(1) == ':';
  }; location.startsWith("file://") || location.startsWith('/') || location.startsWith('.') || location_is_on_windows_drive()) {
    protocol = "file";
    path = QDir::fromNativeSeparators(location.startsWith("file://") ? location.mid(7) : location);
    is_valid = true;
    return;
  }

  // TODO: Why not use QUrl?
  static const QRegularExpression remote_pattern("^(?:(?<protocol>[^:]+)://)?(?:(?<user>[^@]+)@)?(?<host>[^:/]+)" "(?::(?<port>\\d+))?:?(?<path>.*)$");
  const auto match = remote_pattern.match(location);

  if (!match.hasMatch())
    return;

  auto ok = false;
  protocol = match.captured("protocol");
  user_name = match.captured("user");
  host = match.captured("host");
  port = match.captured("port").toUShort(&ok);
  path = match.captured("path");
  is_valid = !host.isEmpty() && (ok || match.captured("port").isEmpty());
}

auto IVersionControl::getRepoUrl(const QString &location) -> RepoUrl
{
  return RepoUrl(location);
}

auto IVersionControl::setTopicCache(TopicCache *topic_cache) -> void
{
  m_topic_cache = topic_cache;
}

auto IVersionControl::vcsTopic(const FilePath &top_level) -> QString
{
  return m_topic_cache ? m_topic_cache->topic(top_level) : QString();
}

IVersionControl::IVersionControl()
{
  VcsManager::addVersionControl(this);
}

IVersionControl::~IVersionControl()
{
  delete m_topic_cache;
}

auto IVersionControl::unmanagedFiles(const FilePaths &file_paths) const -> FilePaths
{
  return filtered(file_paths, [this](const FilePath &fp) {
    return !managesFile(fp.parentDir(), fp.fileName());
  });
}

auto IVersionControl::openSupportMode(const FilePath &file_path) const -> OpenSupportMode
{
  Q_UNUSED(file_path)
  return NoOpen;
}

IVersionControl::TopicCache::~TopicCache() = default;

/*!
   Returns the topic for repository under \a topLevel.

   If the cache for \a topLevel is valid, it will be used. Otherwise it will be refreshed.
 */
auto IVersionControl::TopicCache::topic(const FilePath &top_level) -> QString
{
  QTC_ASSERT(!top_level.isEmpty(), return QString());
  auto & [timeStamp, topic] = m_cache[top_level];
  const auto file = trackFile(top_level);

  if (file.isEmpty())
    return {};

  const auto last_modified = file.lastModified();

  if (last_modified == timeStamp)
    return topic;

  timeStamp = last_modified;
  return topic = refreshTopic(top_level);
}

auto IVersionControl::fillLinkContextMenu(QMenu *, const FilePath &, const QString &) -> void {}

auto IVersionControl::handleLink(const FilePath &working_directory, const QString &reference) -> bool
{
  QTC_ASSERT(!reference.isEmpty(), return false);
  vcsDescribe(working_directory, reference);
  return true;
}

} // namespace Core

#if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)

#include <QFileInfo>

namespace Core {

TestVersionControl::~TestVersionControl()
{
  VcsManager::clearVersionControlCache();
}

void TestVersionControl::setManagedDirectories(const QHash<FilePath, FilePath> &dirs)
{
  m_managedDirs = dirs;
  m_dirCount = 0;
  VcsManager::clearVersionControlCache();
}

void TestVersionControl::setManagedFiles(const QSet<FilePath> &files)
{
  m_managedFiles = files;
  m_fileCount = 0;
  VcsManager::clearVersionControlCache();
}

bool TestVersionControl::managesDirectory(const FilePath &filePath, FilePath *topLevel) const
{
  ++m_dirCount;

  if (m_managedDirs.contains(filePath)) {
    if (topLevel)
      *topLevel = m_managedDirs.value(filePath);
    return true;
  }
  return false;
}

bool TestVersionControl::managesFile(const FilePath &workingDirectory, const QString &fileName) const
{
  ++m_fileCount;

  FilePath full = workingDirectory.pathAppended(fileName);
  if (!managesDirectory(full.parentDir(), nullptr))
    return false;
  return m_managedFiles.contains(full.absoluteFilePath());
}

} // namespace Core
#endif
