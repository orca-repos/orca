// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "documentmanager.hpp"
#include "icore.hpp"
#include "idocument.hpp"
#include "idocumentfactory.hpp"
#include "coreconstants.hpp"

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/diffservice.hpp>
#include <core/dialogs/filepropertiesdialog.hpp>
#include <core/dialogs/readonlyfilesdialog.hpp>
#include <core/dialogs/saveitemsdialog.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/editormanager_p.hpp>
#include <core/editormanager/ieditorfactory.hpp>
#include <core/editormanager/iexternaleditor.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/globalfilechangeblocker.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/optional.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/reloadpromptutils.hpp>
#include <utils/threadutils.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QSettings>
#include <QTimer>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>

static const bool kUseProjectsDirectoryDefault = true;
static Q_LOGGING_CATEGORY(log, "qtc.core.documentmanager", QtWarningMsg)

/*!
  \class Core::DocumentManager
  \inheaderfile coreplugin/documentmanager.h
  \ingroup mainclasses
  \inmodule Orca

  \brief The DocumentManager class manages a set of documents.

  The DocumentManager service monitors a set of IDocument objects.

  This section uses the following terminology:

  \list
    \li A \e file means a collection of data stored on a disk under a name
        (that is, the usual meaning of the term \e file in computing).
    \li A \e document holds content open in Qt Creator. If it corresponds to a
        file, it might differ from it, because it was modified. But a document
        might not correspond to a file at all. For example, diff viewer
        documents or Git blame or log records are created and displayed by
        Qt Creator upon request.
    \li An \a editor provides a view into a document that is actually visible
        to the user and potentially allows editing the document. Multiple
        editors can open views into the same document.
  \endlist

  Plugins should register documents they work with at the document management
  service. The files the IDocument objects point to will be monitored at
  file system level. If a file changes on disk, the status of the IDocument
  object will be adjusted accordingly. On application exit the user will be
  asked to save all modified documents.

  Different IDocument objects in the set can point to the same file in the
  file system. The monitoring for an IDocument can be blocked by
  using the \l Core::FileChangeBlocker class.

  The functions \c expectFileChange() and \c unexpectFileChange() mark a file change
  as expected. On expected file changes all IDocument objects are notified to reload
  themselves.

  The DocumentManager service also provides convenience functions
  for saving documents, such as \l saveModifiedDocuments() and
  \l saveModifiedDocumentsSilently(). They present users with a
  dialog that lists all modified documents and asks them which
  documents should be saved.

  The service also manages the list of recent files to be shown to the user.

  \sa addToRecentFiles(), recentFiles()
 */

static const char settingsGroupC[] = "RecentFiles";
static const char filesKeyC[] = "Files";
static const char editorsKeyC[] = "EditorIds";
static const char directoryGroupC[] = "Directories";
static const char projectDirectoryKeyC[] = "Projects";
static const char useProjectDirectoryKeyC[] = "UseProjectsDirectory";

using namespace Utils;

namespace Core {

static auto readSettings() -> void;
static auto saveModifiedFilesHelper(const QList<IDocument*> &documents, const QString &message, bool *cancelled, bool silently, const QString &always_save_message, bool *always_save, QList<IDocument*> *failed_to_save) -> bool;

namespace Internal {

struct FileStateItem {
  QDateTime modified;
  QFile::Permissions permissions;
};

struct FileState {
  FilePath watched_file_path;
  QMap<IDocument*, FileStateItem> last_updated_state;
  FileStateItem expected;
};

class DocumentManagerPrivate final : public QObject {
  Q_OBJECT
public:
  DocumentManagerPrivate();

  auto fileWatcher() -> QFileSystemWatcher*;
  auto linkWatcher() -> QFileSystemWatcher*;
  auto checkOnNextFocusChange() -> void;
  auto onApplicationFocusChange() -> void;
  auto registerSaveAllAction() -> void;

  QMap<FilePath, FileState> m_states; // filePathKey -> FileState
  QSet<FilePath> m_changed_files;      // watched file paths collected from file watcher notifications
  QList<IDocument*> m_documents_without_watch;
  QMap<IDocument*, FilePaths> m_documents_with_watch; // document -> list of filePathKeys
  QSet<FilePath> m_expected_file_names;               // set of file paths without normalization
  QList<DocumentManager::recent_file> m_recent_files;
  bool m_postpone_auto_reload = false;
  bool m_block_activated = false;
  bool m_check_on_focus_change = false;
  bool m_use_projects_directory = kUseProjectsDirectoryDefault;
  QFileSystemWatcher *m_file_watcher = nullptr; // Delayed creation.
  QFileSystemWatcher *m_link_watcher = nullptr; // Delayed creation (only UNIX/if a link is seen).
  FilePath m_last_visited_directory = FilePath::fromString(QDir::currentPath());
  FilePath m_default_location_for_new_files;
  FilePath m_projects_directory;
  // When we are calling into an IDocument
  // we don't want to receive a changed()
  // signal
  // That makes the code easier
  IDocument *m_blocked_i_document = nullptr;
  QAction *m_save_all_action;
  QString file_dialog_filter_override;
};

static DocumentManager *m_instance;
static DocumentManagerPrivate *d;

auto DocumentManagerPrivate::fileWatcher() -> QFileSystemWatcher*
{
  if (!m_file_watcher) {
    m_file_watcher = new QFileSystemWatcher(m_instance);
    connect(m_file_watcher, &QFileSystemWatcher::fileChanged, m_instance, &DocumentManager::changedFile);
  }

  return m_file_watcher;
}

auto DocumentManagerPrivate::linkWatcher() -> QFileSystemWatcher*
{
  if constexpr (HostOsInfo::isAnyUnixHost()) {
    if (!m_link_watcher) {
      m_link_watcher = new QFileSystemWatcher(m_instance);
      m_link_watcher->setObjectName(QLatin1String("_qt_autotest_force_engine_poller"));
      connect(m_link_watcher, &QFileSystemWatcher::fileChanged, m_instance, &DocumentManager::changedFile);
    }
    return m_link_watcher;
  }

  return fileWatcher();
}

auto DocumentManagerPrivate::checkOnNextFocusChange() -> void
{
  m_check_on_focus_change = true;
}

auto DocumentManagerPrivate::onApplicationFocusChange() -> void
{
  if (!m_check_on_focus_change)
    return;

  m_check_on_focus_change = false;
  m_instance->checkForReload();
}

auto DocumentManagerPrivate::registerSaveAllAction() -> void
{
  const auto mfile = ActionManager::actionContainer(Constants::M_FILE);
  const auto cmd = ActionManager::registerAction(m_save_all_action, Constants::SAVEALL);

  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? QString() : tr("Ctrl+Shift+S")));
  mfile->addAction(cmd, Constants::G_FILE_SAVE);

