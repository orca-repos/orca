// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-document-interface.hpp"

#include <utils/fileutils.hpp>
#include <utils/infobar.hpp>
#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>

#include <QFile>

/*!
    \class Orca::Plugin::Core::IDocument
    \inheaderfile coreplugin/idocument.h
    \inmodule Orca

    \brief The IDocument class describes a document that can be saved and
    reloaded.

    The class has two use cases.

    \section1 Handling External Modifications

    You can implement IDocument and register instances in DocumentManager to
    let it handle external modifications of a file. When the file specified with
    filePath() has changed externally, the DocumentManager asks the
    corresponding IDocument instance what to do via reloadBehavior(). If that
    returns \c IDocument::BehaviorAsk, the user is asked if the file should be
    reloaded from disk. If the user requests the reload or reloadBehavior()
    returns \c IDocument::BehaviorSilent, the DocumentManager calls reload()
    to initiate a reload of the file from disk.

    Core functions: setFilePath(), reload(), reloadBehavior().

    If the content of the document can change in \QC, diverging from the
    content on disk: isModified(), save(), isSaveAsAllowed(),
    fallbackSaveAsPath(), fallbackSaveAsFileName().

    \section1 Editor Document

    The most common use case for implementing an IDocument subclass is as a
    document for an IEditor implementation. Multiple editor instances can work
    on the same document instance, for example if the document is visible in
    multiple splits simultaneously. So the IDocument subclass should hold all
    data that is independent from the specific IEditor instance, for example
    the content and highlighting information.

    Each IDocument subclass is only required to work with the corresponding
    IEditor subclasses that it was designed to work with.

    An IDocument can either be backed by a file, or solely represent some data
    in memory. Documents backed by a file are automatically added to the
    DocumentManager by the EditorManager.

    Core functions: setId(), isModified(), contents(), setContents().

    If the content of the document is backed by a file: open(), save(),
    setFilePath(), mimeType(), shouldAutoSave(), setSuspendAllowed(), and
    everything from \l{Handling External Modifications}.

    If the content of the document is not backed by a file:
    setPreferredDisplayName(), setTemporary().

    \ingroup mainclasses
*/

/*!
    \enum IDocument::OpenResult

    The OpenResult enum describes whether a file was successfully opened.

    \value Success
           The file was read successfully and can be handled by this document
           type.
    \value ReadError
           The file could not be opened for reading, either because it does not
           exist or because of missing permissions.
    \value CannotHandle
           This document type could not handle the file content.
*/

/*!
    \enum IDocument::ReloadSetting

    \internal
*/

/*!
    \enum IDocument::ChangeTrigger

    The ChangeTrigger enum describes whether a file was changed from \QC
    internally or from the outside.

    \value TriggerInternal
           The file was changed by \QC.
    \value TriggerExternal
           The file was changed from the outside.

    \sa IDocument::reloadBehavior()
*/

/*!
    \enum IDocument::ChangeType

    The ChangeType enum describes the way in which the file changed.

    \value TypeContents
           The contents of the file changed.
    \value TypeRemoved
           The file was removed.

    \sa IDocument::reloadBehavior()
    \sa IDocument::reload()
*/

/*!
    \enum IDocument::ReloadFlag

    The ReloadFlag enum describes if a file should be reloaded from disk.

    \value FlagReload
           The file should be reloaded.
    \value FlagIgnore
           The file should not be reloaded, but the document state should
           reflect the change.

    \sa IDocument::reload()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::changed()

    This signal is emitted when the document's meta data, like file name or
    modified state, changes.

    \sa isModified()
    \sa filePath()
    \sa displayName()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::contentsChanged()

    This signal is emitted when the document's content changes.

    \sa contents()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::mimeTypeChanged()

    This signal is emitted when the document content's MIME type changes.

    \sa mimeType()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::aboutToReload()

    This signal is emitted before the document is reloaded from the backing
    file.

    \sa reload()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::reloadFinished(bool success)

    This signal is emitted after the document is reloaded from the backing
    file, or if reloading failed.

    The success state is passed in \a success.

    \sa reload()
*/

/*!
    \fn Orca::Plugin::Core::IDocument::filePathChanged(const Utils::FilePath &oldName, const Utils::FilePath &newName)

    This signal is emitted after the file path changed from \a oldName to \a
    newName.

    \sa filePath()
*/

using namespace Utils;

