// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchresultwindow.hpp"
#include "searchresultcolor.hpp"

#include <QSortFilterProxyModel>

#include <functional>

namespace Core {
namespace Internal {

class SearchResultTreeItem;
class SearchResultTreeModel;

class SearchResultFilterModel final : public QSortFilterProxyModel {
  Q_OBJECT

public:
  explicit SearchResultFilterModel(QObject *parent = nullptr);

  auto setFilter(SearchResultFilter *filter) -> void;
  auto setShowReplaceUi(bool show) const -> void;
  auto setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void;
  auto addResults(const QList<SearchResultItem> &items, SearchResult::AddMode mode) const -> QList<QModelIndex>;
  auto clear() const -> void;
  auto next(const QModelIndex &idx, bool include_generated = false, bool *wrapped = nullptr) const -> QModelIndex;
  auto prev(const QModelIndex &idx, bool include_generated = false, bool *wrapped = nullptr) const -> QModelIndex;
  auto itemForIndex(const QModelIndex &index) const -> SearchResultTreeItem*;

signals:
  auto filterInvalidated() -> void;

private:
  auto filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool override;
  auto nextOrPrev(const QModelIndex &idx, bool *wrapped, const std::function<QModelIndex(const QModelIndex &)> &func) const -> QModelIndex;
  auto sourceModel() const -> SearchResultTreeModel*;

  SearchResultFilter *m_filter = nullptr;
};

} // namespace Internal
} // namespace Core
