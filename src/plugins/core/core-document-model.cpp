// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-document-model.hpp"

#include "core-document-interface.hpp"
#include "core-document-manager.hpp"
#include "core-document-model-private.hpp"
#include "core-editor-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/dropsupport.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QAbstractItemModel>
#include <QIcon>
#include <QMimeData>
#include <QSet>

using namespace Utils;

static Orca::Plugin::Core::DocumentModelPrivate *d;

namespace Orca::Plugin::Core {
namespace {

auto compare(const DocumentModel::Entry *e1, const DocumentModel::Entry *e2) -> bool
{
  // Pinned files should go at the top.
  if (e1->pinned != e2->pinned)
    return e1->pinned;

  const auto cmp = e1->plainDisplayName().localeAwareCompare(e2->plainDisplayName());
  return cmp < 0 || cmp == 0 && e1->fileName() < e2->fileName();
}

// Return a pair of indices. The first is the index that needs to be removed or -1 if no removal
// is necessary. The second is the index to add the entry into, or -1 if no addition is necessary.
// If the entry does not need to be moved, then (-1, -1) will be returned as no action is needed.
auto positionEntry(const QList<DocumentModel::Entry*> &list, DocumentModel::Entry *entry) -> std::pair<int, int>
{
  const int to_remove = list.indexOf(entry);
  const auto to_sort = filtered(list, [entry](const DocumentModel::Entry *e) { return e != entry; });
  const auto begin = std::begin(to_sort);
  const auto end = std::end(to_sort);
  const auto to_insert = static_cast<int>(std::distance(begin, std::lower_bound(begin, end, entry, &compare)));

  if (to_remove == to_insert)
    return std::make_pair(-1, -1);

  return std::make_pair(to_remove, to_insert);
}

} // namespace

DocumentModelPrivate::~DocumentModelPrivate()
{
  qDeleteAll(m_entries);
}

auto DocumentModelPrivate::columnCount(const QModelIndex &parent) const -> int
{
  if (!parent.isValid())
    return 2;
  return 0;
}

auto DocumentModelPrivate::rowCount(const QModelIndex &parent) const -> int
{
  if (!parent.isValid())
    return m_entries.count() + 1/*<no document>*/;
  return 0;
}

auto DocumentModelPrivate::addEntry(DocumentModel::Entry *entry) -> void
{
  const auto file_path = entry->fileName();

  // replace a non-loaded entry (aka 'suspended') if possible
  if (const auto previous_entry = DocumentModel::entryForFilePath(file_path)) {
    if (!entry->isSuspended && previous_entry->isSuspended) {
      previous_entry->isSuspended = false;
      delete previous_entry->document;
      previous_entry->document = entry->document;
      connect(previous_entry->document, &IDocument::changed, this, [this, document = previous_entry->document] { itemChanged(document); });
    }
    delete entry;
    entry = nullptr;
    disambiguateDisplayNames(previous_entry);
    return;
  }

  const auto positions = positionEntry(m_entries, entry);

  // Do not remove anything (new entry), insert somewhere:
  QTC_CHECK(positions.first == -1 && positions.second >= 0);

  const auto row = positions.second + 1/*<no document>*/;

  beginInsertRows(QModelIndex(), row, row);
  m_entries.insert(positions.second, entry);
  disambiguateDisplayNames(entry);

  if (const auto fixed_path = DocumentManager::filePathKey(file_path, DocumentManager::ResolveLinks); !fixed_path.isEmpty())
    m_entry_by_fixed_path[fixed_path] = entry;

  connect(entry->document, &IDocument::changed, this, [this, document = entry->document] {
    itemChanged(document);
  });

  endInsertRows();
}

auto DocumentModelPrivate::disambiguateDisplayNames(const DocumentModel::Entry *entry) -> bool
{
  const auto display_name = entry->plainDisplayName();
  auto min_idx = -1ll, max_idx = -1ll;

  QList<DynamicEntry> dups;
  for (auto i = 0ll, total = m_entries.count(); i < total; ++i) {
    if (const auto e = m_entries.at(i); e == entry || e->plainDisplayName() == display_name) {
      e->document->setUniqueDisplayName(QString());
      dups += DynamicEntry(e);
      max_idx = i;
      if (min_idx < 0)
        min_idx = i;
    }
  }

  const auto dups_count = dups.count();
  if (dups_count == 0)
    return false;

  if (dups_count > 1) {
    auto serial = 0;
    auto count = 0;
    // increase uniqueness unless no dups are left
    forever {
      auto seen_dups = false;
      for (auto i = 0; i < dups_count - 1; ++i) {
        auto &e = dups[i];
        if (const auto my_file_name = e->document->filePath(); e->document->isTemporary() || my_file_name.isEmpty() || count > 10) {
          // path-less entry, append number
          e.setNumberedName(++serial);
          continue;
        }
        for (auto j = i + 1; j < dups_count; ++j) {
          if (auto &e2 = dups[j]; e->displayName().compare(e2->displayName(), HostOsInfo::fileNameCaseSensitivity()) == 0) {
            if (const auto other_file_name = e2->document->filePath(); other_file_name.isEmpty())
              continue;
            seen_dups = true;
            e2.disambiguate();
            if (j > max_idx)
              max_idx = j;
          }
        }
        if (seen_dups) {
          e.disambiguate();
          ++count;
          break;
        }
      }
      if (!seen_dups)
        break;
    }
  }

  emit dataChanged(index(static_cast<int>(min_idx + 1), 0), index(static_cast<int>(max_idx + 1), 0));
  return true;
}

auto DocumentModelPrivate::setPinned(DocumentModel::Entry *entry, const bool pinned) -> void
{
  if (entry->pinned == pinned)
    return;

  entry->pinned = pinned;

  // Ensure that this entry is re-sorted in the list of open documents
  // now that its pinned state has changed.
  d->itemChanged(entry->document);
}

auto DocumentModelPrivate::lockedIcon() -> QIcon
{
  const static auto icon = Icons::LOCKED.icon();
  return icon;
}

auto DocumentModelPrivate::pinnedIcon() -> QIcon
{
  const static auto icon = Icons::PINNED.icon();
  return icon;
}

auto DocumentModelPrivate::indexOfFilePath(const FilePath &file_path) const -> optional<int>
{
  if (file_path.isEmpty())
    return nullopt;

  const auto fixed_path = DocumentManager::filePathKey(file_path, DocumentManager::ResolveLinks);
  const auto index = m_entries.indexOf(m_entry_by_fixed_path.value(fixed_path));

  if (index < 0)
    return nullopt;

  return index;
}

auto DocumentModelPrivate::removeDocument(const int idx) -> void
{
  if (idx < 0)
    return;

  QTC_ASSERT(idx < m_entries.size(), return);
  const auto row = idx + 1/*<no document>*/;
  beginRemoveRows(QModelIndex(), row, row);
  const auto entry = m_entries.takeAt(idx);
  endRemoveRows();

  if (const auto fixed_path = DocumentManager::filePathKey(entry->fileName(), DocumentManager::ResolveLinks); !fixed_path.isEmpty())
    m_entry_by_fixed_path.remove(fixed_path);

  disconnect(entry->document, &IDocument::changed, this, nullptr);
  disambiguateDisplayNames(entry);

  delete entry;
}

auto DocumentModelPrivate::indexOfDocument(IDocument *document) const -> optional<int>
{
  const auto index = indexOf(m_entries, [&document](const DocumentModel::Entry *entry) {
    return entry->document == document;
  });

  if (index < 0)
    return nullopt;

  return index;
}

auto DocumentModelPrivate::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  if (const DocumentModel::Entry *e = DocumentModel::entryAtRow(index.row()); !e || e->fileName().isEmpty())
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  return Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

auto DocumentModelPrivate::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  const auto data = new DropMimeData;