namespace Orca::Plugin::Core {

class IDocumentPrivate {
public:
  ~IDocumentPrivate()
  {
    delete info_bar;
  }

  QString mime_type;
  FilePath file_path;
  QString preferred_display_name;
  QString unique_display_name;
  FilePath auto_save_path;
  InfoBar *info_bar = nullptr;
  Id id;
  optional<bool> file_is_read_only;
  bool temporary = false;
  bool has_write_warning = false;
  bool restored = false;
  bool is_suspend_allowed = false;
};

/*!
    Creates an IDocument with \a parent.

    \note Using the \a parent for ownership of the document is generally a
    bad idea if the IDocument is intended for use with IEditor. It is better to
    use shared ownership in that case.
*/
IDocument::IDocument(QObject *parent) : QObject(parent), d(new IDocumentPrivate) {}

/*!
    Destroys the IDocument.
    If there was an auto save file for this document, it is removed.

    \sa shouldAutoSave()
*/
IDocument::~IDocument()
{
  removeAutoSaveFile();
  delete d;
}

/*!
    \fn void IDocument::setId(Utils::Id id)

    Sets the ID for this document type to \a id. This is coupled with the
    corresponding IEditor implementation and the \l{IEditorFactory::id()}{id()}
    of the IEditorFactory. If the IDocument implementation only works with a
    single IEditor type, this is preferably set in the IDocuments's
    constructor.

    \sa id()
*/
auto IDocument::setId(const Id id) const -> void
{
  d->id = id;
}

/*!
    Returns the ID for this document type.

    \sa setId()
*/
auto IDocument::id() const -> Id
{
  QTC_CHECK(d->id.isValid());
  return d->id;
}

/*!
    The open() method is used to load the contents of a file when a document is
    opened in an editor.

    If the document is opened from an auto save file, \a realFilePath is the
    name of the auto save file that should be loaded, and \a filePath is the
    file name of the resulting file. In that case, the contents of the auto
    save file should be loaded, the file name of the IDocument should be set to
    \a filePath, and the document state be set to modified.

    If the editor is opened from a regular file, \a filePath and \a
    filePath are the same.

    Use \a errorString to return an error message if this document cannot
    handle the file contents.

    Returns whether the file was opened and read successfully.

    The default implementation does nothing and returns
    CannotHandle.

    \sa EditorManager::openEditor()
    \sa shouldAutoSave()
    \sa setFilePath()
*/
auto IDocument::open(QString *error_string, const FilePath &file_path, const FilePath &real_file_path) -> OpenResult
{
  Q_UNUSED(error_string)
  Q_UNUSED(file_path)
  Q_UNUSED(real_file_path)

  return OpenResult::CannotHandle;
}

/*!
    Saves the contents of the document to the \a filePath on disk.

    If \a autoSave is \c true, the saving is done for an auto-save, so the
    document should avoid cleanups or other operations that it does for
    user-requested saves.

    Use \a errorString to return an error message if saving failed.

    Returns whether saving was successful.

    The default implementation does nothing and returns \c false.

    \sa shouldAutoSave()
*/
auto IDocument::save(QString *error_string, const FilePath &file_path, const bool auto_save) -> bool
{
  Q_UNUSED(error_string)
  Q_UNUSED(file_path)
  Q_UNUSED(auto_save)

  return false;
}

/*!
    Returns the current contents of the document. The default implementation
    returns an empty QByteArray.

    \sa setContents()
    \sa contentsChanged()
*/
auto IDocument::contents() const -> QByteArray
{
  return {};
}

/*!
    The setContents() method is for example used by
    EditorManager::openEditorWithContents() to set the \a contents of this
    document.

    Returns whether setting the contents was successful.

    The default implementation does nothing and returns false.

    \sa contents()
    \sa EditorManager::openEditorWithContents()
*/
auto IDocument::setContents(const QByteArray &contents) -> bool
{
  Q_UNUSED(contents)
  return false;
}

/*!
    Returns the absolute path of the file that this document refers to. May be
    empty for documents that are not backed by a file.

    \sa setFilePath()
*/
auto IDocument::filePath() const -> const FilePath&
{
  return d->file_path;
}

/*!
    The reloadBehavior() method is used by the DocumentManager to ask what to
    do if the file backing this document has changed on disk.

    The \a trigger specifies if the change was triggered by some operation in
    \QC. The \a type specifies if the file changed permissions or contents, or
    was removed completely.

    Returns whether the user should be asked or the document should be
    reloaded silently.

    The default implementation requests a silent reload if only the permissions
    changed or if the contents have changed but the \a trigger is internal and
    the document is not modified.

    \sa isModified()
*/
auto IDocument::reloadBehavior(const ChangeTrigger state, const ChangeType type) const -> ReloadBehavior
{
  if (type == TypeContents && state == TriggerInternal && !isModified())
    return BehaviorSilent;

  return BehaviorAsk;
}

/*!
    Reloads the document from the backing file when that changed on disk.

    If \a flag is FlagIgnore the file should not actually be loaded, but the
    document should reflect the change in its \l{isModified()}{modified state}.

    The \a type specifies whether only the file permissions changed or if the
    contents of the file changed.

    Use \a errorString to return an error message, if this document cannot
    handle the file contents.

    Returns if the file was reloaded successfully.

    The default implementation does nothing and returns \c true.

    Subclasses should emit aboutToReload() before, and reloadFinished() after
    reloading the file.

    \sa isModified()
    \sa aboutToReload()
    \sa reloadFinished()
    \sa changed()
*/
auto IDocument::reload(QString *error_string, const ReloadFlag flag, const ChangeType type) -> bool
{
  Q_UNUSED(error_string)
  Q_UNUSED(flag)
  Q_UNUSED(type)

  return true;
}

/*!
    Updates the cached information about the read-only status of the backing file.
*/
auto IDocument::checkPermissions() -> void
{
  const auto previous_read_only = d->file_is_read_only.value_or(false);

  if (!filePath().isEmpty()) {
    d->file_is_read_only = !filePath().isWritableFile();
  } else {
    d->file_is_read_only = false;
  }

  if (previous_read_only != *d->file_is_read_only)
    emit changed();
}

/*!
    Returns whether the document should automatically be saved at a user-defined
    interval.

    The default implementation returns \c false.
*/
auto IDocument::shouldAutoSave() const -> bool
{
  return false;
}

/*!
    Returns whether the document has been modified after it was loaded from a
    file.

    The default implementation returns \c false. Re-implementations should emit
    changed() when this property changes.

    \sa changed()
*/
auto IDocument::isModified() const -> bool
{
  return false;
}

/*!
    Returns whether the document may be saved under a different file name.

    The default implementation returns \c false.

    \sa save()
*/
auto IDocument::isSaveAsAllowed() const -> bool
{
  return false;
}

/*!
    Returns whether the document may be suspended.

    The EditorManager can automatically suspend editors and its corresponding
    documents if the document is backed by a file, is not modified, and is not
    temporary. Suspended IEditor and IDocument instances are deleted and
    removed from memory, but are still visually accessible as if the document
    was still opened in \QC.

    The default is \c false.

    \sa setSuspendAllowed()
    \sa isModified()
    \sa isTemporary()
*/
auto IDocument::isSuspendAllowed() const -> bool
{
  return d->is_suspend_allowed;
}

/*!
    Sets whether the document may be suspended to \a value.

    \sa isSuspendAllowed()
*/
auto IDocument::setSuspendAllowed(const bool value) const -> void
{
  d->is_suspend_allowed = value;
}

/*!
    Returns whether the file backing this document is read-only, or \c false if
    the document is not backed by a file.
*/
auto IDocument::isFileReadOnly() const -> bool
{
  if (filePath().isEmpty())
    return false;

  if (!d->file_is_read_only)
    const_cast<IDocument*>(this)->checkPermissions();

  return d->file_is_read_only.value_or(false);
}

/*!
    Returns if the document is temporary, and should for example not be
    considered when saving or restoring the session state, or added to the recent
    files list.

    The default is \c false.

    \sa setTemporary()
*/
auto IDocument::isTemporary() const -> bool
{
  return d->temporary;
}

/*!
    Sets whether the document is \a temporary.

    \sa isTemporary()
*/
auto IDocument::setTemporary(const bool temporary) const -> void
{
  d->temporary = temporary;
}

/*!
    Returns a path to use for the \uicontrol{Save As} file dialog in case the
    document is not backed by a file.

    \sa fallbackSaveAsFileName()
*/
auto IDocument::fallbackSaveAsPath() const -> FilePath
{
  return {};
}

/*!
    Returns a file name to use for the \uicontrol{Save As} file dialog in case
    the document is not backed by a file.

    \sa fallbackSaveAsPath()
*/
auto IDocument::fallbackSaveAsFileName() const -> QString
{
  return {};
}

/*!
    Returns the MIME type of the document content, if applicable.

    Subclasses should set this with setMimeType() after setting or loading
    content.

    The default MIME type is empty.

    \sa setMimeType()
    \sa mimeTypeChanged()
*/
auto IDocument::mimeType() const -> QString
{
  return d->mime_type;
}

/*!
    Sets the MIME type of the document content to \a mimeType.

    \sa mimeType()
*/
auto IDocument::setMimeType(const QString &mime_type) -> void
{
  if (d->mime_type != mime_type) {
    d->mime_type = mime_type;
    emit mimeTypeChanged();
  }
}

/*!
    \internal
*/
auto IDocument::autoSave(QString *error_string, const FilePath &file_path) -> bool
{
  if (!save(error_string, file_path, true))
    return false;

  d->auto_save_path = file_path;
  return true;
}

static constexpr char k_restored_auto_save[] = "RestoredAutoSave";

/*!
    \internal
*/
auto IDocument::setRestoredFrom(const FilePath &path) const -> void
{
  d->auto_save_path = path;
  d->restored = true;
  const InfoBarEntry info(Id(k_restored_auto_save), tr("File was restored from auto-saved copy. " "Select Save to confirm or Revert to Saved to discard changes."));
  infoBar()->addInfo(info);
}

/*!
    \internal
*/
auto IDocument::removeAutoSaveFile() const -> void
{
  if (!d->auto_save_path.isEmpty()) {
    QFile::remove(d->auto_save_path.toString());
    d->auto_save_path.clear();
    if (d->restored) {
      d->restored = false;
      infoBar()->removeInfo(Id(k_restored_auto_save));
    }
  }
}

/*!
    \internal
*/
auto IDocument::hasWriteWarning() const -> bool
{
  return d->has_write_warning;
}

/*!
    \internal
*/
auto IDocument::setWriteWarning(const bool has) const -> void
{
  d->has_write_warning = has;
}

/*!
    Returns the document's Utils::InfoBar, which is shown at the top of an
    editor.
*/
auto IDocument::infoBar() const -> InfoBar*
{
  if (!d->info_bar)
    d->info_bar = new InfoBar;
  return d->info_bar;
}

/*!
    Sets the absolute \a filePath of the file that backs this document. The
    default implementation sets the file name and sends the filePathChanged() and
    changed() signals.

    \sa filePath()
    \sa filePathChanged()
    \sa changed()
*/
auto IDocument::setFilePath(const FilePath &file_path) -> void
{
  if (d->file_path == file_path)
    return;

  const auto old_name = d->file_path;
  d->file_path = file_path;

  emit filePathChanged(old_name, d->file_path);
  emit changed();
}

/*!
    Returns the string to display for this document, for example in the
    \uicontrol{Open Documents} view and the documents drop down.

    The display name is one of the following, in order:

    \list 1
        \li Unique display name set by the document model
        \li Preferred display name set by the owner
        \li Base name of the document's file name
    \endlist

    \sa setPreferredDisplayName()
    \sa filePath()
    \sa changed()
*/
auto IDocument::displayName() const -> QString
{
  return d->unique_display_name.isEmpty() ? plainDisplayName() : d->unique_display_name;
}

/*!
    Sets the preferred display \a name for this document.

    \sa preferredDisplayName()
    \sa displayName()
 */
auto IDocument::setPreferredDisplayName(const QString &name) -> void
{
  if (name == d->preferred_display_name)
    return;

  d->preferred_display_name = name;
  emit changed();
}

/*!
    Returns the preferred display name for this document.

    The default preferred display name is empty, which means that the display
    name is preferably the file name of the file backing this document.

    \sa setPreferredDisplayName()
    \sa displayName()
*/
auto IDocument::preferredDisplayName() const -> QString
{
  return d->preferred_display_name;
}

/*!
    \internal
    Returns displayName without disambiguation.
 */
auto IDocument::plainDisplayName() const -> QString
{
  return d->preferred_display_name.isEmpty() ? d->file_path.fileName() : d->preferred_display_name;
}

/*!
    \internal
    Sets unique display name for the document. Used by the document model.
 */
auto IDocument::setUniqueDisplayName(const QString &name) const -> void
{
  d->unique_display_name = name;
}

/*!
    \internal
*/
auto IDocument::uniqueDisplayName() const -> QString
{
  return d->unique_display_name;
}

} // namespace Orca::Plugin::Core
