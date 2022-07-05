// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/fileutils.h>
#include <utils/id.h>

#include <QObject>
#include <QPair>

namespace Utils {
class FilePath;
}

namespace Core {

class IDocument;

namespace Internal {
class DocumentManagerPrivate;
class MainWindow;
}

class CORE_EXPORT DocumentManager : public QObject {
  Q_OBJECT

public:
  enum ResolveMode {
    ResolveLinks,
    KeepLinks
  };

  using recent_file = QPair<Utils::FilePath, Utils::Id>;

  static auto instance() -> DocumentManager*;

  // file pool to monitor
  static auto addDocuments(const QList<IDocument*> &documents, bool add_watcher = true) -> void;
  static auto addDocument(IDocument *document, bool add_watcher = true) -> void;
  static auto removeDocument(IDocument *document) -> bool;
  static auto modifiedDocuments() -> QList<IDocument*>;
  static auto renamedFile(const Utils::FilePath &from, const Utils::FilePath &to) -> void;
  static auto expectFileChange(const Utils::FilePath &file_path) -> void;
  static auto unexpectFileChange(const Utils::FilePath &file_path) -> void;

  // recent files
  static auto addToRecentFiles(const Utils::FilePath &file_path, Utils::Id editor_id = {}) -> void;
  static auto clearRecentFiles() -> Q_SLOT void;
  static auto recentFiles() -> QList<recent_file>;
  static auto saveSettings() -> void;
  // helper functions
  static auto filePathKey(const Utils::FilePath &file_path, ResolveMode resolve_mode) -> Utils::FilePath;
  static auto saveDocument(IDocument *document, const Utils::FilePath &file_path = Utils::FilePath(), bool *is_read_only = nullptr) -> bool;
  static auto allDocumentFactoryFiltersString(QString *all_files_filter) -> QString;
  static auto getOpenFileNames(const QString &filters, const Utils::FilePath &path_in = {}, QString *selected_filter = nullptr) -> Utils::FilePaths;
  static auto getSaveFileName(const QString &title, const Utils::FilePath &path_in, const QString &filter = {}, QString *selected_filter = nullptr) -> Utils::FilePath;
  static auto getSaveFileNameWithExtension(const QString &title, const Utils::FilePath &path_in, const QString &filter) -> Utils::FilePath;
  static auto getSaveAsFileName(const IDocument *document) -> Utils::FilePath;
  static auto saveAllModifiedDocumentsSilently(bool *canceled = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto saveModifiedDocumentsSilently(const QList<IDocument*> &documents, bool *canceled = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto saveModifiedDocumentSilently(IDocument *document, bool *canceled = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto saveAllModifiedDocuments(const QString &message = QString(), bool *canceled = nullptr, const QString &always_save_message = QString(), bool *always_save = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto saveModifiedDocuments(const QList<IDocument*> &documents, const QString &message = QString(), bool *canceled = nullptr, const QString &always_save_message = QString(), bool *alwaysSave = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto saveModifiedDocument(IDocument *document, const QString &message = QString(), bool *canceled = nullptr, const QString &always_save_message = QString(), bool *always_save = nullptr, QList<IDocument*> *failed_to_close = nullptr) -> bool;
  static auto showFilePropertiesDialog(const Utils::FilePath &file_path) -> void;
  static auto fileDialogLastVisitedDirectory() -> Utils::FilePath;
  static auto setFileDialogLastVisitedDirectory(const Utils::FilePath &) -> void;
  static auto fileDialogInitialDirectory() -> Utils::FilePath;
  static auto defaultLocationForNewFiles() -> Utils::FilePath;
  static auto setDefaultLocationForNewFiles(const Utils::FilePath &location) -> void;
  static auto useProjectsDirectory() -> bool;
  static auto setUseProjectsDirectory(bool) -> void;
  static auto projectsDirectory() -> Utils::FilePath;
  static auto setProjectsDirectory(const Utils::FilePath &directory) -> void;

  /* Used to notify e.g. the code model to update the given files. Does *not*
     lead to any editors to reload or any other editor manager actions. */
  static auto notifyFilesChangedInternally(const Utils::FilePaths &file_paths) -> void;
  static auto setFileDialogFilter(const QString &filter) -> void;
  static auto fileDialogFilter(QString *selected_filter = nullptr) -> QString;

signals:
  /* Used to notify e.g. the code model to update the given files. Does *not*
     lead to any editors to reload or any other editor manager actions. */
  auto filesChangedInternally(const Utils::FilePaths &file_paths) -> void;
  /// emitted if all documents changed their name e.g. due to the file changing on disk
  auto allDocumentsRenamed(const Utils::FilePath &from, const Utils::FilePath &to) -> void;
  /// emitted if one document changed its name e.g. due to save as
  auto documentRenamed(IDocument *document, const Utils::FilePath &from, const Utils::FilePath &to) -> void;
  auto projectsDirectoryChanged(const Utils::FilePath &directory) -> void;
  auto filesChangedExternally(const QSet<Utils::FilePath> &file_paths) -> void;

private:
  explicit DocumentManager(QObject *parent);
  ~DocumentManager() override;

  auto documentDestroyed(QObject *obj) -> void;
  auto checkForNewFileName() const -> void;
  auto checkForReload() -> void;
  auto changedFile(const QString &file) -> void;
  auto filePathChanged(const Utils::FilePath &old_name, const Utils::FilePath &new_name) const -> void;
  auto updateSaveAll() -> void;
  static auto registerSaveAllAction() -> void;

  friend class Internal::MainWindow;
  friend class Internal::DocumentManagerPrivate;
};

class CORE_EXPORT FileChangeBlocker {
public:
  explicit FileChangeBlocker(const Utils::FilePath &file_path);
  ~FileChangeBlocker();

private:
  const Utils::FilePath m_file_path;
  Q_DISABLE_COPY(FileChangeBlocker)
};

} // namespace Core

Q_DECLARE_METATYPE(Core::DocumentManager::recent_file)
