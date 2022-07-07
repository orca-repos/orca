// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "itemviewfind.hpp"

#include <core/findplaceholder.hpp>

#include <aggregation/aggregate.hpp>

#include <QTextCursor>
#include <QTreeView>
#include <QVBoxLayout>

namespace Core {

/*!
    \class Core::ItemViewFind
    \inmodule Orca
    \internal
*/

class ItemModelFindPrivate {
public:
  explicit ItemModelFindPrivate(QAbstractItemView *view, const int role, const ItemViewFind::FetchOption option) : m_view(view), m_incremental_wrapped_state(false), m_role(role), m_option(option) { }

  QAbstractItemView *m_view;
  QModelIndex m_incremental_find_start;
  bool m_incremental_wrapped_state;
  int m_role;
  ItemViewFind::FetchOption m_option;
};

ItemViewFind::ItemViewFind(QAbstractItemView *view, const int role, const FetchOption option) : d(new ItemModelFindPrivate(view, role, option)) {}

ItemViewFind::~ItemViewFind()
{
  delete d;
}

auto ItemViewFind::supportsReplace() const -> bool
{
  return false;
}

auto ItemViewFind::supportedFindFlags() const -> FindFlags
{
  return FindBackward | FindCaseSensitively | FindRegularExpression | FindWholeWords;
}

auto ItemViewFind::resetIncrementalSearch() -> void
{
  d->m_incremental_find_start = QModelIndex();
  d->m_incremental_wrapped_state = false;
}

auto ItemViewFind::clearHighlights() -> void {}

auto ItemViewFind::currentFindString() const -> QString
{
  return {};
}

auto ItemViewFind::completedFindString() const -> QString
{
  return {};
}

auto ItemViewFind::highlightAll([[maybe_unused]] const QString &txt, [[maybe_unused]] FindFlags find_flags) -> void {}

auto ItemViewFind::findIncremental(const QString &txt, const FindFlags find_flags) -> Result
{
  if (!d->m_incremental_find_start.isValid()) {
    d->m_incremental_find_start = d->m_view->currentIndex();
    d->m_incremental_wrapped_state = false;
  }

  d->m_view->setCurrentIndex(d->m_incremental_find_start);
  auto wrapped = false;
  const auto result = find(txt, find_flags, true /*startFromCurrent*/, &wrapped);

  if (wrapped != d->m_incremental_wrapped_state) {
    d->m_incremental_wrapped_state = wrapped;
    showWrapIndicator(d->m_view);
  }

  return result;
}

auto ItemViewFind::findStep(const QString &txt, const FindFlags find_flags) -> Result
{
  auto wrapped = false;
  const auto result = find(txt, find_flags, false/*startFromNext*/, &wrapped);

  if (wrapped)
    showWrapIndicator(d->m_view);

  if (result == Found) {
    d->m_incremental_find_start = d->m_view->currentIndex();
    d->m_incremental_wrapped_state = false;
  }

  return result;
}

static auto createHelper(QAbstractItemView *tree_view, const ItemViewFind::ColorOption color_option, ItemViewFind *finder) -> QFrame*
{
  const auto widget = new QFrame;
  widget->setFrameStyle(QFrame::NoFrame);

  const auto place_holder = new FindToolBarPlaceHolder(widget);
  place_holder->setLightColored(color_option);

  const auto vbox = new QVBoxLayout(widget);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  vbox->addWidget(tree_view);
  vbox->addWidget(place_holder);

  const auto agg = new Aggregation::Aggregate;
  agg->add(tree_view);
  agg->add(finder);

  return widget;
}

auto ItemViewFind::createSearchableWrapper(QAbstractItemView *tree_view, const ColorOption color_option, const FetchOption option) -> QFrame*
{
  return createHelper(tree_view, color_option, new ItemViewFind(tree_view, Qt::DisplayRole, option));
}

auto ItemViewFind::createSearchableWrapper(ItemViewFind *finder, const ColorOption color_option) -> QFrame*
{
  return createHelper(finder->d->m_view, color_option, finder);
}

auto ItemViewFind::find(const QString &search_txt, const FindFlags find_flags, const bool start_from_current_index, bool *wrapped) const -> Result
{
  if (wrapped)
    *wrapped = false;

  if (search_txt.isEmpty())
    return NotFound;

  if (d->m_view->model()->rowCount() <= 0) // empty model
    return NotFound;

  auto current_index = d->m_view->currentIndex();
  if (!current_index.isValid()) // nothing selected, start from top
    current_index = d->m_view->model()->index(0, 0);

  const auto flags = textDocumentFlagsForFindFlags(find_flags);
  QModelIndex result_index;
  auto index = current_index;
  auto current_row = current_index.row();

  const bool sensitive = find_flags & FindCaseSensitively;
  QRegularExpression search_expr;

  if (find_flags & FindRegularExpression) {
    search_expr = QRegularExpression(search_txt, sensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
  } else if (find_flags & FindWholeWords) {
    const auto escaped_search_text = QRegularExpression::escape(search_txt);
    const QString word_boundary = QLatin1String("\b");
    search_expr = QRegularExpression(word_boundary + escaped_search_text + word_boundary, sensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
  } else {
    search_expr = QRegularExpression(QRegularExpression::escape(search_txt), sensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
  }

  const bool backward = flags & QTextDocument::FindBackward;

  if (wrapped)
    *wrapped = false;

  auto any_wrapped = false;
  auto step_wrapped = false;

  if (!start_from_current_index)
    index = followingIndex(index, backward, &step_wrapped);
  else
    current_row = -1;

  do {
    any_wrapped |= step_wrapped; // update wrapped state if we actually stepped to next/prev item
    if (index.isValid()) {
      if (const auto &text = d->m_view->model()->data(index, d->m_role).toString(); d->m_view->model()->flags(index) & Qt::ItemIsSelectable && (index.row() != current_row || index.parent() != current_index.parent()) && text.indexOf(search_expr) != -1)
        result_index = index;
    }
    index = followingIndex(index, backward, &step_wrapped);
  } while (!result_index.isValid() && index.isValid() && index != current_index);

  if (result_index.isValid()) {
    d->m_view->setCurrentIndex(result_index);
    d->m_view->scrollTo(result_index);
    if (result_index.parent().isValid())
      if (const auto tree_view = qobject_cast<QTreeView*>(d->m_view))
        tree_view->expand(result_index.parent());

    if (wrapped)
      *wrapped = any_wrapped;

    return Found;
  }
  return NotFound;
}

auto ItemViewFind::nextIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex
{
  if (wrapped)
    *wrapped = false;

  const auto model = d->m_view->model();

  // pathological
  if (!idx.isValid())
    return model->index(0, 0);

  // same parent has more columns, go to next column
  const auto parent_idx = idx.parent();

  if (idx.column() + 1 < model->columnCount(parent_idx))
    return model->index(idx.row(), idx.column() + 1, parent_idx);

  // tree views have their children attached to first column
  // make sure we are at first column
  auto current = model->index(idx.row(), 0, parent_idx);

  // check for children
  if (d->m_option == FetchMoreWhileSearching && model->canFetchMore(current))
    model->fetchMore(current);
  if (model->rowCount(current) > 0)
    return model->index(0, 0, current);

  // no more children, go up and look for parent with more children
  QModelIndex next_index;
  while (!next_index.isValid()) {
    const auto row = current.row();
    current = current.parent();

    if (d->m_option == FetchMoreWhileSearching && model->canFetchMore(current))
      model->fetchMore(current);

    if (row + 1 < model->rowCount(current)) {
      // Same parent has another child
      next_index = model->index(row + 1, 0, current);
    } else {
      // go up one parent
      if (!current.isValid()) {
        // we start from the beginning
        if (wrapped)
          *wrapped = true;
        next_index = model->index(0, 0);
      }
    }
  }
  return next_index;
}

auto ItemViewFind::prevIndex(const QModelIndex &idx, bool *wrapped) const -> QModelIndex
{
  if (wrapped)
    *wrapped = false;

  const auto model = d->m_view->model();

  // if same parent has earlier columns, just move there
  if (idx.column() > 0)
    return model->index(idx.row(), idx.column() - 1, idx.parent());

  auto current = idx;
  auto check_for_children = true;

  if (current.isValid()) {
    if (const auto row = current.row(); row > 0) {
      current = model->index(row - 1, 0, current.parent());
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
    if (d->m_option == FetchMoreWhileSearching && model->canFetchMore(current))
      model->fetchMore(current);
    while (const auto rc = model->rowCount(current)) {
      current = model->index(rc - 1, 0, current);
    }
  }

  // set to last column
  current = model->index(current.row(), model->columnCount(current.parent()) - 1, current.parent());
  return current;
}

auto ItemViewFind::followingIndex(const QModelIndex &idx, const bool backward, bool *wrapped) const -> QModelIndex
{
  if (backward)
    return prevIndex(idx, wrapped);
  return nextIndex(idx, wrapped);
}

} // namespace Core
