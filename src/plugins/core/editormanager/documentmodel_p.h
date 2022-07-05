// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "documentmodel.h"
#include "ieditor.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QMap>

namespace Core {
namespace Internal {

class DocumentModelPrivate final : public QAbstractItemModel {
  Q_OBJECT

public:
  ~DocumentModelPrivate() override;

  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto parent(const QModelIndex &/*index*/) const -> QModelIndex override { return {}; }
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto supportedDragActions() const -> Qt::DropActions override;
  auto mimeTypes() const -> QStringList override;
  auto addEntry(DocumentModel::Entry *entry) -> void;
  auto removeDocument(int idx) -> void;
  auto indexOfFilePath(const Utils::FilePath &file_path) const -> Utils::optional<int>;
  auto indexOfDocument(IDocument *document) const -> Utils::optional<int>;
  auto disambiguateDisplayNames(const DocumentModel::Entry *entry) -> bool;
  static auto setPinned(DocumentModel::Entry *entry, bool pinned) -> void;
  static auto lockedIcon() -> QIcon;
  static auto pinnedIcon() -> QIcon;
  static auto addEditor(IEditor *editor, bool *is_new_document) -> void;
  static auto addSuspendedDocument(const Utils::FilePath &file_path, const QString &display_name, Utils::Id id) -> DocumentModel::Entry*;
  static auto firstSuspendedEntry() -> DocumentModel::Entry*;
  static auto removeEditor(IEditor *editor) -> DocumentModel::Entry*;
  static auto removeEntry(DocumentModel::Entry *entry) -> void;

  enum class pinned_file_removal_policy {
    do_not_remove_pinned_files,
    remove_pinned_files
  };

  static auto removeAllSuspendedEntries(pinned_file_removal_policy pinned_file_removal_policy = pinned_file_removal_policy::remove_pinned_files) -> void;
  auto itemChanged(IDocument *document) -> void;

  class DynamicEntry {
  public:
    explicit DynamicEntry(DocumentModel::Entry *e);

    auto operator->() const -> DocumentModel::Entry*;
    auto disambiguate() -> void;
    auto setNumberedName(int number) const -> void;

    DocumentModel::Entry* entry;
    int path_components;
  };

  QList<DocumentModel::Entry*> m_entries;
  QMap<IDocument*, QList<IEditor*>> m_editors;
  QHash<Utils::FilePath, DocumentModel::Entry*> m_entry_by_fixed_path;
};

} // Internal
} // Core
