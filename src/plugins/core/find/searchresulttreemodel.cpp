// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchresulttreemodel.h"
#include "searchresulttreeitems.h"
#include "searchresulttreeitemroles.h"

#include <utils/algorithm.h>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>

namespace Core {
namespace Internal {

class SearchResultTreeModel final : public QAbstractItemModel {
  Q_OBJECT

public:
  explicit SearchResultTreeModel(QObject *parent = nullptr);
  ~SearchResultTreeModel() override;

  auto setShowReplaceUi(bool show) -> void;
  auto setTextEditorFont(const QFont &font, const search_result_colors &colors) -> void;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;
  auto next(const QModelIndex &idx, bool include_generated = false, bool *wrapped = nullptr) const -> QModelIndex;
  auto prev(const QModelIndex &idx, bool include_generated = false, bool *wrapped = nullptr) const -> QModelIndex;
  auto addResults(const QList<SearchResultItem> &items, SearchResult::AddMode mode) -> QList<QModelIndex>;
  static auto treeItemAtIndex(const QModelIndex &idx) -> SearchResultTreeItem*;

signals:
  auto jumpToSearchResult(const QString &file_name, int line_number, int search_term_start, int search_term_length) -> void;

public slots:
  auto clear() -> void;

private:
  auto index(const SearchResultTreeItem *item) const -> QModelIndex;
  auto addResultsToCurrentParent(const QList<SearchResultItem> &items, SearchResult::AddMode mode) -> void;
  auto addPath(const QStringList &path) -> QSet<SearchResultTreeItem*>;
  auto data(const SearchResultTreeItem *row, int role) const -> QVariant;
  auto setCheckState(const QModelIndex &idx, Qt::CheckState check_state, bool first_call = true) -> bool;
  auto updateCheckStateFromChildren(const QModelIndex &idx, SearchResultTreeItem *item) -> void;
  auto nextIndex(const QModelIndex &idx, bool *wrapped = nullptr) const -> QModelIndex;
  auto prevIndex(const QModelIndex &idx, bool *wrapped = nullptr) const -> QModelIndex;

  SearchResultTreeItem *m_root_item;
  SearchResultTreeItem *m_current_parent;
  search_result_colors m_colors;
  QModelIndex m_current_index;
  QStringList m_current_path; // the path that belongs to the current parent
  QFont m_text_editor_font;
  bool m_show_replace_ui;
  bool m_editor_font_is_used;
};

SearchResultTreeModel::SearchResultTreeModel(QObject *parent) : QAbstractItemModel(parent), m_current_parent(nullptr), m_show_replace_ui(false), m_editor_font_is_used(false)
{
  m_root_item = new SearchResultTreeItem;
  m_text_editor_font = QFont(QLatin1String("Courier"));
}

SearchResultTreeModel::~SearchResultTreeModel()
{
  delete m_root_item;
}

auto SearchResultTreeModel::setShowReplaceUi(const bool show) -> void
{
  m_show_replace_ui = show;
  // We cannot send dataChanged for the whole hierarchy in one go,
  // because all items in a dataChanged must have the same parent.
  // Send dataChanged for each parent of children individually...
  QList<QModelIndex> change_queue;
  change_queue.append(QModelIndex());

  while (!change_queue.isEmpty()) {
    const auto current = change_queue.takeFirst();
    if (const auto child_count = rowCount(current); child_count > 0) {
      emit dataChanged(index(0, 0, current), index(child_count - 1, 0, current));
      for (auto r = 0; r < child_count; ++r)
        change_queue.append(index(r, 0, current));
    }
  }
}

auto SearchResultTreeModel::setTextEditorFont(const QFont &font, const search_result_colors &colors) -> void
{
  emit layoutAboutToBeChanged();
  m_text_editor_font = font;
  m_colors = colors;
  emit layoutChanged();
}

auto SearchResultTreeModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  auto flags = QAbstractItemModel::flags(index);

  if (index.isValid()) {
    if (m_show_replace_ui)
      flags |= Qt::ItemIsUserCheckable;
  }