  m_save_all_action->setEnabled(false);

  connect(m_save_all_action, &QAction::triggered, []() {
    DocumentManager::saveAllModifiedDocumentsSilently();
  });
}

DocumentManagerPrivate::DocumentManagerPrivate() : m_save_all_action(new QAction(tr("Save A&ll"), this))
{
  // we do not want to do too much directly in the focus change event, so queue the connection
  connect(qApp, &QApplication::focusChanged, this, &DocumentManagerPrivate::onApplicationFocusChange, Qt::QueuedConnection);
}

} // namespace Internal

using namespace Internal;

DocumentManager::DocumentManager(QObject *parent) : QObject(parent)
{
  d = new DocumentManagerPrivate;
  m_instance = this;

  connect(GlobalFileChangeBlocker::instance(), &GlobalFileChangeBlocker::stateChanged, this, [](const bool blocked) {
    d->m_postpone_auto_reload = blocked;
    if (!blocked)
      QTimer::singleShot(500, m_instance, &DocumentManager::checkForReload);
  });

  readSettings();

  if (d->m_use_projects_directory)
    setFileDialogLastVisitedDirectory(d->m_projects_directory);
}

DocumentManager::~DocumentManager()
{
  delete d;
}

auto DocumentManager::instance() -> DocumentManager*
{
  return m_instance;
}

/* Only called from addFileInfo(IDocument *). Adds the document & state to various caches/lists,
   but does not actually add a watcher. */
static auto addFileInfo(IDocument *document, const FilePath &file_path, const FilePath &file_path_key) -> void
{
  if (!file_path.isEmpty()) {
    FileStateItem state;
    qCDebug(log) << "adding document for" << file_path << "(" << file_path_key << ")";
    state.modified = file_path.lastModified();
    state.permissions = file_path.permissions();

    // Add state if we don't have already
    if (!d->m_states.contains(file_path_key)) {
      FileState state;
      state.watched_file_path = file_path;
      d->m_states.insert(file_path_key, state);
    }
    d->m_states[file_path_key].last_updated_state.insert(document, state);
  }
  d->m_documents_with_watch[document].append(file_path_key); // inserts a new QStringList if not already there
}

/* Adds the IDocuments' file and possibly it's final link target to both m_states
   (if it's file name is not empty), and the m_filesWithWatch list,
   and adds a file watcher for each if not already done.
   (The added file names are guaranteed to be absolute and cleaned.) */
static auto addFileInfos(const QList<IDocument*> &documents) -> void
{
  QTC_ASSERT(isMainThread(), return);
  FilePaths paths_to_watch;
  FilePaths link_paths_to_watch;

  for (const auto &document : documents) {
    const auto file_path = DocumentManager::filePathKey(document->filePath(), DocumentManager::KeepLinks);
    const auto resolved_file_path = file_path.canonicalPath();
    const auto is_link = file_path != resolved_file_path;

    addFileInfo(document, file_path, file_path);

    if (is_link) {
      addFileInfo(document, resolved_file_path, resolved_file_path);
      if (!file_path.needsDevice()) {
        link_paths_to_watch.append(d->m_states.value(file_path).watched_file_path);
        paths_to_watch.append(d->m_states.value(resolved_file_path).watched_file_path);
      }
    } else if (!file_path.needsDevice()) {
      paths_to_watch.append(d->m_states.value(file_path).watched_file_path);
    }
  }

  // Add or update watcher on file path
  // This is also used to update the watcher in case of saved (==replaced) files or
  // update link targets, even if there are multiple documents registered for it
  if (!paths_to_watch.isEmpty()) {
    qCDebug(log) << "adding full watch for" << paths_to_watch;
    d->fileWatcher()->addPaths(transform(paths_to_watch, &FilePath::toString));
  }

  if (!link_paths_to_watch.isEmpty()) {
    qCDebug(log) << "adding link watch for" << link_paths_to_watch;
    d->linkWatcher()->addPaths(transform(link_paths_to_watch, &FilePath::toString));
  }
}

/*!
    Adds a list of \a documents to the collection. If \a addWatcher is \c true
    (the default), the documents' files are added to a file system watcher that
    notifies the document manager about file changes.
*/
auto DocumentManager::addDocuments(const QList<IDocument*> &documents, const bool add_watcher) -> void
{
  if (!add_watcher) {
    // We keep those in a separate list

    for(auto document: documents) {
      if (document && !d->m_documents_without_watch.contains(document)) {
        connect(document, &QObject::destroyed, m_instance, &DocumentManager::documentDestroyed);
        connect(document, &IDocument::filePathChanged, m_instance, &DocumentManager::filePathChanged);
        connect(document, &IDocument::changed, m_instance, &DocumentManager::updateSaveAll);
        d->m_documents_without_watch.append(document);
      }
    }
    return;
  }

  const auto documents_to_watch = filtered(documents, [](IDocument *document) {
    return document && !d->m_documents_with_watch.contains(document);
  });

  for (const auto document : documents_to_watch) {
    connect(document, &IDocument::changed, m_instance, &DocumentManager::checkForNewFileName);
    connect(document, &QObject::destroyed, m_instance, &DocumentManager::documentDestroyed);
    connect(document, &IDocument::filePathChanged, m_instance, &DocumentManager::filePathChanged);
    connect(document, &IDocument::changed, m_instance, &DocumentManager::updateSaveAll);
  }

  addFileInfos(documents_to_watch);
}

