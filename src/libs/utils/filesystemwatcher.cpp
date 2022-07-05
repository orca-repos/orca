// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filesystemwatcher.h"
#include "globalfilechangeblocker.h"

#include <QDebug>
#include <QDir>
#include <QFileSystemWatcher>
#include <QDateTime>

enum { debug = 0 };

// Returns upper limit of file handles that can be opened by this process at
// once. (which is limited on MacOS, exceeding it will probably result in
// crashes).
static auto getFileLimit() -> quint64
{
#ifdef Q_OS_MAC
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    return rl.rlim_cur; // quint64
#else
    return 0xFFFFFFFF;
#endif
}

/*!
    \class Utils::FileSystemWatcher
    \brief The FileSystemWatcher class is a file watcher that internally uses
           a centralized QFileSystemWatcher
           and enforces limits on Mac OS.

    \section1 Design Considerations

    Constructing/Destructing a QFileSystemWatcher is expensive. This can be
    worked around by using a centralized watcher.

    \note It is (still) possible to create several instances of a
    QFileSystemWatcher by passing an (arbitrary) integer id != 0 to the
    constructor. This allows separating watchers that
    easily exceed operating system limits from others (see below).

    \section1 Mac OS Specifics

    There is a hard limit on the number of file handles that can be open at
    one point per process on Mac OS X (e.g. it is 2560 on Mac OS X Snow Leopard
    Server, as shown by \c{ulimit -a}). Opening one or several \c .qmlproject's
    with a large number of directories to watch easily exceeds this. The
    results are crashes later on, e.g. when threads cannot be created any more.

    This class implements a heuristic that the file system watcher used for
    \c .qmlproject files never uses more than half the number of available
    file handles. It also increases the number from \c rlim_cur to \c rlim_max
    - the old code in main.cpp failed, see last section in

    \l{http://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man2/setrlimit.2.html}

    for details.
*/

namespace Utils {

// Centralized file watcher static data per integer id.
class FileSystemWatcherStaticData {
public:
  FileSystemWatcherStaticData() : maxFileOpen(getFileLimit()) {}

  quint64 maxFileOpen;
  int m_objectCount = 0;
  QHash<QString, int> m_fileCount;
  QHash<QString, int> m_directoryCount;
  QFileSystemWatcher *m_watcher = nullptr;
};

using FileSystemWatcherStaticDataMap = QMap<int, FileSystemWatcherStaticData>;

Q_GLOBAL_STATIC(FileSystemWatcherStaticDataMap, fileSystemWatcherStaticDataMap)

class WatchEntry {
public:
  using WatchMode = FileSystemWatcher::WatchMode;

  explicit WatchEntry(const QString &file, WatchMode wm) : watchMode(wm), modifiedTime(QFileInfo(file).lastModified()) {}

  WatchEntry() = default;

  auto trigger(const QString &fileName) -> bool;

  WatchMode watchMode = FileSystemWatcher::WatchAllChanges;
  QDateTime modifiedTime;
};

// Check if watch should trigger on signal considering watchmode.
auto WatchEntry::trigger(const QString &fileName) -> bool
{
  if (watchMode == FileSystemWatcher::WatchAllChanges)
    return true;
  // Modified changed?
  const QFileInfo fi(fileName);
  const QDateTime newModifiedTime = fi.exists() ? fi.lastModified() : QDateTime();
  if (newModifiedTime != modifiedTime) {
    modifiedTime = newModifiedTime;
    return true;
  }
  return false;
}

using WatchEntryMap = QHash<QString, WatchEntry>;
using WatchEntryMapIterator = WatchEntryMap::iterator;

class FileSystemWatcherPrivate {
public:
  explicit FileSystemWatcherPrivate(FileSystemWatcher *q, int id) : m_id(id), q(q)
  {
    QObject::connect(Utils::GlobalFileChangeBlocker::instance(), &Utils::GlobalFileChangeBlocker::stateChanged, q, [this](bool blocked) { autoReloadPostponed(blocked); });
  }

  WatchEntryMap m_files;
  WatchEntryMap m_directories;

  QSet<QString> m_postponedFiles;
  QSet<QString> m_postponedDirectories;

  auto checkLimit() const -> bool;
  auto fileChanged(const QString &path) -> void;
  auto directoryChanged(const QString &path) -> void;