  return flags;
}

auto SearchResultTreeModel::index(const int row, const int column, const QModelIndex &parent) const -> QModelIndex
{
  if (!hasIndex(row, column, parent))
    return {};

  const SearchResultTreeItem *parent_item;

  if (!parent.isValid())
    parent_item = m_root_item;
  else
    parent_item = treeItemAtIndex(parent);

  if (const SearchResultTreeItem *child_item = parent_item->childAt(row))
    return createIndex(row, column, child_item);
  else
    return {};
}

auto SearchResultTreeModel::index(const SearchResultTreeItem *item) const -> QModelIndex
{
  return createIndex(item->rowOfItem(), 0, item);
}

auto SearchResultTreeModel::parent(const QModelIndex &child) const -> QModelIndex
{
  if (!child.isValid())
    return {};

  const SearchResultTreeItem *child_item = treeItemAtIndex(child);
  const SearchResultTreeItem *parent_item = child_item->parent();

  if (parent_item == m_root_item)
    return {};

  return createIndex(parent_item->rowOfItem(), 0, parent_item);
}

auto SearchResultTreeModel::rowCount(const QModelIndex &parent) const -> int
{
  if (parent.column() > 0)
    return 0;

  const SearchResultTreeItem *parent_item;

  if (!parent.isValid())
    parent_item = m_root_item;
  else
    parent_item = treeItemAtIndex(parent);

  return parent_item->childrenCount();
}

auto SearchResultTreeModel::columnCount(const QModelIndex &parent) const -> int
{
  Q_UNUSED(parent)
  return 1;
}

auto SearchResultTreeModel::treeItemAtIndex(const QModelIndex &idx) -> SearchResultTreeItem*
{
  return static_cast<SearchResultTreeItem*>(idx.internalPointer());
}

auto SearchResultTreeModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  if (!index.isValid())
    return {};

  QVariant result;

  if (role == Qt::SizeHintRole) {
    auto height = QApplication::fontMetrics().height();
    if (m_editor_font_is_used) {
      const auto editor_font_height = QFontMetrics(m_text_editor_font).height();
      height = qMax(height, editor_font_height);
    }
    result = QSize(0, height);
  } else {
    result = data(treeItemAtIndex(index), role);
  }

  return result;
}

auto SearchResultTreeModel::setData(const QModelIndex &index, const QVariant &value, const int role) -> bool
{
  if (role == Qt::CheckStateRole) {
    const auto check_state = static_cast<Qt::CheckState>(value.toInt());
    return setCheckState(index, check_state);
  }
  return QAbstractItemModel::setData(index, value, role);
}

auto SearchResultTreeModel::setCheckState(const QModelIndex &idx, const Qt::CheckState check_state, const bool first_call) -> bool
{
  const auto item = treeItemAtIndex(idx);

  if (item->checkState() == check_state)
    return false;

  item->setCheckState(check_state);

  if (first_call) {
    emit dataChanged(idx, idx);
    updateCheckStateFromChildren(idx.parent(), item->parent());
  }

  // check children
  if (const auto children = item->childrenCount()) {
    for (auto i = 0; i < children; ++i)
      setCheckState(index(i, 0, idx), check_state, false);
    emit dataChanged(index(0, 0, idx), index(children - 1, 0, idx));
  }

  return true;
}

auto SearchResultTreeModel::updateCheckStateFromChildren(const QModelIndex &idx, SearchResultTreeItem *item) -> void
{
  if (!item || item == m_root_item)
    return;

  auto has_checked = false;
  auto has_unchecked = false;

  for (auto i = 0; i < item->childrenCount(); ++i) {
    if (const auto child = item->childAt(i); child->checkState() == Qt::Checked)
      has_checked = true;
    else if (child->checkState() == Qt::Unchecked)
      has_unchecked = true;
    else if (child->checkState() == Qt::PartiallyChecked)
      has_checked = has_unchecked = true;
  }

  if (has_checked && has_unchecked)
    item->setCheckState(Qt::PartiallyChecked);
  else if (has_checked)
    item->setCheckState(Qt::Checked);
  else
    item->setCheckState(Qt::Unchecked);

  emit dataChanged(idx, idx);
  updateCheckStateFromChildren(idx.parent(), item->parent());
}

