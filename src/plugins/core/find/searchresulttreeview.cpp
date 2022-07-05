// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchresulttreeview.h"
#include "searchresulttreeitemroles.h"
#include "searchresulttreemodel.h"
#include "searchresulttreeitemdelegate.h"

#include <utils/qtcassert.h>

#include <QHeaderView>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace Core {
namespace Internal {

class FilterWidget final : public QWidget {
public:
  FilterWidget(QWidget *parent, QWidget *content) : QWidget(parent, Qt::Popup)
  {
    setAttribute(Qt::WA_DeleteOnClose);
    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addWidget(content);
    setLayout(layout);
    move(parent->mapToGlobal(QPoint(0, -QWidget::sizeHint().height())));
  }
};

SearchResultTreeView::SearchResultTreeView(QWidget *parent) : TreeView(parent), m_model(new SearchResultFilterModel(this)), m_auto_expand_results(false)
{
  QTreeView::setModel(m_model);
  connect(m_model, &SearchResultFilterModel::filterInvalidated, this, &SearchResultTreeView::filterInvalidated);

  setItemDelegate(new SearchResultTreeItemDelegate(8, this));
  setIndentation(14);
  setUniformRowHeights(true);
  setExpandsOnDoubleClick(true);
  header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  header()->setStretchLastSection(false);
  header()->hide();

  connect(this, &SearchResultTreeView::activated, this, &SearchResultTreeView::emitJumpToSearchResult);
}

auto SearchResultTreeView::setAutoExpandResults(const bool expand) -> void
{
  m_auto_expand_results = expand;
}

auto SearchResultTreeView::setTextEditorFont(const QFont &font, const search_result_colors &colors) -> void
{
  m_model->setTextEditorFont(font, colors);

  QPalette p;
  p.setColor(QPalette::Base, colors.value(SearchResultColor::Style::Default).text_background);
  setPalette(p);
}

auto SearchResultTreeView::clear() const -> void
{
  m_model->clear();
}

auto SearchResultTreeView::addResults(const QList<SearchResultItem> &items, const SearchResult::AddMode mode) -> void
{
  if (auto added_parents = m_model->addResults(items, mode); m_auto_expand_results && !added_parents.isEmpty()) {
    for(const auto &index: added_parents)
      setExpanded(index, true);
  }
}

auto SearchResultTreeView::setFilter(SearchResultFilter *filter) -> void
{
  m_filter = filter;

  if (m_filter)
    m_filter->setParent(this);

  m_model->setFilter(filter);
  emit filterChanged();
}

auto SearchResultTreeView::hasFilter() const -> bool
{
  return m_filter;
}

auto SearchResultTreeView::showFilterWidget(QWidget *parent) const -> void
{
  QTC_ASSERT(hasFilter(), return);
  const auto options_widget = new FilterWidget(parent, m_filter->createWidget());
  options_widget->show();
}

auto SearchResultTreeView::keyPressEvent(QKeyEvent *event) -> void
{
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == 0 && currentIndex().isValid() && state() != EditingState) {
    const auto item = model()->data(currentIndex(), ItemDataRoles::ResultItemRole).value<SearchResultItem>();
    emit jumpToSearchResult(item);
    return;
  }

  TreeView::keyPressEvent(event);
}

auto SearchResultTreeView::event(QEvent *e) -> bool
{
  if (e->type() == QEvent::Resize)
    header()->setMinimumSectionSize(width());

  return TreeView::event(e);
}

auto SearchResultTreeView::emitJumpToSearchResult(const QModelIndex &index) -> void
{
  if (model()->data(index, ItemDataRoles::IsGeneratedRole).toBool())
    return;

  const auto item = model()->data(index, ItemDataRoles::ResultItemRole).value<SearchResultItem>();

  emit jumpToSearchResult(item);
}

auto SearchResultTreeView::setTabWidth(const int tab_width) -> void
{
  const auto delegate = dynamic_cast<SearchResultTreeItemDelegate*>(itemDelegate());
  delegate->setTabWidth(tab_width);
  doItemsLayout();
}

auto SearchResultTreeView::model() const -> SearchResultFilterModel*
{
  return m_model;
}

} // namespace Internal
} // namespace Core