  const int m_id;
  FileSystemWatcherStaticData *m_staticData = nullptr;

private:
  auto autoReloadPostponed(bool postponed) -> void;
  bool m_postponed = false;
  FileSystemWatcher *q;
};

auto FileSystemWatcherPrivate::checkLimit() const -> bool
{
  // We are potentially watching a _lot_ of directories. This might crash
  // orca when we hit the upper limit.
  // Heuristic is therefore: Do not use more than half of the file handles
  // available in THIS watcher.
  return quint64(m_directories.size() + m_files.size()) < (m_staticData->maxFileOpen / 2);
}

auto FileSystemWatcherPrivate::fileChanged(const QString &path) -> void
{
  if (m_postponed)
    m_postponedFiles.insert(path);
  else emit q->fileChanged(path);
}

auto FileSystemWatcherPrivate::directoryChanged(const QString &path) -> void
{
  if (m_postponed)
    m_postponedDirectories.insert(path);
  else emit q->directoryChanged(path);
}

auto FileSystemWatcherPrivate::autoReloadPostponed(bool postponed) -> void
{
  if (m_postponed == postponed)
    return;
  m_postponed = postponed;
  if (!postponed) {
    for (const QString &file : qAsConst(m_postponedFiles)) emit q->fileChanged(file);
    m_postponedFiles.clear();
    for (const QString &directory : qAsConst(m_postponedDirectories)) emit q->directoryChanged(directory);
    m_postponedDirectories.clear();
  }
}

/*!
    Adds directories to watcher 0.
*/

FileSystemWatcher::FileSystemWatcher(QObject *parent) : QObject(parent), d(new FileSystemWatcherPrivate(this, 0))
{
  init();
}

/*!
    Adds directories to a watcher with the specified \a id.
*/

FileSystemWatcher::FileSystemWatcher(int id, QObject *parent) : QObject(parent), d(new FileSystemWatcherPrivate(this, id))
{
  init();
}

auto FileSystemWatcher::init() -> void
{
  // Check for id in map/
  FileSystemWatcherStaticDataMap &map = *fileSystemWatcherStaticDataMap();
  FileSystemWatcherStaticDataMap::iterator it = map.find(d->m_id);
  if (it == map.end())
    it = map.insert(d->m_id, FileSystemWatcherStaticData());
  d->m_staticData = &it.value();

  if (!d->m_staticData->m_watcher) {
    d->m_staticData->m_watcher = new QFileSystemWatcher();
    if (debug)
      qDebug() << this << "Created watcher for id " << d->m_id;
  }
  ++(d->m_staticData->m_objectCount);
  connect(d->m_staticData->m_watcher, &QFileSystemWatcher::fileChanged, this, &FileSystemWatcher::slotFileChanged);
  connect(d->m_staticData->m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileSystemWatcher::slotDirectoryChanged);
}

FileSystemWatcher::~FileSystemWatcher()
{
  clear();

  if (!fileSystemWatcherStaticDataMap.isDestroyed() && --(d->m_staticData->m_objectCount) == 0) {
    delete d->m_staticData->m_watcher;
    d->m_staticData->m_watcher = nullptr;
    d->m_staticData->m_fileCount.clear();
    d->m_staticData->m_directoryCount.clear();
    if (debug)
      qDebug() << this << "Deleted watcher" << d->m_id;
  }
  delete d;
}

auto FileSystemWatcher::watchesFile(const QString &file) const -> bool
{
  return d->m_files.contains(file);
}

auto FileSystemWatcher::addFile(const QString &file, WatchMode wm) -> void
{
  addFiles(QStringList(file), wm);
}

auto FileSystemWatcher::addFiles(const QStringList &files, WatchMode wm) -> void
{
  if (debug)
    qDebug() << this << d->m_id << "addFiles mode=" << wm << files << " limit currently: " << (d->m_files.size() + d->m_directories.size()) << " of " << d->m_staticData->maxFileOpen;
  QStringList toAdd;
  for (const QString &file : files) {
    if (watchesFile(file)) {
      qWarning("FileSystemWatcher: File %s is already being watched", qPrintable(file));
      continue;
    }

    if (!d->checkLimit()) {
      qWarning("File %s is not watched: Too many file handles are already open (max is %u).", qPrintable(file), unsigned(d->m_staticData->maxFileOpen));
      break;
    }

    d->m_files.insert(file, WatchEntry(file, wm));

    const int count = ++d->m_staticData->m_fileCount[file];
    Q_ASSERT(count > 0);

    if (count == 1)
      toAdd << file;

    const QString directory = QFileInfo(file).path();
    const int dirCount = ++d->m_staticData->m_directoryCount[directory];
    Q_ASSERT(dirCount > 0);

    if (dirCount == 1)
      toAdd << directory;
  }

  if (!toAdd.isEmpty())
    d->m_staticData->m_watcher->addPaths(toAdd);
}

auto FileSystemWatcher::removeFile(const QString &file) -> void
{
  removeFiles(QStringList(file));
}

auto FileSystemWatcher::removeFiles(const QStringList &files) -> void
{
  if (debug)
    qDebug() << this << d->m_id << "removeFiles " << files;
  QStringList toRemove;
  for (const QString &file : files) {
    WatchEntryMapIterator it = d->m_files.find(file);
    if (it == d->m_files.end()) {
      qWarning("FileSystemWatcher: File %s is not watched.", qPrintable(file));
      continue;
    }
    d->m_files.erase(it);

    const int count = --(d->m_staticData->m_fileCount[file]);
    Q_ASSERT(count >= 0);

    if (!count)
      toRemove << file;

    const QString directory = QFileInfo(file).path();
    const int dirCount = --d->m_staticData->m_directoryCount[directory];
    Q_ASSERT(dirCount >= 0);

    if (!dirCount)
      toRemove << directory;
  }

  if (!toRemove.isEmpty())
    d->m_staticData->m_watcher->removePaths(toRemove);
}

auto FileSystemWatcher::clear() -> void
{
  if (!d->m_files.isEmpty())
    removeFiles(files());
  if (!d->m_directories.isEmpty())
    removeDirectories(directories());
}

auto FileSystemWatcher::files() const -> QStringList
{
  return d->m_files.keys();
}

auto FileSystemWatcher::watchesDirectory(const QString &directory) const -> bool
{
  return d->m_directories.contains(directory);
}

auto FileSystemWatcher::addDirectory(const QString &directory, WatchMode wm) -> void
{
  addDirectories(QStringList(directory), wm);
}

auto FileSystemWatcher::addDirectories(const QStringList &directories, WatchMode wm) -> void
{
  if (debug)
    qDebug() << this << d->m_id << "addDirectories mode " << wm << directories << " limit currently: " << (d->m_files.size() + d->m_directories.size()) << " of " << d->m_staticData->maxFileOpen;
  QStringList toAdd;
  for (const QString &directory : directories) {
    if (watchesDirectory(directory)) {
      qWarning("FileSystemWatcher: Directory %s is already being watched.", qPrintable(directory));
      continue;
    }

    if (!d->checkLimit()) {
      qWarning("Directory %s is not watched: Too many file handles are already open (max is %u).", qPrintable(directory), unsigned(d->m_staticData->maxFileOpen));
      break;
    }

    d->m_directories.insert(directory, WatchEntry(directory, wm));

    const int count = ++d->m_staticData->m_directoryCount[directory];
    Q_ASSERT(count > 0);

    if (count == 1)
      toAdd << directory;
  }

  if (!toAdd.isEmpty())
    d->m_staticData->m_watcher->addPaths(toAdd);
}

auto FileSystemWatcher::removeDirectory(const QString &directory) -> void
{
  removeDirectories(QStringList(directory));
}

auto FileSystemWatcher::removeDirectories(const QStringList &directories) -> void
{
  if (debug)
    qDebug() << this << d->m_id << "removeDirectories" << directories;

  QStringList toRemove;
  for (const QString &directory : directories) {
    WatchEntryMapIterator it = d->m_directories.find(directory);
    if (it == d->m_directories.end()) {
      qWarning("FileSystemWatcher: Directory %s is not watched.", qPrintable(directory));
      continue;
    }
    d->m_directories.erase(it);

    const int count = --d->m_staticData->m_directoryCount[directory];
    Q_ASSERT(count >= 0);

    if (!count)
      toRemove << directory;
  }
  if (!toRemove.isEmpty())
    d->m_staticData->m_watcher->removePaths(toRemove);
}

auto FileSystemWatcher::directories() const -> QStringList
{
  return d->m_directories.keys();
}

auto FileSystemWatcher::slotFileChanged(const QString &path) -> void
{
  const WatchEntryMapIterator it = d->m_files.find(path);
  if (it != d->m_files.end() && it.value().trigger(path)) {
    if (debug)
      qDebug() << this << "triggers on file " << path << it.value().watchMode << it.value().modifiedTime.toString(Qt::ISODate);
    d->fileChanged(path);
  }
}

auto FileSystemWatcher::slotDirectoryChanged(const QString &path) -> void
{
  const WatchEntryMapIterator it = d->m_directories.find(path);
  if (it != d->m_directories.end() && it.value().trigger(path)) {
    if (debug)
      qDebug() << this << "triggers on dir " << path << it.value().watchMode << it.value().modifiedTime.toString(Qt::ISODate);
    d->directoryChanged(path);
  }

  QStringList toReadd;
  const QDir dir(path);
  for (const QFileInfo &entry : dir.entryInfoList(QDir::Files)) {
    const QString file = entry.filePath();
    if (d->m_files.contains(file))
      toReadd.append(file);
  }

  if (!toReadd.isEmpty()) {
    for (const QString &rejected : d->m_staticData->m_watcher->addPaths(toReadd))
      toReadd.removeOne(rejected);

    // If we've successfully added the file, that means it was deleted and replaced.
    for (const QString &reAdded : qAsConst(toReadd))
      d->fileChanged(reAdded);
  }
}

} // namespace Utils
