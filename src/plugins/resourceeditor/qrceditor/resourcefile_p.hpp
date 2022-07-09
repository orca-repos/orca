// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QIcon>

#include <core/idocument.hpp>
#include <utils/textfileformat.hpp>

namespace ResourceEditor {
namespace Internal {

class File;
struct Prefix;

/*!
    \class Node

    Forms the base class for nodes in a \l ResourceFile tree.
*/
class Node {
protected:
  Node(File *file, Prefix *prefix) : m_file(file), m_prefix(prefix)
  {
    Q_ASSERT(m_prefix);
  }

public:
  auto file() const -> File* { return m_file; }
  auto prefix() const -> Prefix* { return m_prefix; }

private:
  File *m_file;
  Prefix *m_prefix;
};

/*!
    \class File

    Represents a file node in a \l ResourceFile tree.
*/
class File : public Node {
public:
  File(Prefix *prefix, const QString &_name, const QString &_alias = QString());

  auto checkExistence() -> void;
  auto exists() -> bool;
  auto setExists(bool exists) -> void;
  auto operator <(const File &other) const -> bool { return name < other.name; }
  auto operator ==(const File &other) const -> bool { return name == other.name; }
  auto operator !=(const File &other) const -> bool { return name != other.name; }

  QString name;
  QString alias;
  QIcon icon;
  // not used, only loaded and saved
  QString compress;
  QString compressAlgo;
  QString threshold;

private:
  bool m_checked;
  bool m_exists;
};

class FileList : public QList<File*> {
public:
  auto containsFile(File *file) -> bool;
};

/*!
    \class Prefix

    Represents a prefix node in a \l ResourceFile tree.
*/
struct Prefix : Node {
  Prefix(const QString &_name = QString(), const QString &_lang = QString(), const FileList &_file_list = FileList()) : Node(nullptr, this), name(_name), lang(_lang), file_list(_file_list) {}

  ~Prefix()
  {
    qDeleteAll(file_list);
    file_list.clear();
  }

  auto operator ==(const Prefix &other) const -> bool { return (name == other.name) && (lang == other.lang); }

  QString name;
  QString lang;
  FileList file_list;
};

using PrefixList = QList<Prefix*>;

/*!
    \class ResourceFile

    Represents the structure of a Qt Resource File (.qrc) file.
*/
class ResourceFile {
  Q_DECLARE_TR_FUNCTIONS(ResourceFile)

public:
  ResourceFile(const Utils::FilePath &filePath = {}, const QString &contents = {});
  ~ResourceFile();

  auto setFilePath(const Utils::FilePath &filePath) -> void { m_filePath = filePath; }
  auto filePath() const -> Utils::FilePath { return m_filePath; }
  auto load() -> Core::IDocument::OpenResult;
  auto save() -> bool;
  auto contents() const -> QString;
  auto errorMessage() const -> QString { return m_error_message; }
  auto refresh() -> void;
  auto prefixCount() const -> int;
  auto prefix(int idx) const -> QString;
  auto lang(int idx) const -> QString;
  auto fileCount(int prefix_idx) const -> int;
  auto file(int prefix_idx, int file_idx) const -> QString;
  auto alias(int prefix_idx, int file_idx) const -> QString;
  auto addFile(int prefix_idx, const QString &file, int file_idx = -1) -> int;
  auto addPrefix(const QString &prefix, const QString &lang, int prefix_idx = -1) -> int;
  auto removePrefix(int prefix_idx) -> void;
  auto removeFile(int prefix_idx, int file_idx) -> void;
  auto replacePrefix(int prefix_idx, const QString &prefix) -> bool;
  auto replaceLang(int prefix_idx, const QString &lang) -> bool;
  auto replacePrefixAndLang(int prefix_idx, const QString &prefix, const QString &lang) -> bool;
  auto replaceAlias(int prefix_idx, int file_idx, const QString &alias) -> void;
  auto renameFile(const QString &fileName, const QString &newFileName) -> bool;
  auto replaceFile(int pref_idx, int file_idx, const QString &file) -> void;
  auto indexOfPrefix(const QString &prefix, const QString &lang) const -> int;
  auto indexOfFile(int pref_idx, const QString &file) const -> int;
  auto contains(const QString &prefix, const QString &lang, const QString &file = QString()) const -> bool;
  auto contains(int pref_idx, const QString &file) const -> bool;
  auto relativePath(const QString &abs_path) const -> QString;
  auto absolutePath(const QString &rel_path) const -> QString;
  auto orderList() -> void;
  static auto fixPrefix(const QString &prefix) -> QString;

private:
  PrefixList m_prefix_list;
  Utils::FilePath m_filePath;
  QString m_contents;
  QString m_error_message;
  Utils::TextFileFormat m_textFileFormat;

public:
  auto prefixPointer(int prefixIndex) const -> void*;
  auto filePointer(int prefixIndex, int fileIndex) const -> void*;
  auto prefixPointerIndex(const Prefix *prefix) const -> int;

private:
  auto clearPrefixList() -> void;
  auto indexOfPrefix(const QString &prefix, const QString &lang, int skip) const -> int;
};

/*!
    \class ResourceModel

    Wraps a \l ResourceFile as a single-column tree model.
*/
class ResourceModel : public QAbstractItemModel {
  Q_OBJECT

public:
  explicit ResourceModel(QObject *parent = nullptr);