  for (const auto &index : indexes) {
    const DocumentModel::Entry *e = DocumentModel::entryAtRow(index.row());
    if (!e || e->fileName().isEmpty())
      continue;
    data->addFile(e->fileName());
  }

  return data;
}

auto DocumentModelPrivate::index(const int row, const int column, const QModelIndex &parent) const -> QModelIndex
{
  Q_UNUSED(parent)

  if (column < 0 || column > 1 || row < 0 || row >= m_entries.count() + 1/*<no document>*/)
    return {};

  return createIndex(row, column);
}

auto DocumentModelPrivate::supportedDragActions() const -> Qt::DropActions
{
  return Qt::MoveAction;
}

auto DocumentModelPrivate::mimeTypes() const -> QStringList
{
  return DropSupport::mimeTypesForFilePaths();
}

auto DocumentModelPrivate::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (!index.isValid() || index.column() != 0 && role < Qt::UserRole)
    return {};

  const DocumentModel::Entry *entry = DocumentModel::entryAtRow(index.row());

  if (!entry) {
    // <no document> entry
    switch (role) {
    case Qt::DisplayRole:
      return tr("<no document>");
    case Qt::ToolTipRole:
      return tr("No document is selected.");
    default:
      return {};
    }
  }

  switch (role) {
  case Qt::DisplayRole: {
    auto name = entry->displayName();
    if (entry->document->isModified())
      name += QLatin1Char('*');
    return name;
  }
  case Qt::DecorationRole:
    if (entry->document->isFileReadOnly())
      return lockedIcon();
    if (entry->pinned)
      return pinnedIcon();
    return {};
  case Qt::ToolTipRole:
    return entry->fileName().isEmpty() ? entry->displayName() : entry->fileName().toUserOutput();
  default:
    break;
  }

  return {};
}

