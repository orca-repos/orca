// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QAbstractItemModel>
#include <QPair>
#include <QDir>

QT_BEGIN_NAMESPACE
class QFileIconProvider;
QT_END_NAMESPACE

namespace Utils {

class ExtendedInformation;
class FileSystemModelPrivate;

class ORCA_UTILS_EXPORT FileSystemModel : public QAbstractItemModel {
  Q_OBJECT

public:
  enum Roles {
    FileIconRole = Qt::DecorationRole,
    FilePathRole = Qt::UserRole + 1,
    FileNameRole = Qt::UserRole + 2,
    FilePermissions = Qt::UserRole + 3
  };

  enum Option {
    DontWatchForChanges = 0x00000001,
    DontResolveSymlinks = 0x00000002,
    DontUseCustomDirectoryIcons = 0x00000004
  };

  Q_ENUM(Option)
  Q_DECLARE_FLAGS(Options, Option)

  explicit FileSystemModel(QObject *parent = nullptr);
  ~FileSystemModel();

  auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto index(const QString &path, int column = 0) const -> QModelIndex;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  using QObject::parent;
  auto sibling(int row, int column, const QModelIndex &idx) const -> QModelIndex override;
  auto hasChildren(const QModelIndex &parent = QModelIndex()) const -> bool override;
  auto canFetchMore(const QModelIndex &parent) const -> bool override;
  auto fetchMore(const QModelIndex &parent) -> void override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto myComputer(int role = Qt::DisplayRole) const -> QVariant;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto sort(int column, Qt::SortOrder order = Qt::AscendingOrder) -> void override;
  auto mimeTypes() const -> QStringList override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool override;
  auto supportedDropActions() const -> Qt::DropActions override;
  auto roleNames() const -> QHash<int, QByteArray> override;

  // FileSystemModel specific API
  auto setRootPath(const QString &path) -> QModelIndex;
  auto rootPath() const -> QString;
  auto rootDirectory() const -> QDir;
  auto setIconProvider(QFileIconProvider *provider) -> void;
  auto iconProvider() const -> QFileIconProvider*;
  auto setFilter(QDir::Filters filters) -> void;
  auto filter() const -> QDir::Filters;
  auto setResolveSymlinks(bool enable) -> void;
  auto resolveSymlinks() const -> bool;
  auto setReadOnly(bool enable) -> void;
  auto isReadOnly() const -> bool;
  auto setNameFilterDisables(bool enable) -> void;
  auto nameFilterDisables() const -> bool;
  auto setNameFilters(const QStringList &filters) -> void;
  auto nameFilters() const -> QStringList;
  auto setOption(Option option, bool on = true) -> void;
  auto testOption(Option option) const -> bool;
  auto setOptions(Options options) -> void;
  auto options() const -> Options;
  auto filePath(const QModelIndex &index) const -> QString;
  auto isDir(const QModelIndex &index) const -> bool;
  auto size(const QModelIndex &index) const -> qint64;
  auto type(const QModelIndex &index) const -> QString;
  auto lastModified(const QModelIndex &index) const -> QDateTime;
  auto mkdir(const QModelIndex &parent, const QString &name) -> QModelIndex;
  auto rmdir(const QModelIndex &index) -> bool;
  auto fileName(const QModelIndex &aindex) const -> QString;
  auto fileIcon(const QModelIndex &aindex) const -> QIcon;
  auto permissions(const QModelIndex &index) const -> QFile::Permissions;
  auto fileInfo(const QModelIndex &index) const -> QFileInfo;
  auto remove(const QModelIndex &index) -> bool;

signals:
  auto rootPathChanged(const QString &newPath) -> void;
  auto fileRenamed(const QString &path, const QString &oldName, const QString &newName) -> void;
  auto directoryLoaded(const QString &path) -> void;

protected:
  auto timerEvent(QTimerEvent *event) -> void override;
  auto event(QEvent *event) -> bool override;

private:
  FileSystemModelPrivate *d = nullptr;
  friend class FileSystemModelPrivate;
  friend class QFileDialogPrivate;
};

} // Utils

QT_BEGIN_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(Utils::FileSystemModel::Options)
QT_END_NAMESPACE