auto setDataInternal(const QModelIndex &index, const QVariant &value, int role) -> void;

auto SearchResultTreeModel::data(const SearchResultTreeItem *row, int role) const -> QVariant
{
  QVariant result;

  switch (role) {
  case Qt::CheckStateRole:
    result = row->checkState();
    break;
  case Qt::ToolTipRole:
    result = row->item.lineText().trimmed();
    break;
  case Qt::FontRole:
    if (row->item.useTextEditorFont())
      result = m_text_editor_font;
    else
      result = QVariant();
    break;
  case Qt::ForegroundRole:
    result = m_colors.value(row->item.style()).text_foreground;
    break;
  case Qt::BackgroundRole:
    result = m_colors.value(row->item.style()).text_background;
    break;
  case ItemDataRoles::ResultLineRole:
  case Qt::DisplayRole:
    result = row->item.lineText();
    break;
  case ItemDataRoles::ResultItemRole:
    result = QVariant::fromValue(row->item);
    break;
  case ItemDataRoles::ResultBeginLineNumberRole:
    result = row->item.mainRange().begin.line;
    break;
  case ItemDataRoles::ResultIconRole:
    result = row->item.icon();
    break;
  case ItemDataRoles::ResultHighlightBackgroundColor:
    result = m_colors.value(row->item.style()).highlight_background;
    break;
  case ItemDataRoles::ResultHighlightForegroundColor:
    result = m_colors.value(row->item.style()).highlight_foreground;
    break;
  case ItemDataRoles::ResultBeginColumnNumberRole:
    result = row->item.mainRange().begin.column;
    break;
  case ItemDataRoles::SearchTermLengthRole:
    result = row->item.mainRange().length(row->item.lineText());
    break;
  case ItemDataRoles::IsGeneratedRole:
    result = row->isGenerated();
    break;
  default:
    result = QVariant();
    break;
  }

  return result;
}

auto SearchResultTreeModel::headerData(const int section, const Qt::Orientation orientation, const int role) const -> QVariant
{
  Q_UNUSED(section)
  Q_UNUSED(orientation)
  Q_UNUSED(role)
  return {};
}

/**
 * Makes sure that the nodes for a specific path exist and sets
 * m_currentParent to the last final
 */
auto SearchResultTreeModel::addPath(const QStringList &path) -> QSet<SearchResultTreeItem*>
{
  QSet<SearchResultTreeItem*> path_nodes;
  auto current_item = m_root_item;
  auto current_item_index = QModelIndex();
  SearchResultTreeItem *part_item = nullptr;
  QStringList current_path;

  for(const auto &part: path) {
    const auto insertion_index = current_item->insertionIndex(part, &part_item);
    if (!part_item) {
      SearchResultItem item;
      item.setPath(current_path);
      item.setLineText(part);
      part_item = new SearchResultTreeItem(item, current_item);
      if (m_show_replace_ui)
        part_item->setCheckState(Qt::Checked);
      part_item->setGenerated(true);
      beginInsertRows(current_item_index, insertion_index, insertion_index);
      current_item->insertChild(insertion_index, part_item);
      endInsertRows();
    }
    path_nodes << part_item;
    current_item_index = index(insertion_index, 0, current_item_index);
    current_item = part_item;
    current_path << part;
  }

  m_current_parent = current_item;
  m_current_path = current_path;
  m_current_index = current_item_index;
  return path_nodes;
}