auto DocumentModelPrivate::itemChanged(IDocument *document) -> void
{
  const auto idx = indexOfDocument(document);

  if (!idx)
    return;

  const auto fixed_path = DocumentManager::filePathKey(document->filePath(), DocumentManager::ResolveLinks);
  const auto entry = m_entries.at(idx.value());
  auto found = false;

  // The entry's fileName might have changed, so find the previous fileName that was associated
  // with it and remove it, then add the new fileName.
  for (auto it = m_entry_by_fixed_path.begin(), end = m_entry_by_fixed_path.end(); it != end; ++it) {
    if (it.value() == entry) {
      found = true;
      if (it.key() != fixed_path) {
        m_entry_by_fixed_path.remove(it.key());
        if (!fixed_path.isEmpty())
          m_entry_by_fixed_path[fixed_path] = entry;
      }
      break;
    }
  }

  if (!found && !fixed_path.isEmpty())
    m_entry_by_fixed_path[fixed_path] = entry;

  if (!disambiguateDisplayNames(m_entries.at(idx.value()))) {
    const auto mindex = index(idx.value() + 1/*<no document>*/, 0);
    emit dataChanged(mindex, mindex);
  }

  // Make sure the entries stay sorted:
  if (const auto positions = positionEntry(m_entries, entry); positions.first >= 0 && positions.second >= 0) {
    // Entry did move: update its position.

    // Account for the <no document> entry.
    static constexpr auto no_document_entry_offset = 1;
    const auto from_index = positions.first + no_document_entry_offset;
    const auto to_index = positions.second + no_document_entry_offset;
    const auto effective_to_index = to_index > from_index ? to_index + 1 : to_index; // Account for the weird requirements of beginMoveRows().

    beginMoveRows(QModelIndex(), from_index, from_index, QModelIndex(), effective_to_index);
    m_entries.move(from_index - 1ll, to_index - 1ll);
    endMoveRows();
  } else {
    // Nothing to remove or add: The entry did not move.
    QTC_CHECK(positions.first == -1 && positions.second == -1);
  }
}

