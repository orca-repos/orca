// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QStandardItemModel>

namespace ClassView {
namespace Internal {

class TreeItemModelPrivate;

class TreeItemModel : public QStandardItemModel {
  Q_OBJECT

public:
  explicit TreeItemModel(QObject *parent = nullptr);
  ~TreeItemModel() override;

  auto moveRootToTarget(const QStandardItem *target) -> void;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto canFetchMore(const QModelIndex &parent) const -> bool override;
  auto fetchMore(const QModelIndex &parent) -> void override;
  auto hasChildren(const QModelIndex &parent = QModelIndex()) const -> bool override;
  auto supportedDragActions() const -> Qt::DropActions override;
  auto mimeTypes() const -> QStringList override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;

private:
  //! private class data pointer
  TreeItemModelPrivate *d;
};

} // namespace Internal
} // namespace ClassView