auto SearchResultTreeModel::addResultsToCurrentParent(const QList<SearchResultItem> &items, const SearchResult::AddMode mode) -> void
{
  if (!m_current_parent)
    return;

  if (mode == SearchResult::AddOrdered) {
    // this is the mode for e.g. text search
    beginInsertRows(m_current_index, m_current_parent->childrenCount(), static_cast<int>(m_current_parent->childrenCount() + items.count()));
    for(const auto &item: items) {
      m_current_parent->appendChild(item);
    }
    endInsertRows();
  } else if (mode == SearchResult::AddSorted) {
    for(const auto &item: items) {
      SearchResultTreeItem *existing_item;
      const auto insertion_index = m_current_parent->insertionIndex(item, &existing_item);
      if (existing_item) {
        existing_item->setGenerated(false);
        existing_item->item = item;
        auto item_index = index(insertion_index, 0, m_current_index);
        emit dataChanged(item_index, item_index);
      } else {
        beginInsertRows(m_current_index, insertion_index, insertion_index);
        m_current_parent->insertChild(insertion_index, item);
        endInsertRows();
      }
    }
  }

  updateCheckStateFromChildren(index(m_current_parent), m_current_parent);
  emit dataChanged(m_current_index, m_current_index); // Make sure that the number after the file name gets updated
}

static auto lessThanByPath(const SearchResultItem &a, const SearchResultItem &b) -> bool
{
  if (a.path().size() < b.path().size())
    return true;

  if (a.path().size() > b.path().size())
    return false;

  for (auto i = 0; i < a.path().size(); ++i) {
    if (a.path().at(i) < b.path().at(i))
      return true;
    if (a.path().at(i) > b.path().at(i))
      return false;
  }

  return false;
}

/**
 * Adds the search result to the list of results, creating nodes for the path when
 * necessary.
 */
auto SearchResultTreeModel::addResults(const QList<SearchResultItem> &items, const SearchResult::AddMode mode) -> QList<QModelIndex>
{
  QSet<SearchResultTreeItem*> path_nodes;
  auto sorted_items = items;
  std::ranges::stable_sort(sorted_items, lessThanByPath);
  QList<SearchResultItem> item_set;

  for(const auto &item: sorted_items) {
    m_editor_font_is_used |= item.useTextEditorFont();
    if (!m_current_parent || (m_current_path != item.path())) {
      // first add all the items from before
      if (!item_set.isEmpty()) {
        addResultsToCurrentParent(item_set, mode);
        item_set.clear();
      }
      // switch parent
      path_nodes += addPath(item.path());
    }
    item_set << item;
  }

  if (!item_set.isEmpty()) {
    addResultsToCurrentParent(item_set, mode);
    item_set.clear();
  }

  QList<QModelIndex> path_indices;
  for(const auto item: path_nodes)
    path_indices << index(item);

  return path_indices;
}

auto SearchResultTreeModel::clear() -> void
{
  beginResetModel();
  m_current_parent = nullptr;
  m_root_item->clearChildren();
  m_editor_font_is_used = false;
  endResetModel();
}

auto SearchResultTreeModel::nextIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex
{
  // pathological
  if (!idx.isValid())
    return index(0, 0);

  if (rowCount(idx) > 0) {
    // node with children
    return index(0, 0, idx);
  }
  // leaf node
  QModelIndex next_index;
  auto current = idx;
  while (!next_index.isValid()) {
    const auto row = current.row();
    current = current.parent();
    if (row + 1 < rowCount(current)) {
      // Same parent has another child
      next_index = index(row + 1, 0, current);
    } else {
      // go up one parent
      if (!current.isValid()) {
        // we start from the beginning
        if (wrapped)
          *wrapped = true;
        next_index = index(0, 0);
      }
    }
  }

  return next_index;
}

auto SearchResultTreeModel::next(const QModelIndex &idx, const bool include_generated, bool *wrapped) const -> QModelIndex
{
  auto value = idx;
  do {
    value = nextIndex(value, wrapped);
  } while (value != idx && !include_generated && treeItemAtIndex(value)->isGenerated());
  return value;
}

auto SearchResultTreeModel::prevIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex
{
  auto current = idx;
  auto check_for_children = true;

  if (current.isValid()) {
    if (const auto row = current.row(); row > 0) {
      current = index(row - 1, 0, current.parent());
    } else {
      current = current.parent();
      check_for_children = !current.isValid();
      if (check_for_children && wrapped) {
        // we start from the end
        *wrapped = true;
      }
    }
  }

  if (check_for_children) {
    // traverse down the hierarchy
    while (const auto rc = rowCount(current)) {
      current = index(rc - 1, 0, current);
    }
  }

  return current;
}