/* Removes all occurrences of the IDocument from m_filesWithWatch and m_states.
   If that results in a file no longer being referenced by any IDocument, this
   also removes the file watcher.
*/
static auto removeFileInfo(IDocument *document) -> void
{
  QTC_ASSERT(isMainThread(), return);

  if (!d->m_documents_with_watch.contains(document))
    return;

  for(const auto &file_path: d->m_documents_with_watch.value(document)) {
    if (!d->m_states.contains(file_path))
      continue;

    qCDebug(log) << "removing document (" << file_path << ")";
    d->m_states[file_path].last_updated_state.remove(document);

    if (d->m_states.value(file_path).last_updated_state.isEmpty()) {
      if (const auto &watched_file_path = d->m_states.value(file_path).watched_file_path; !watched_file_path.needsDevice()) {
        const auto &local_file_path = watched_file_path.path();
        if (d->m_file_watcher && d->m_file_watcher->files().contains(local_file_path)) {
          qCDebug(log) << "removing watch for" << local_file_path;
          d->m_file_watcher->removePath(local_file_path);
        }
        if (d->m_link_watcher && d->m_link_watcher->files().contains(local_file_path)) {
          qCDebug(log) << "removing watch for" << local_file_path;
          d->m_link_watcher->removePath(local_file_path);
        }
      }
      d->m_states.remove(file_path);
    }
  }
  d->m_documents_with_watch.remove(document);
}

/*!
    Tells the document manager that a file has been renamed from \a from to
    \a to on disk from within \QC.

    Needs to be called right after the actual renaming on disk (that is, before
    the file system watcher can report the event during the next event loop run).

    \a from needs to be an absolute file path.
    This will notify all IDocument objects pointing to that file of the rename
    by calling \l IDocument::setFilePath(), and update the cached time and
    permission information to avoid annoying the user with \e {the file has
    been removed} popups.
*/
auto DocumentManager::renamedFile(const FilePath &from, const FilePath &to) -> void
{
  const auto &from_key = filePathKey(from, KeepLinks);

  // gather the list of IDocuments
  QList<IDocument*> documents_to_rename;
  for (auto it = d->m_documents_with_watch.cbegin(), end = d->m_documents_with_watch.cend(); it != end; ++it) {
    if (it.value().contains(from_key))
      documents_to_rename.append(it.key());
  }

  // rename the IDocuments
  for(auto document: documents_to_rename) {
    d->m_blocked_i_document = document;
    removeFileInfo(document);
    document->setFilePath(to);
    addFileInfos({document});
    d->m_blocked_i_document = nullptr;
  }

  emit m_instance->allDocumentsRenamed(from, to);
}

auto DocumentManager::filePathChanged(const FilePath &old_name, const FilePath &new_name) const -> void
{
  const auto doc = qobject_cast<IDocument*>(sender());
  QTC_ASSERT(doc, return);

  if (doc == d->m_blocked_i_document)
    return;

  emit m_instance->documentRenamed(doc, old_name, new_name);
}

auto DocumentManager::updateSaveAll() -> void
{
  d->m_save_all_action->setEnabled(!modifiedDocuments().empty());
}

/*!
    Adds \a document to the collection. If \a addWatcher is \c true
    (the default), the document's file is added to a file system watcher
    that notifies the document manager about file changes.
*/
auto DocumentManager::addDocument(IDocument *document, const bool add_watcher) -> void
{
  addDocuments({document}, add_watcher);
}

auto DocumentManager::documentDestroyed(QObject *obj) -> void
{
  // NOTE: Don't use dynamic_cast. By the time QObject::destroyed() is emitted, IDocument has already been destroyed.
  if (const auto document = static_cast<IDocument*>(obj); !d->m_documents_without_watch.removeOne(document)) // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    removeFileInfo(document);
}

/*!
    Removes \a document from the collection.

    Returns \c true if the document had the \c addWatcher argument to
    addDocument() set.
*/
auto DocumentManager::removeDocument(IDocument *document) -> bool
{
  QTC_ASSERT(document, return false);

  auto add_watcher = false;

  // Special casing unwatched files
  if (!d->m_documents_without_watch.removeOne(document)) {
    add_watcher = true;
    removeFileInfo(document);
    disconnect(document, &IDocument::changed, m_instance, &DocumentManager::checkForNewFileName);
  }

  disconnect(document, &QObject::destroyed, m_instance, &DocumentManager::documentDestroyed);
  disconnect(document, &IDocument::changed, m_instance, &DocumentManager::updateSaveAll);

  return add_watcher;
}

/* Slot reacting on IDocument::changed. We need to check if the signal was sent
   because the document was saved under different name. */
auto DocumentManager::checkForNewFileName() const -> void
{
  auto document = qobject_cast<IDocument*>(sender());

  // We modified the IDocument
  // Trust the other code to also update the m_states map
  if (document == d->m_blocked_i_document)
    return;

  QTC_ASSERT(document, return);
  QTC_ASSERT(d->m_documents_with_watch.contains(document), return);

  // Maybe the name has changed or file has been deleted and created again ...
  // This also updates the state to the on disk state
  removeFileInfo(document);
  addFileInfos({document});
}

/*!
    Returns a guaranteed cleaned absolute file path for \a filePath.
    Resolves symlinks if \a resolveMode is ResolveLinks.
*/
auto DocumentManager::filePathKey(const FilePath &file_path, const ResolveMode resolve_mode) -> FilePath
{
  const auto &result = file_path.absoluteFilePath().cleanPath();

  if (resolve_mode == ResolveLinks)
    return result.canonicalPath();

  return result;
}

/*!
    Returns the list of IDocuments that have been modified.
*/
auto DocumentManager::modifiedDocuments() -> QList<IDocument*>
{
  QList<IDocument*> modified;

  const auto doc_end = d->m_documents_with_watch.keyEnd();
  for (auto doc_it = d->m_documents_with_watch.keyBegin(); doc_it != doc_end; ++doc_it) {
    if (const auto document = *doc_it; document->isModified())
      modified << document;
  }

  for(const auto document: d->m_documents_without_watch) {
    if (document->isModified())
      modified << document;
  }

  return modified;
}

/*!
    Treats any subsequent change to \a filePath as an expected file change.

    \sa unexpectFileChange()
*/
auto DocumentManager::expectFileChange(const FilePath &file_path) -> void
{
  if (file_path.isEmpty())
    return;

  d->m_expected_file_names.insert(file_path);
}

/* only called from unblock and unexpect file change functions */
static auto updateExpectedState(const FilePath &file_path_key) -> void
{
  if (file_path_key.isEmpty())
    return;

  if (d->m_states.contains(file_path_key)) {
    const auto watched = d->m_states.value(file_path_key).watched_file_path;
    d->m_states[file_path_key].expected.modified = watched.lastModified();
    d->m_states[file_path_key].expected.permissions = watched.permissions();
  }
}

