// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchresultwindow.hpp"

#include <QList>

namespace Core {
namespace Internal {

class SearchResultTreeItem final {
public:
  explicit SearchResultTreeItem(const SearchResultItem &item = SearchResultItem(), SearchResultTreeItem *parent = nullptr);
  ~SearchResultTreeItem();

  auto isLeaf() const -> bool;
  auto parent() const -> SearchResultTreeItem*;
  auto childAt(int index) const -> SearchResultTreeItem*;
  auto insertionIndex(const QString &text, SearchResultTreeItem **existing_item) const -> int;
  auto insertionIndex(const SearchResultItem &item, SearchResultTreeItem **existing_item) const -> int;
  auto insertChild(int index, SearchResultTreeItem *child) -> void;
  auto insertChild(int index, const SearchResultItem &item) -> void;
  auto appendChild(const SearchResultItem &item) -> void;
  auto childrenCount() const -> int;
  auto rowOfItem() const -> int;
  auto clearChildren() -> void;
  auto checkState() const -> Qt::CheckState;
  auto setCheckState(Qt::CheckState check_state) -> void;
  auto isGenerated() const -> bool { return m_is_generated; }
  auto setGenerated(const bool value) -> void { m_is_generated = value; }

  SearchResultItem item;

private:
  SearchResultTreeItem *m_parent;
  QList<SearchResultTreeItem*> m_children;
  bool m_is_generated;
  Qt::CheckState m_check_state;
};

} // namespace Internal
} // namespace Core