  auto index(int row, int column, const QModelIndex &parent = {}) const -> QModelIndex override;
  auto parent(const QModelIndex &index) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent) const -> int override;
  auto columnCount(const QModelIndex &parent) const -> int override;
  auto hasChildren(const QModelIndex &parent) const -> bool override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto refresh() -> void;
  auto errorMessage() const -> QString;
  auto nonExistingFiles() const -> QList<QModelIndex>;

protected:
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role) -> bool override;

public:
  auto filePath() const -> Utils::FilePath { return m_resource_file.filePath(); }
  auto setFilePath(const Utils::FilePath &filePath) -> void { m_resource_file.setFilePath(filePath); }
  auto getItem(const QModelIndex &index, QString &prefix, QString &file) const -> void;
  auto lang(const QModelIndex &index) const -> QString;
  auto alias(const QModelIndex &index) const -> QString;
  auto file(const QModelIndex &index) const -> QString;

  virtual auto addNewPrefix() -> QModelIndex;
  virtual auto addFiles(const QModelIndex &idx, const QStringList &file_list) -> QModelIndex;
  auto existingFilesSubtracted(int prefixIndex, const QStringList &fileNames) const -> QStringList;
  auto addFiles(int prefixIndex, const QStringList &fileNames, int cursorFile, int &firstFile, int &lastFile) -> void;
  auto insertPrefix(int prefixIndex, const QString &prefix, const QString &lang) -> void;
  auto insertFile(int prefixIndex, int fileIndex, const QString &fileName, const QString &alias) -> void;
  virtual auto changePrefix(const QModelIndex &idx, const QString &prefix) -> void;
  virtual auto changeLang(const QModelIndex &idx, const QString &lang) -> void;
  virtual auto changeAlias(const QModelIndex &idx, const QString &alias) -> void;
  virtual auto deleteItem(const QModelIndex &idx) -> QModelIndex;
  auto getIndex(const QString &prefix, const QString &lang, const QString &file) -> QModelIndex;
  auto prefixIndex(const QModelIndex &sel_idx) const -> QModelIndex;
  auto absolutePath(const QString &path) const -> QString { return m_resource_file.absolutePath(path); }
  auto relativePath(const QString &path) const -> QString { return m_resource_file.relativePath(path); }

private:
  auto lastResourceOpenDirectory() const -> QString;
  auto renameFile(const QString &fileName, const QString &newFileName) -> bool;

public:
  virtual auto reload() -> Core::IDocument::OpenResult;
  virtual auto save() -> bool;
  auto contents() const -> QString { return m_resource_file.contents(); }

  // QString errorMessage() const { return m_resource_file.errorMessage(); }

  auto dirty() const -> bool { return m_dirty; }
  auto setDirty(bool b) -> void;
  auto orderList() -> void;

private:
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;

  static auto iconFileExtension(const QString &path) -> bool;
  static auto resourcePath(const QString &prefix, const QString &file) -> QString;

signals:
  auto dirtyChanged(bool b) -> void;
  auto contentsChanged() -> void;

private:
  ResourceFile m_resource_file;
  bool m_dirty;
  QString m_lastResourceDir;
  QIcon m_prefixIcon;
};

/*!
    \class EntryBackup

    Holds the backup of a tree node including children.
*/
class EntryBackup {
protected:
  ResourceModel *m_model;
  int m_prefixIndex;
  QString m_name;

  EntryBackup(ResourceModel &model, int prefixIndex, const QString &name) : m_model(&model), m_prefixIndex(prefixIndex), m_name(name) { }

public:
  virtual auto restore() const -> void = 0;
  virtual ~EntryBackup() = default;
};

class RelativeResourceModel : public ResourceModel {
public:
  RelativeResourceModel(QObject *parent = nullptr);

  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override
  {
    if (!index.isValid())
      return QVariant();
    /*
            void const * const internalPointer = index.internalPointer();
    
            if ((role == Qt::DisplayRole) && (internalPointer != NULL))
                return ResourceModel::data(index, Qt::ToolTipRole);
    */
    return ResourceModel::data(index, role);
  }

  auto setResourceDragEnabled(bool e) -> void { m_resourceDragEnabled = e; }
  auto resourceDragEnabled() const -> bool { return m_resourceDragEnabled; }
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto removeEntry(const QModelIndex &index) -> EntryBackup*;

private:
  bool m_resourceDragEnabled;
};

} // namespace Internal
} // namespace ResourceEditor