auto DocumentModelPrivate::addEditor(IEditor *editor, bool *is_new_document) -> void
{
  if (!editor)
    return;

  auto &editor_list = d->m_editors[editor->document()];
  const auto is_new = editor_list.isEmpty();

  if (is_new_document)
    *is_new_document = is_new;

  editor_list << editor;

  if (is_new) {
    const auto entry = new DocumentModel::Entry;
    entry->document = editor->document();
    d->addEntry(entry);
  }
}

/*!
    \class Core::DocumentModel
    \inmodule Orca
    \internal
*/

/*!
    \class Core::DocumentModel::Entry
    \inmodule Orca
    \internal
*/

auto DocumentModelPrivate::addSuspendedDocument(const FilePath &file_path, const QString &display_name, const Id id) -> DocumentModel::Entry*
{
  QTC_CHECK(id.isValid());
  const auto entry = new DocumentModel::Entry;

  entry->document = new IDocument;
  entry->document->setFilePath(file_path);

  if (!display_name.isEmpty())
    entry->document->setPreferredDisplayName(display_name);

  entry->document->setId(id);
  entry->isSuspended = true;

  d->addEntry(entry);
  return entry;
}

auto DocumentModelPrivate::firstSuspendedEntry() -> DocumentModel::Entry*
{
  return findOrDefault(d->m_entries, [](const DocumentModel::Entry *entry) { return entry->isSuspended; });
}

/*!
    Removes an editor from the list of open editors for its entry. If the editor is the last
    one, the entry is put into suspended state.
    Returns the affected entry.
*/
auto DocumentModelPrivate::removeEditor(IEditor *editor) -> DocumentModel::Entry*
{
  QTC_ASSERT(editor, return nullptr);

  const auto document = editor->document();

  QTC_ASSERT(d->m_editors.contains(document), return nullptr);

  d->m_editors[document].removeAll(editor);
  const auto entry = DocumentModel::entryForDocument(document);

  QTC_ASSERT(entry, return nullptr);

  if (d->m_editors.value(document).isEmpty()) {
    d->m_editors.remove(document);
    entry->document = new IDocument;
    entry->document->setFilePath(document->filePath());
    entry->document->setPreferredDisplayName(document->preferredDisplayName());
    entry->document->setUniqueDisplayName(document->uniqueDisplayName());
    entry->document->setId(document->id());
    entry->isSuspended = true;
  }

  return entry;
}

auto DocumentModelPrivate::removeEntry(DocumentModel::Entry *entry) -> void
{
  // For non suspended entries, we wouldn't know what to do with the associated editors
  QTC_ASSERT(entry->isSuspended, return);
  const auto index = static_cast<int>(d->m_entries.indexOf(entry));
  d->removeDocument(index);
}

auto DocumentModelPrivate::removeAllSuspendedEntries(const pinned_file_removal_policy pinned_file_removal_policy) -> void
{
  for (auto i = d->m_entries.count() - 1; i >= 0; --i) {
    const DocumentModel::Entry *entry = d->m_entries.at(i);
    if (!entry->isSuspended)
      continue;
    if (pinned_file_removal_policy == pinned_file_removal_policy::do_not_remove_pinned_files && entry->pinned)
      continue;
    const auto row = static_cast<int>(i + 1) /*<no document>*/;
    d->beginRemoveRows(QModelIndex(), row, row);
    delete d->m_entries.takeAt(i);
    d->endRemoveRows();
  }

  QSet<QString> display_names;
  for (const auto entry : qAsConst(d->m_entries)) {
    const auto display_name = entry->plainDisplayName();
    if (display_names.contains(display_name))
      continue;
    display_names.insert(display_name);
    d->disambiguateDisplayNames(entry);
  }
}

DocumentModelPrivate::DynamicEntry::DynamicEntry(DocumentModel::Entry *e) : entry(e), path_components(0) {}

auto DocumentModelPrivate::DynamicEntry::operator->() const -> DocumentModel::Entry*
{
  return entry;
}