/*!
    Considers all changes to \a filePath unexpected again.

    \sa expectFileChange()
*/
auto DocumentManager::unexpectFileChange(const FilePath &file_path) -> void
{
  // We are updating the expected time of the file
  // And in changedFile we'll check if the modification time
  // is the same as the saved one here
  // If so then it's a expected change

  if (file_path.isEmpty())
    return;

  d->m_expected_file_names.remove(file_path);
  const auto clean_abs_file_path = filePathKey(file_path, KeepLinks);
  updateExpectedState(filePathKey(file_path, KeepLinks));

  if (const auto resolved_clean_abs_file_path = clean_abs_file_path.canonicalPath(); clean_abs_file_path != resolved_clean_abs_file_path)
    updateExpectedState(filePathKey(file_path, ResolveLinks));
}

static auto saveModifiedFilesHelper(const QList<IDocument*> &documents, const QString &message, bool *cancelled, bool silently, const QString &always_save_message, bool *always_save, QList<IDocument*> *failed_to_save) -> bool
{
  if (cancelled)
    *cancelled = false;

  QList<IDocument*> not_saved;
  QHash<IDocument*, QString> modified_documents_map;

  for(auto document: documents) {
    if (document && document->isModified() && !document->isTemporary()) {
      auto name = document->filePath().toString();
      if (name.isEmpty())
        name = document->fallbackSaveAsFileName();

      // There can be several IDocuments pointing to the same file
      // Prefer one that is not readonly
      // (even though it *should* not happen that the IDocuments are inconsistent with readonly)
      if (!modified_documents_map.key(name, nullptr) || !document->isFileReadOnly())
        modified_documents_map.insert(document, name);
    }
  }

  if (const auto modified_documents = modified_documents_map.keys(); !modified_documents.isEmpty()) {
    QList<IDocument*> documents_to_save;
    if (silently) {
      documents_to_save = modified_documents;
    } else {
      SaveItemsDialog dia(ICore::dialogParent(), modified_documents);
      if (!message.isEmpty())
        dia.setMessage(message);
      if (!always_save_message.isNull())
        dia.setAlwaysSaveMessage(always_save_message);
      if (dia.exec() != QDialog::Accepted) {
        if (cancelled)
          *cancelled = true;
        if (always_save)
          *always_save = dia.alwaysSaveChecked();
        if (failed_to_save)
          *failed_to_save = modified_documents;
        if (const auto files_to_diff = dia.filesToDiff(); !files_to_diff.isEmpty()) {
          if (const auto diff_service = DiffService::instance())
            diff_service->diffModifiedFiles(files_to_diff);
        }
        return false;
      }

      if (always_save)
        *always_save = dia.alwaysSaveChecked();

      documents_to_save = dia.itemsToSave();
    }

    // Check for files without write permissions.
    QList<IDocument*> ro_documents;

    for(const auto document: documents_to_save) {
      if (document->isFileReadOnly())
        ro_documents << document;
    }

    if (!ro_documents.isEmpty()) {
      ReadOnlyFilesDialog ro_dialog(ro_documents, ICore::dialogParent());
      ro_dialog.setShowFailWarning(true, DocumentManager::tr("Could not save the files.", "error message"));
      if (ro_dialog.exec() == ReadOnlyFilesDialog::RO_Cancel) {
        if (cancelled)
          *cancelled = true;
        if (failed_to_save)
          *failed_to_save = modified_documents;
        return false;
      }
    }

    for(const auto document: documents_to_save) {
      if (!EditorManagerPrivate::saveDocument(document)) {
        if (cancelled)
          *cancelled = true;
        not_saved.append(document);
      }
    }
  }

  if (failed_to_save)
    *failed_to_save = not_saved;

  return not_saved.isEmpty();
}

auto DocumentManager::saveDocument(IDocument *document, const FilePath &file_path, bool *is_read_only) -> bool
{
  auto ret = true;
  const auto &save_path = file_path.isEmpty() ? document->filePath() : file_path;
  expectFileChange(save_path);                      // This only matters to other IDocuments which refer to this file
  const auto add_watcher = removeDocument(document); // So that our own IDocument gets no notification at all

  QString error_string;
  if (!document->save(&error_string, file_path, false)) {
    if (is_read_only) {
      // Check whether the existing file is writable
      if (QFile ofi(save_path.toString()); !ofi.open(QIODevice::ReadWrite) && ofi.open(QIODevice::ReadOnly)) {
        *is_read_only = true;
        goto out;
      }
      *is_read_only = false;
    }
    QMessageBox::critical(ICore::dialogParent(), tr("File Error"), tr("Error while saving file: %1").arg(error_string));
out:
    ret = false;
  }

  addDocument(document, add_watcher);
  unexpectFileChange(save_path);
  m_instance->updateSaveAll();
  return ret;
}

auto DocumentManager::fileDialogFilter(QString *selected_filter) -> QString
{
  if (!d->file_dialog_filter_override.isEmpty()) {
    if (selected_filter)
      *selected_filter = d->file_dialog_filter_override.split(";;").first();
    return d->file_dialog_filter_override;
  }

  return allDocumentFactoryFiltersString(selected_filter);
}

auto DocumentManager::allDocumentFactoryFiltersString(QString *all_files_filter = nullptr) -> QString
{
  QSet<QString> unique_filters;

  for (const auto factory : IEditorFactory::allEditorFactories()) {
    for (const auto &mt : factory->mimeTypes()) {
      if (const auto filter = mimeTypeForName(mt).filterString(); !filter.isEmpty())
        unique_filters.insert(filter);
    }
  }

  for (const auto factory : IDocumentFactory::allDocumentFactories()) {
    for (const auto &mt : factory->mimeTypes()) {
      if (const auto filter = mimeTypeForName(mt).filterString(); !filter.isEmpty())
        unique_filters.insert(filter);
    }
  }

  auto filters = toList(unique_filters);
  filters.sort();
  const auto all_files = allFilesFilterString();

  if (all_files_filter)
    *all_files_filter = all_files;

  filters.prepend(all_files);
  return filters.join(QLatin1String(";;"));
}

