// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchresulttreeitems.hpp"

namespace Core {
namespace Internal {

SearchResultTreeItem::SearchResultTreeItem(const SearchResultItem &item, SearchResultTreeItem *parent) : item(item), m_parent(parent), m_is_generated(false), m_check_state(item.selectForReplacement() ? Qt::Checked : Qt::Unchecked) {}

SearchResultTreeItem::~SearchResultTreeItem()
{
  clearChildren();
}

auto SearchResultTreeItem::isLeaf() const -> bool
{
  return childrenCount() == 0 && parent() != nullptr;
}

auto SearchResultTreeItem::checkState() const -> Qt::CheckState
{
  return m_check_state;
}

auto SearchResultTreeItem::setCheckState(const Qt::CheckState check_state) -> void
{
  m_check_state = check_state;
}

auto SearchResultTreeItem::clearChildren() -> void
{
  qDeleteAll(m_children);
  m_children.clear();
}

auto SearchResultTreeItem::childrenCount() const -> int
{
  return static_cast<int>(m_children.count());
}

auto SearchResultTreeItem::rowOfItem() const -> int
{
  return m_parent ? static_cast<int>(m_parent->m_children.indexOf(const_cast<SearchResultTreeItem*>(this))) : 0;
}

auto SearchResultTreeItem::childAt(const int index) const -> SearchResultTreeItem*
{
  return m_children.at(index);
}

auto SearchResultTreeItem::parent() const -> SearchResultTreeItem*
{
  return m_parent;
}

static auto lessThanByText(SearchResultTreeItem *a, const QString &b) -> bool
{
  return a->item.lineText() < b;
}

auto SearchResultTreeItem::insertionIndex(const QString &text, SearchResultTreeItem **existing_item) const -> int
{
  const auto insertion_position = std::lower_bound(m_children.begin(), m_children.end(), text, lessThanByText);

  if (existing_item) {
    if (insertion_position != m_children.end() && (*insertion_position)->item.lineText() == text)
      *existing_item = *insertion_position;
    else
      *existing_item = nullptr;
  }

  return static_cast<int>(insertion_position - m_children.begin());
}

auto SearchResultTreeItem::insertionIndex(const SearchResultItem &item, SearchResultTreeItem **existing_item) const -> int
{
  return insertionIndex(item.lineText(), existing_item);
}

auto SearchResultTreeItem::insertChild(const int index, SearchResultTreeItem *child) -> void
{
  m_children.insert(index, child);
}

auto SearchResultTreeItem::insertChild(const int index, const SearchResultItem &item) -> void
{
  const auto child = new SearchResultTreeItem(item, this);
  insertChild(index, child);
}

auto SearchResultTreeItem::appendChild(const SearchResultItem &item) -> void
{
  insertChild(static_cast<int>(m_children.count()), item);
}

} // namespace Internal
} // namespace Core