auto SearchResultTreeModel::prev(const QModelIndex &idx, const bool include_generated, bool *wrapped) const -> QModelIndex
{
  auto value = idx;
  do {
    value = prevIndex(value, wrapped);
  } while (value != idx && !include_generated && treeItemAtIndex(value)->isGenerated());
  return value;
}

SearchResultFilterModel::SearchResultFilterModel(QObject *parent) : QSortFilterProxyModel(parent)
{
  QSortFilterProxyModel::setSourceModel(new SearchResultTreeModel(this));
}

auto SearchResultFilterModel::setFilter(SearchResultFilter *filter) -> void
{
  if (m_filter)
    m_filter->disconnect(this);

  m_filter = filter;

  if (m_filter) {
    connect(m_filter, &SearchResultFilter::filterChanged, this, [this] {
      invalidateFilter();
      emit filterInvalidated();
    });
  }

  invalidateFilter();
}

auto SearchResultFilterModel::setShowReplaceUi(const bool show) const -> void
{
  sourceModel()->setShowReplaceUi(show);
}

auto SearchResultFilterModel::setTextEditorFont(const QFont &font, const search_result_colors &colors) const -> void
{
  sourceModel()->setTextEditorFont(font, colors);
}

auto SearchResultFilterModel::addResults(const QList<SearchResultItem> &items, const SearchResult::AddMode mode) const -> QList<QModelIndex>
{
  auto source_indexes = sourceModel()->addResults(items, mode);

  source_indexes = Utils::filtered(source_indexes, [this](const QModelIndex &idx) {
    return filterAcceptsRow(idx.row(), idx.parent());
  });

  return Utils::transform(source_indexes, [this](const QModelIndex &idx) { return mapFromSource(idx); });
}

auto SearchResultFilterModel::clear() const -> void
{
  sourceModel()->clear();
}

auto SearchResultFilterModel::nextOrPrev(const QModelIndex &idx, bool *wrapped, const std::function<QModelIndex (const QModelIndex &)> &func) const -> QModelIndex
{
  if (wrapped)
    *wrapped = false;

  const auto source_index = mapToSource(idx);
  auto next_or_prev_source_index = func(source_index);

  while (next_or_prev_source_index != source_index && !filterAcceptsRow(next_or_prev_source_index.row(), next_or_prev_source_index.parent())) {
    next_or_prev_source_index = func(next_or_prev_source_index);
  }

  return mapFromSource(next_or_prev_source_index);
}

auto SearchResultFilterModel::next(const QModelIndex &idx, bool include_generated, bool *wrapped) const -> QModelIndex
{
  return nextOrPrev(idx, wrapped, [this, include_generated, wrapped](const QModelIndex &index) {
    return sourceModel()->next(index, include_generated, wrapped);
  });
}

auto SearchResultFilterModel::prev(const QModelIndex &idx, bool include_generated, bool *wrapped) const -> QModelIndex
{
  return nextOrPrev(idx, wrapped, [this, include_generated, wrapped](const QModelIndex &index) {
    return sourceModel()->prev(index, include_generated, wrapped);
  });
}

auto SearchResultFilterModel::itemForIndex(const QModelIndex &index) const -> SearchResultTreeItem*
{
  return static_cast<SearchResultTreeItem*>(mapToSource(index).internalPointer());
}

auto SearchResultFilterModel::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const -> bool
{
  const auto idx = sourceModel()->index(source_row, 0, source_parent);
  const SearchResultTreeItem *const item = SearchResultTreeModel::treeItemAtIndex(idx);

  if (!item)
    return false;

  if (!m_filter)
    return true;

  if (item->item.userData().isValid())
    return m_filter->matches(item->item);

  const auto child_count = sourceModel()->rowCount(idx);
  for (auto i = 0; i < child_count; ++i) {
    if (filterAcceptsRow(i, idx))
      return true;
  }

  return false;
}

auto SearchResultFilterModel::sourceModel() const -> SearchResultTreeModel*
{
  return dynamic_cast<SearchResultTreeModel*>(QSortFilterProxyModel::sourceModel());
}

} // namespace Internal
} // namespace Core

#include <searchresulttreemodel.moc>