auto DocumentManager::getSaveFileName(const QString &title, const FilePath &path_in, const QString &filter, QString *selected_filter) -> FilePath
{
  const auto path = path_in.isEmpty() ? fileDialogInitialDirectory() : path_in;
  FilePath file_path;
  bool repeat;
  do {
    repeat = false;
    file_path = FileUtils::getSaveFilePath(nullptr, title, path, filter, selected_filter);
    if (!file_path.isEmpty()) {
      // If the selected filter is All Files (*) we leave the name exactly as the user
      // specified. Otherwise the suffix must be one available in the selected filter. If
      // the name already ends with such suffix nothing needs to be done. But if not, the
      // first one from the filter is appended.
      if (selected_filter && *selected_filter != allFilesFilterString()) {
        // Mime database creates filter strings like this: Anything here (*.foo *.bar)
        const QRegularExpression reg_exp(QLatin1String(R"(.*\s+\((.*)\)$)"));
        if (auto match_it = reg_exp.globalMatch(*selected_filter); match_it.hasNext()) {
          auto suffix_ok = false;
          const auto match = match_it.next();
          auto caption = match.captured(1);
          caption.remove(QLatin1Char('*'));
          const auto suffixes = caption.split(QLatin1Char(' '));
          for (const auto &suffix : suffixes) {
            if (file_path.endsWith(suffix)) {
              suffix_ok = true;
              break;
            }
          }
          if (!suffix_ok && !suffixes.isEmpty()) {
            file_path = file_path.stringAppended(suffixes.at(0));
            if (file_path.exists()) {
              if (QMessageBox::warning(ICore::dialogParent(), tr("Overwrite?"), tr("An item named \"%1\" already exists at this location. " "Do you want to overwrite it?").arg(file_path.toUserOutput()), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
                repeat = true;
              }
            }
          }
        }
      }
    }
  } while (repeat);

  if (!file_path.isEmpty())
    setFileDialogLastVisitedDirectory(file_path.absolutePath());

  return file_path;
}

auto DocumentManager::getSaveFileNameWithExtension(const QString &title, const FilePath &path_in, const QString &filter) -> FilePath
{
  auto selected = filter;
  return getSaveFileName(title, path_in, filter, &selected);
}

/*!
    Asks the user for a new file name (\uicontrol {Save File As}) for \a document.
*/
auto DocumentManager::getSaveAsFileName(const IDocument *document) -> FilePath
{
  QTC_ASSERT(document, return {});
  const auto filter = allDocumentFactoryFiltersString();
  const auto file_path = document->filePath();
  QString selected_filter;
  auto file_dialog_path = file_path;

  if (!file_path.isEmpty()) {
    selected_filter = mimeTypeForFile(file_path).filterString();
  } else {
    const auto suggested_name = document->fallbackSaveAsFileName();
    if (!suggested_name.isEmpty()) {
      if (const auto types = mimeTypesForFileName(suggested_name); !types.isEmpty())
        selected_filter = types.first().filterString();
    }
    if (const auto default_path = document->fallbackSaveAsPath(); !default_path.isEmpty() && !suggested_name.isEmpty())
      file_dialog_path = default_path / suggested_name;
  }

  if (selected_filter.isEmpty())
    selected_filter = mimeTypeForName(document->mimeType()).filterString();

  return getSaveFileName(tr("Save File As"), file_dialog_path, filter, &selected_filter);
}

/*!
    Silently saves all documents and returns \c true if all modified documents
    are saved successfully.

    This method tries to avoid showing dialogs to the user, but can do so anyway
    (e.g. if a file is not writeable).

    If users canceled any of the dialogs they interacted with, \a canceled
    is set. If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.
*/
auto DocumentManager::saveAllModifiedDocumentsSilently(bool *canceled, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedDocumentsSilently(modifiedDocuments(), canceled, failed_to_close);
}

/*!
    Silently saves \a documents and returns \c true if all of them were saved
    successfully.

    This method tries to avoid showing dialogs to the user, but can do so anyway
    (e.g. if a file is not writeable).

    If users canceled any of the dialogs they interacted with, \a canceled
    is set. If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.
*/
auto DocumentManager::saveModifiedDocumentsSilently(const QList<IDocument*> &documents, bool *canceled, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedFilesHelper(documents, QString(), canceled, true, QString(), nullptr, failed_to_close);
}

/*!
    Silently saves \a document and returns \c true if it was saved successfully.

    This method tries to avoid showing dialogs to the user, but can do so anyway
    (e.g. if a file is not writeable).

    If users canceled any of the dialogs they interacted with, \a canceled
    is set. If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.

*/
auto DocumentManager::saveModifiedDocumentSilently(IDocument *document, bool *canceled, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedDocumentsSilently({document}, canceled, failed_to_close);
}

/*!
    Presents a dialog with all modified documents to users and asks them which
    of these should be saved.

    This method may show additional dialogs to the user, e.g. if a file is
    not writeable.

    The dialog text can be set using \a message. If users canceled any
    of the dialogs they interacted with, \a canceled is set and the
    method returns \c false.

    The \a alwaysSaveMessage shows an additional checkbox in the dialog.
    The state of this checkbox is written into \a alwaysSave if set.

    If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.
*/
auto DocumentManager::saveAllModifiedDocuments(const QString &message, bool *canceled, const QString &always_save_message, bool *always_save, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedDocuments(modifiedDocuments(), message, canceled, always_save_message, always_save, failed_to_close);
}

/*!
    Presents a dialog with \a documents to users and asks them which
    of these should be saved.

    This method may show additional dialogs to the user, e.g. if a file is
    not writeable.

    The dialog text can be set using \a message. If users canceled any
    of the dialogs they interacted with, \a canceled is set and the
    method returns \c false.

    The \a alwaysSaveMessage shows an additional checkbox in the dialog.
    The state of this checkbox is written into \a alwaysSave if set.

    If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.
*/
auto DocumentManager::saveModifiedDocuments(const QList<IDocument*> &documents, const QString &message, bool *canceled, const QString &always_save_message, bool *alwaysSave, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedFilesHelper(documents, message, canceled, false, always_save_message, alwaysSave, failed_to_close);
}

/*!
    Presents a dialog with the \a document to users and asks them whether
    it should be saved.

    This method may show additional dialogs to the user, e.g. if a file is
    not writeable.

    The dialog text can be set using \a message. If users canceled any
    of the dialogs they interacted with, \a canceled is set and the
    method returns \c false.

    The \a alwaysSaveMessage shows an additional checkbox in the dialog.
    The state of this checkbox is written into \a alwaysSave if set.

    If passed to the method, \a failedToClose returns a list of
    documents that could not be saved.
*/
auto DocumentManager::saveModifiedDocument(IDocument *document, const QString &message, bool *canceled, const QString &always_save_message, bool *always_save, QList<IDocument*> *failed_to_close) -> bool
{
  return saveModifiedDocuments({document}, message, canceled, always_save_message, always_save, failed_to_close);
}

auto DocumentManager::showFilePropertiesDialog(const FilePath &file_path) -> void
{
  FilePropertiesDialog properties(file_path);
  properties.exec();
}

/*!
    Asks the user for a set of file names to be opened. The \a filters
    and \a selectedFilter arguments are interpreted like in
    QFileDialog::getOpenFileNames(). \a pathIn specifies a path to open the
    dialog in if that is not overridden by the user's policy.
*/

auto DocumentManager::getOpenFileNames(const QString &filters, const FilePath &path_in, QString *selected_filter) -> FilePaths
{
  const auto path = path_in.isEmpty() ? fileDialogInitialDirectory() : path_in;
  auto files = FileUtils::getOpenFilePaths(nullptr, tr("Open File"), path, filters, selected_filter);

  if (!files.isEmpty())
    setFileDialogLastVisitedDirectory(files.front().absolutePath());

  return files;
}

auto DocumentManager::changedFile(const QString &file_name) -> void
{
  const auto file_path = FilePath::fromString(file_name);
  const auto wasempty = d->m_changed_files.isEmpty();

  if (d->m_states.contains(filePathKey(file_path, KeepLinks)))
    d->m_changed_files.insert(file_path);

  qCDebug(log) << "file change notification for" << file_path;

  if (wasempty && !d->m_changed_files.isEmpty())
    QTimer::singleShot(200, this, &DocumentManager::checkForReload);
}

auto DocumentManager::checkForReload() -> void
{
  if (d->m_postpone_auto_reload || d->m_changed_files.isEmpty())
    return;

  if (QApplication::applicationState() != Qt::ApplicationActive)
    return;

  // If d->m_blockActivated is true, then it means that the event processing of either the
  // file modified dialog, or of loading large files, has delivered a file change event from
  // a watcher *and* the timer triggered. We may never end up here in a nested way, so
  // recheck later at the end of the checkForReload function.
  if (d->m_block_activated)
    return;

  if (QApplication::activeModalWidget()) {
    // We do not want to prompt for modified file if we currently have some modal dialog open.
    // There is no really sensible way to get notified globally if a window closed,
    // so just check on every focus change.
    d->checkOnNextFocusChange();
    return;
  }

  d->m_block_activated = true;

  auto default_behavior = EditorManager::reloadSetting();
  auto previous_reload_answer = ReloadCurrent;
  auto previous_deleted_answer = FileDeletedSave;

  QList<IDocument*> documents_to_close;
  QHash<IDocument*, FilePath> documents_to_save;

  // collect file information
  QMap<FilePath, FileStateItem> current_states;
  QMap<FilePath, IDocument::ChangeType> change_types;
  QSet<IDocument*> changed_i_documents;

  for(const auto &file_path: d->m_changed_files) {
    const auto file_key = filePathKey(file_path, KeepLinks);
    qCDebug(log) << "handling file change for" << file_path << "(" << file_key << ")";
    auto type = IDocument::TypeContents;
    FileStateItem state;

    if (!file_path.exists()) {
      qCDebug(log) << "file was removed";
      type = IDocument::TypeRemoved;
    } else {
      state.modified = file_path.lastModified();
      state.permissions = file_path.permissions();
      qCDebug(log) << "file was modified, time:" << state.modified << "permissions: " << state.permissions;
    }

    current_states.insert(file_key, state);
    change_types.insert(file_key, type);

    for(auto document: d->m_states.value(file_key).last_updated_state.keys())
      changed_i_documents.insert(document);
  }

  // clean up. do this before we may enter the main loop, otherwise we would
  // lose consecutive notifications.
  emit filesChangedExternally(d->m_changed_files);
  d->m_changed_files.clear();

  // collect information about "expected" file names
  // we can't do the "resolving" already in expectFileChange, because
  // if the resolved names are different when unexpectFileChange is called
  // we would end up with never-unexpected file names
  QSet<FilePath> expected_file_keys;
  for(const auto &file_path: d->m_expected_file_names) {
    const auto clean_abs_file_path = filePathKey(file_path, KeepLinks);
    expected_file_keys.insert(filePathKey(file_path, KeepLinks));

    if (const auto resolved_clean_abs_file_path = clean_abs_file_path.canonicalPath(); clean_abs_file_path != resolved_clean_abs_file_path)
      expected_file_keys.insert(filePathKey(file_path, ResolveLinks));
  }

  // handle the IDocuments
  QStringList error_strings;
  QStringList files_to_diff;

  for(auto document: changed_i_documents) {
    auto trigger = IDocument::TriggerInternal;
    optional<IDocument::ChangeType> type;
    auto changed = false;
    // find out the type & behavior from the two possible files
    // behavior is internal if all changes are expected (and none removed)
    // type is "max" of both types (remove > contents > permissions)
    for(const auto &file_key: d->m_documents_with_watch.value(document)) {
      // was the file reported?
      if (!current_states.contains(file_key))
        continue;

      auto current_state = current_states.value(file_key);
      auto expected_state = d->m_states.value(file_key).expected;
      auto last_state = d->m_states.value(file_key).last_updated_state.value(document);

      // did the file actually change?
      if (last_state.modified == current_state.modified && last_state.permissions == current_state.permissions)
        continue;
      changed = true;

      // was it only a permission change?
      if (last_state.modified == current_state.modified)
        continue;

      // was the change unexpected?
      if ((current_state.modified != expected_state.modified || current_state.permissions != expected_state.permissions) && !expected_file_keys.contains(file_key)) {
        trigger = IDocument::TriggerExternal;
      }

      // find out the type
      if (auto file_change = change_types.value(file_key); file_change == IDocument::TypeRemoved)
        type = IDocument::TypeRemoved;
      else if (file_change == IDocument::TypeContents && !type)
        type = IDocument::TypeContents;
    }

    if (!changed) // probably because the change was blocked with (un)blockFileChange
      continue;

    // handle it!
    d->m_blocked_i_document = document;

    // Update file info, also handling if e.g. link target has changed.
    // We need to do that before the file is reloaded, because removing the watcher will
    // loose any pending change events. Loosing change events *before* the file is reloaded
    // doesn't matter, because in that case we then reload the new version of the file already
    // anyhow.
    removeFileInfo(document);
    addFileInfos({document});

    auto success = true;
    QString error_string;
    // we've got some modification
    document->checkPermissions();

    // check if it's contents or permissions:
    if (!type) {
      // Only permission change
      success = true;
      // now we know it's a content change or file was removed
    } else if (default_behavior == IDocument::ReloadUnmodified && type == IDocument::TypeContents && !document->isModified()) {
      // content change, but unmodified (and settings say to reload in this case)
      success = document->reload(&error_string, IDocument::FlagReload, *type);
      // file was removed or it's a content change and the default behavior for
      // unmodified files didn't kick in
    } else if (default_behavior == IDocument::ReloadUnmodified && type == IDocument::TypeRemoved && !document->isModified()) {
      // file removed, but unmodified files should be reloaded
      // so we close the file
      documents_to_close << document;
    } else if (default_behavior == IDocument::IgnoreAll) {
      // content change or removed, but settings say ignore
      success = document->reload(&error_string, IDocument::FlagIgnore, *type);
      // either the default behavior is to always ask,
      // or the ReloadUnmodified default behavior didn't kick in,
      // so do whatever the IDocument wants us to do
    } else {
      // check if IDocument wants us to ask
      if (document->reloadBehavior(trigger, *type) == IDocument::BehaviorSilent) {
        // content change or removed, IDocument wants silent handling
        if (type == IDocument::TypeRemoved)
          documents_to_close << document;
        else
          success = document->reload(&error_string, IDocument::FlagReload, *type);
        // IDocument wants us to ask
      } else if (type == IDocument::TypeContents) {
        // content change, IDocument wants to ask user
        if (previous_reload_answer == ReloadNone || previous_reload_answer == ReloadNoneAndDiff) {
          // answer already given, ignore
          success = document->reload(&error_string, IDocument::FlagIgnore, IDocument::TypeContents);
        } else if (previous_reload_answer == ReloadAll) {
          // answer already given, reload
          success = document->reload(&error_string, IDocument::FlagReload, IDocument::TypeContents);
        } else {
          // Ask about content change
          previous_reload_answer = reloadPrompt(document->filePath(), document->isModified(), DiffService::instance(), ICore::dialogParent());
          switch (previous_reload_answer) {
          case ReloadAll:
          case ReloadCurrent:
            success = document->reload(&error_string, IDocument::FlagReload, IDocument::TypeContents);
            break;
          case ReloadSkipCurrent:
          case ReloadNone:
          case ReloadNoneAndDiff:
            success = document->reload(&error_string, IDocument::FlagIgnore, IDocument::TypeContents);
            break;
          case CloseCurrent:
            documents_to_close << document;
            break;
          }
        }
        if (previous_reload_answer == ReloadNoneAndDiff)
          files_to_diff.append(document->filePath().toString());

        // IDocument wants us to ask, and it's the TypeRemoved case
      } else {
        // Ask about removed file
        auto unhandled = true;
        while (unhandled) {
          if (previous_deleted_answer != FileDeletedCloseAll) {
            previous_deleted_answer = fileDeletedPrompt(document->filePath().toString(), ICore::dialogParent());
          }
          switch (previous_deleted_answer) {
          case FileDeletedSave:
            documents_to_save.insert(document, document->filePath());
            unhandled = false;
            break;
          case FileDeletedSaveAs: {
            if (const auto save_file_name = getSaveAsFileName(document); !save_file_name.isEmpty()) {
              documents_to_save.insert(document, save_file_name);
              unhandled = false;
            }
            break;
          }
          case FileDeletedClose:
          case FileDeletedCloseAll:
            documents_to_close << document;
            unhandled = false;
            break;
          }
        }
      }
    }
    if (!success) {
      if (error_string.isEmpty())
        error_strings << tr("Cannot reload %1").arg(document->filePath().toUserOutput());
      else
        error_strings << error_string;
    }
    d->m_blocked_i_document = nullptr;
  }

  if (!files_to_diff.isEmpty()) {
    if (auto diff_service = DiffService::instance())
      diff_service->diffModifiedFiles(files_to_diff);
  }

  if (!error_strings.isEmpty())
    QMessageBox::critical(ICore::dialogParent(), tr("File Error"), error_strings.join(QLatin1Char('\n')));

  // handle deleted files
  EditorManager::closeDocuments(documents_to_close, false);
  for (auto it = documents_to_save.cbegin(), end = documents_to_save.cend(); it != end; ++it) {
    saveDocument(it.key(), it.value());
    it.key()->checkPermissions();
  }

  d->m_block_activated = false;
  // re-check in case files where modified while the dialog was open
  QMetaObject::invokeMethod(this, &DocumentManager::checkForReload, Qt::QueuedConnection);
}

/*!
    Adds the \a filePath to the list of recent files. Associates the file to
    be reopened with the editor that has the specified \a editorId, if possible.
    \a editorId defaults to the empty ID, which lets \QC figure out
    the best editor itself.
*/
auto DocumentManager::addToRecentFiles(const FilePath &file_path, Id editor_id) -> void
{
  if (file_path.isEmpty())
    return;

  const auto file_key = filePathKey(file_path, KeepLinks);

  Utils::erase(d->m_recent_files, [file_key](const recent_file &file) {
    return file_key == filePathKey(file.first, KeepLinks);
  });

  while (d->m_recent_files.count() >= EditorManagerPrivate::maxRecentFiles())
    d->m_recent_files.removeLast();

  d->m_recent_files.prepend(recent_file(file_path, editor_id));
}

/*!
    Clears the list of recent files. Should only be called by
    the core plugin when the user chooses to clear the list.
*/
auto DocumentManager::clearRecentFiles() -> void
{
  d->m_recent_files.clear();
}

/*!
    Returns the list of recent files.
*/
auto DocumentManager::recentFiles() -> QList<recent_file>
{
  return d->m_recent_files;
}

auto DocumentManager::saveSettings() -> void
{
  QVariantList recent_files;
  QStringList recent_editor_ids;
  for(const auto & [fst, snd]: d->m_recent_files) {
    recent_files.append(fst.toVariant());
    recent_editor_ids.append(snd.toString());
  }

  const auto s = ICore::settings();
  s->beginGroup(settingsGroupC);
  s->setValueWithDefault(filesKeyC, recent_files);
  s->setValueWithDefault(editorsKeyC, recent_editor_ids);
  s->endGroup();
  s->beginGroup(directoryGroupC);
  s->setValueWithDefault(projectDirectoryKeyC, d->m_projects_directory.toString(), PathChooser::homePath().toString());
  s->setValueWithDefault(useProjectDirectoryKeyC, d->m_use_projects_directory, kUseProjectsDirectoryDefault);
  s->endGroup();
}

auto readSettings() -> void
{
  QSettings *s = ICore::settings();
  d->m_recent_files.clear();
  s->beginGroup(QLatin1String(settingsGroupC));
  const auto recent_files = s->value(QLatin1String(filesKeyC)).toList();
  const auto recent_editor_ids = s->value(QLatin1String(editorsKeyC)).toStringList();
  s->endGroup();

  // clean non-existing files
  for (auto i = 0, n = static_cast<int>(recent_files.size()); i < n; ++i) {
    QString editor_id;

    if (i < recent_editor_ids.size()) // guard against old or weird settings
      editor_id = recent_editor_ids.at(i);

    if (const auto &file_path = FilePath::fromVariant(recent_files.at(i)); file_path.exists() && !file_path.isDir())
      d->m_recent_files.append({file_path, Id::fromString(editor_id)});
  }

  s->beginGroup(QLatin1String(directoryGroupC));

  if (const auto settings_project_dir = FilePath::fromString(s->value(QLatin1String(projectDirectoryKeyC), QString()).toString()); !settings_project_dir.isEmpty() && settings_project_dir.isDir())
    d->m_projects_directory = settings_project_dir;
  else
    d->m_projects_directory = PathChooser::homePath();

  d->m_use_projects_directory = s->value(QLatin1String(useProjectDirectoryKeyC), kUseProjectsDirectoryDefault).toBool();
  s->endGroup();
}

/*!

  Returns the initial directory for a new file dialog. If there is a current
  document associated with a file, uses that. Or if there is a default location
  for new files, uses that. Otherwise, uses the last visited directory.

  \sa setFileDialogLastVisitedDirectory(), setDefaultLocationForNewFiles()
*/

auto DocumentManager::fileDialogInitialDirectory() -> FilePath
{
  if (const auto doc = EditorManager::currentDocument(); doc && !doc->isTemporary() && !doc->filePath().isEmpty())
    return doc->filePath().absolutePath();

  if (!d->m_default_location_for_new_files.isEmpty())
    return d->m_default_location_for_new_files;

  return d->m_last_visited_directory;
}

/*!

  Returns the default location for new files.

  \sa fileDialogInitialDirectory()
*/
auto DocumentManager::defaultLocationForNewFiles() -> FilePath
{
  return d->m_default_location_for_new_files;
}

/*!
 Sets the default \a location for new files.
 */
auto DocumentManager::setDefaultLocationForNewFiles(const FilePath &location) -> void
{
  d->m_default_location_for_new_files = location;
}

/*!

  Returns the directory for projects. Defaults to HOME.

  \sa setProjectsDirectory(), setUseProjectsDirectory()
*/

auto DocumentManager::projectsDirectory() -> FilePath
{
  return d->m_projects_directory;
}

/*!

  Sets the \a directory for projects.

  \sa projectsDirectory(), useProjectsDirectory()
*/

auto DocumentManager::setProjectsDirectory(const FilePath &directory) -> void
{
  if (d->m_projects_directory != directory) {
    d->m_projects_directory = directory;
    emit m_instance->projectsDirectoryChanged(d->m_projects_directory);
  }
}

/*!

    Returns whether the directory for projects is to be used or whether the user
    chose to use the current directory.

  \sa setProjectsDirectory(), setUseProjectsDirectory()
*/

auto DocumentManager::useProjectsDirectory() -> bool
{
  return d->m_use_projects_directory;
}

/*!

  Sets whether the directory for projects is to be used to
  \a useProjectsDirectory.

  \sa projectsDirectory(), useProjectsDirectory()
*/

auto DocumentManager::setUseProjectsDirectory(bool useProjectsDirectory) -> void
{
  d->m_use_projects_directory = useProjectsDirectory;
}

/*!

  Returns the last visited directory of a file dialog.

  \sa setFileDialogLastVisitedDirectory(), fileDialogInitialDirectory()

*/

auto DocumentManager::fileDialogLastVisitedDirectory() -> FilePath
{
  return d->m_last_visited_directory;
}

/*!

  Sets the last visited \a directory of a file dialog that will be remembered
  for the next one.

  \sa fileDialogLastVisitedDirectory(), fileDialogInitialDirectory()

  */

auto DocumentManager::setFileDialogLastVisitedDirectory(const FilePath &directory) -> void
{
  d->m_last_visited_directory = directory;
}

auto DocumentManager::notifyFilesChangedInternally(const FilePaths &file_paths) -> void
{
  emit m_instance->filesChangedInternally(file_paths);
}

auto DocumentManager::setFileDialogFilter(const QString &filter) -> void
{
  d->file_dialog_filter_override = filter;
}

auto DocumentManager::registerSaveAllAction() -> void
{
  d->registerSaveAllAction();
}

// -------------- FileChangeBlocker

/*!
    \class Core::FileChangeBlocker
    \inheaderfile coreplugin/documentmanager.h
    \inmodule Orca

    \brief The FileChangeBlocker class blocks all change notifications to all
    IDocument objects that match the given filename.

    Additionally, the class unblocks in the destructor. To also reload the
    IDocument object in the destructor, set modifiedReload() to \c true.
*/

FileChangeBlocker::FileChangeBlocker(const FilePath &file_path) : m_file_path(file_path)
{
  DocumentManager::expectFileChange(file_path);
}

FileChangeBlocker::~FileChangeBlocker()
{
  DocumentManager::unexpectFileChange(m_file_path);
}

} // namespace Core

#include "documentmanager.moc"
