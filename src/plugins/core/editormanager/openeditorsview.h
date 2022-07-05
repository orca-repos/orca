// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/inavigationwidgetfactory.h>
#include <core/opendocumentstreeview.h>

#include <QAbstractProxyModel>
#include <QCoreApplication>

namespace Core {

class IEditor;

namespace Internal {

class ProxyModel final : public QAbstractProxyModel {
public:
  explicit ProxyModel(QObject *parent = nullptr);

  auto mapFromSource(const QModelIndex &source_index) const -> QModelIndex override;
  auto mapToSource(const QModelIndex &proxy_index) const -> QModelIndex override;
  auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto setSourceModel(QAbstractItemModel *source_model) -> void override;
  // QAbstractProxyModel::sibling is broken in Qt 5
  auto sibling(int row, int column, const QModelIndex &idx) const -> QModelIndex override;
  // QAbstractProxyModel::supportedDragActions delegation is missing in Qt 5
  auto supportedDragActions() const -> Qt::DropActions override;

private:
  auto sourceDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right) -> void;
  auto sourceRowsRemoved(const QModelIndex &parent, int start, int end) -> void;
  auto sourceRowsInserted(const QModelIndex &parent, int start, int end) -> void;
  auto sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) -> void;
  auto sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end) -> void;
};

class OpenEditorsWidget : public OpenDocumentsTreeView {
  Q_DECLARE_TR_FUNCTIONS(OpenEditorsWidget)
  Q_DISABLE_COPY_MOVE(OpenEditorsWidget)

public:
  OpenEditorsWidget();
  ~OpenEditorsWidget() override;

private:
  auto handleActivated(const QModelIndex &) -> void;
  auto updateCurrentItem(const IEditor *) -> void;
  auto contextMenuRequested(QPoint pos) const -> void;
  auto activateEditor(const QModelIndex &index) const -> void;
  auto closeDocument(const QModelIndex &index) -> void;

  ProxyModel *m_model;
};

class OpenEditorsViewFactory final : public INavigationWidgetFactory {
public:
  OpenEditorsViewFactory();

  auto createWidget() -> NavigationView override;
};

} // namespace Internal
} // namespace Core
