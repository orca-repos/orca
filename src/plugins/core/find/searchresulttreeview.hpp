// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchresultwindow.hpp"

#include <utils/itemviews.hpp>

namespace Core {
class SearchResultColor;

namespace Internal {

class SearchResultFilterModel;

class SearchResultTreeView final : public Utils::TreeView {
  Q_OBJECT

public:
  explicit SearchResultTreeView(QWidget *parent = nullptr);

  auto setAutoExpandResults(bool expand) -> void;
  auto setTextEditorFont(const QFont &font, const search_result_colors &colors) -> void;
  auto setTabWidth(int tab_width) -> void;
  auto model() const -> SearchResultFilterModel*;
  auto addResults(const QList<SearchResultItem> &items, SearchResult::AddMode mode) -> void;
  auto setFilter(SearchResultFilter *filter) -> void;
  auto hasFilter() const -> bool;
  auto showFilterWidget(QWidget *parent) const -> void;
  auto keyPressEvent(QKeyEvent *event) -> void override;
  auto event(QEvent *e) -> bool override;

signals:
  auto jumpToSearchResult(const SearchResultItem &item) -> void;
  auto filterInvalidated() -> void;
  auto filterChanged() -> void;

public slots:
  auto clear() const -> void;
  auto emitJumpToSearchResult(const QModelIndex &index) -> void;

protected:
  SearchResultFilterModel *m_model;
  SearchResultFilter *m_filter = nullptr;
  bool m_auto_expand_results;
};

} // namespace Internal
} // namespace Core
