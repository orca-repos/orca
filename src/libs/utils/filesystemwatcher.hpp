// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QObject>

namespace Utils {
class FileSystemWatcherPrivate;

// Documentation inside.
class ORCA_UTILS_EXPORT FileSystemWatcher : public QObject {
  Q_OBJECT

public:
  enum WatchMode {
    WatchModifiedDate,
    WatchAllChanges
  };

  explicit FileSystemWatcher(QObject *parent = nullptr);
  explicit FileSystemWatcher(int id, QObject *parent = nullptr);
  ~FileSystemWatcher() override;

  auto addFile(const QString &file, WatchMode wm) -> void;
  auto addFiles(const QStringList &files, WatchMode wm) -> void;
  auto removeFile(const QString &file) -> void;
  auto removeFiles(const QStringList &files) -> void;
  auto clear() -> void;
  auto watchesFile(const QString &file) const -> bool;
  auto files() const -> QStringList;
  auto addDirectory(const QString &file, WatchMode wm) -> void;
  auto addDirectories(const QStringList &files, WatchMode wm) -> void;
  auto removeDirectory(const QString &file) -> void;
  auto removeDirectories(const QStringList &files) -> void;
  auto watchesDirectory(const QString &file) const -> bool;
  auto directories() const -> QStringList;

signals:
  auto fileChanged(const QString &path) -> void;
  auto directoryChanged(const QString &path) -> void;

private:
  auto init() -> void;
  auto slotFileChanged(const QString &path) -> void;
  auto slotDirectoryChanged(const QString &path) -> void;

  FileSystemWatcherPrivate *d;
};

} // namespace Utils