auto DocumentModelPrivate::DynamicEntry::disambiguate() -> void
{
  const auto display = entry->fileName().fileNameWithPathComponents(++path_components);
  entry->document->setUniqueDisplayName(display);
}

auto DocumentModelPrivate::DynamicEntry::setNumberedName(const int number) const -> void
{
  entry->document->setUniqueDisplayName(QStringLiteral("%1 (%2)").arg(entry->document->displayName()).arg(number));
}

DocumentModel::Entry::Entry() : document(nullptr), isSuspended(false), pinned(false) {}

DocumentModel::Entry::~Entry()
{
  if (isSuspended)
    delete document;
}

DocumentModel::DocumentModel() = default;

auto DocumentModel::init() -> void
{
  d = new DocumentModelPrivate;
}

auto DocumentModel::destroy() -> void
{
  delete d;
}

auto DocumentModel::lockedIcon() -> QIcon
{
  return DocumentModelPrivate::lockedIcon();
}

auto DocumentModel::model() -> QAbstractItemModel*
{
  return d;
}

auto DocumentModel::Entry::fileName() const -> FilePath
{
  return document->filePath();
}

auto DocumentModel::Entry::displayName() const -> QString
{
  return document->displayName();
}

auto DocumentModel::Entry::plainDisplayName() const -> QString
{
  return document->plainDisplayName();
}

auto DocumentModel::Entry::id() const -> Id
{
  return document->id();
}

auto DocumentModel::editorsForDocument(IDocument *document) -> QList<IEditor*>
{
  return d->m_editors.value(document);
}

auto DocumentModel::editorsForOpenedDocuments() -> QList<IEditor*>
{
  return editorsForDocuments(openedDocuments());
}

auto DocumentModel::editorsForDocuments(const QList<IDocument*> &documents) -> QList<IEditor*>
{
  QList<IEditor*> result;
  foreach(IDocument *document, documents)
    result += d->m_editors.value(document);
  return result;
}

auto DocumentModel::indexOfDocument(IDocument *document) -> optional<int>
{
  return d->indexOfDocument(document);
}

auto DocumentModel::indexOfFilePath(const FilePath &file_path) -> optional<int>
{
  return d->indexOfFilePath(file_path);
}

auto DocumentModel::entryForDocument(IDocument *document) -> Entry*
{
  return findOrDefault(d->m_entries, [&document](const Entry *entry) { return entry->document == document; });
}

auto DocumentModel::entryForFilePath(const FilePath &file_path) -> Entry*
{
  const auto index = d->indexOfFilePath(file_path);

  if (!index)
    return nullptr;

  return d->m_entries.at(*index);
}

auto DocumentModel::openedDocuments() -> QList<IDocument*>
{
  return d->m_editors.keys();
}

auto DocumentModel::documentForFilePath(const FilePath &file_path) -> IDocument*
{
  const auto index = d->indexOfFilePath(file_path);
  if (!index)
    return nullptr;
  return d->m_entries.at(*index)->document;
}

auto DocumentModel::editorsForFilePath(const FilePath &file_path) -> QList<IEditor*>
{
  if (const auto document = documentForFilePath(file_path))
    return editorsForDocument(document);
  return {};
}

auto DocumentModel::entryAtRow(const int row) -> Entry*
{
  const auto entry_index = row - 1/*<no document>*/;
  if (entry_index < 0)
    return nullptr;
  return d->m_entries[entry_index];
}

auto DocumentModel::entryCount() -> int
{
  return static_cast<int>(d->m_entries.count());
}

auto DocumentModel::rowOfDocument(IDocument *document) -> optional<int>
{
  if (!document)
    return 0 /*<no document>*/;

  if (const auto index = indexOfDocument(document))
    return *index + 1/*correction for <no document>*/;

  return nullopt;
}

auto DocumentModel::entries() -> QList<Entry*>
{
  return d->m_entries;
}

} // namespace Orca::Plugin::Core
