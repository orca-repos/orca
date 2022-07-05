// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/fileutils.h>
#include <utils/id.h>
#include <utils/optional.h>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QIcon;
QT_END_NAMESPACE

namespace Core {

class IEditor;
class IDocument;

class CORE_EXPORT DocumentModel {
public:
  static auto init() -> void;
  static auto destroy() -> void;
  static auto lockedIcon() -> QIcon;
  static auto model() -> QAbstractItemModel*;

  struct CORE_EXPORT Entry {
    Entry();
    ~Entry();

    auto fileName() const -> Utils::FilePath;
    auto displayName() const -> QString;
    auto plainDisplayName() const -> QString;
    auto uniqueDisplayName() const -> QString;
    auto id() const -> Utils::Id;

    IDocument *document;
    // When an entry is suspended, it means that it is not in memory,
    // and there is no IEditor for it and only a dummy IDocument.
    // This is typically the case for files that have not been opened yet,
    // but can also happen later after they have been opened.
    // The related setting for this is found in:
    // Edit > Preferences > Environment > System > Auto-suspend unmodified files
    bool isSuspended;
    // The entry has been pinned, which means that it should stick to
    // the top of any lists of open files, and that any actions that close
    // files in bulk should not close this one.
    bool pinned;

  private:
    Q_DISABLE_COPY_MOVE(Entry)
  };

  static auto entryAtRow(int row) -> Entry*;
  static auto rowOfDocument(IDocument *document) -> Utils::optional<int>;
  static auto entryCount() -> int;
  static auto entries() -> QList<Entry*>;
  static auto indexOfDocument(IDocument *document) -> Utils::optional<int>;
  static auto indexOfFilePath(const Utils::FilePath &file_path) -> Utils::optional<int>;
  static auto entryForDocument(IDocument *document) -> Entry*;
  static auto entryForFilePath(const Utils::FilePath &file_path) -> Entry*;
  static auto openedDocuments() -> QList<IDocument*>;
  static auto documentForFilePath(const Utils::FilePath &file_path) -> IDocument*;
  static auto editorsForFilePath(const Utils::FilePath &file_path) -> QList<IEditor*>;
  static auto editorsForDocument(IDocument *document) -> QList<IEditor*>;
  static auto editorsForDocuments(const QList<IDocument*> &documents) -> QList<IEditor*>;
  static auto editorsForOpenedDocuments() -> QList<IEditor*>;

private:
  DocumentModel();
};

} // namespace Core

Q_DECLARE_METATYPE(Core::DocumentModel::Entry *)
