// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "projectnodes.hpp"

#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/fileutils.hpp>

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>

#include <functional>

namespace Orca::Plugin::Core {
class IVersionControl;
}

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT TreeScanner : public QObject {
  Q_OBJECT

public:
  struct Result {
    std::shared_ptr<FolderNode> folderNode;
    QList<FileNode*> allFiles;
  };

  using Future = QFuture<Result>;
  using FutureWatcher = QFutureWatcher<Result>;
  using FutureInterface = QFutureInterface<Result>;
  using FileFilter = std::function<bool(const Utils::MimeType &, const Utils::FilePath &)>;
  using FileTypeFactory = std::function<FileType(const Utils::MimeType &, const Utils::FilePath &)>;

  explicit TreeScanner(QObject *parent = nullptr);
  ~TreeScanner() override;

  // Start scanning in given directory
  auto asyncScanForFiles(const Utils::FilePath &directory) -> bool;
  // Setup filter for ignored files
  auto setFilter(FileFilter filter) -> void;
  // Setup factory for file types
  auto setTypeFactory(FileTypeFactory factory) -> void;
  auto future() const -> Future;
  auto isFinished() const -> bool;
  // Takes not-owning result
  auto result() const -> Result;
  // Takes owning of result
  auto release() -> Result;
  // Clear scan results
  auto reset() -> void;

  // Standard filters helpers
  static auto isWellKnownBinary(const Utils::MimeType &mimeType, const Utils::FilePath &fn) -> bool;
  static auto isMimeBinary(const Utils::MimeType &mimeType, const Utils::FilePath &fn) -> bool;

  // Standard file factory
  static auto genericFileType(const Utils::MimeType &mdb, const Utils::FilePath &fn) -> FileType;

signals:
  auto finished() -> void;

private:
  static auto scanForFiles(FutureInterface &fi, const Utils::FilePath &directory, const FileFilter &filter, const FileTypeFactory &factory) -> void;
  
  FileFilter m_filter;
  FileTypeFactory m_factory;
  FutureWatcher m_futureWatcher;
  Future m_scanFuture;
};

} // namespace ProjectExplorer
