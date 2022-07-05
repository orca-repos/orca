// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "treeviewcombobox.h"

#include <QWheelEvent>

using namespace Utils;

TreeViewComboBoxView::TreeViewComboBoxView(QWidget *parent) : QTreeView(parent)
{
  // TODO: Disable the root for all items (with a custom delegate?)
  setRootIsDecorated(false);
}

auto TreeViewComboBoxView::adjustWidth(int width) -> void
{
  setMaximumWidth(width);
  setMinimumWidth(qMin(qMax(sizeHintForColumn(0), minimumSizeHint().width()), width));
}

TreeViewComboBox::TreeViewComboBox(QWidget *parent) : QComboBox(parent)
{
  m_view = new TreeViewComboBoxView;
  m_view->setHeaderHidden(true);
  m_view->setItemsExpandable(true);
  setView(m_view);
  m_view->viewport()->installEventFilter(this);
}

auto TreeViewComboBox::indexAbove(QModelIndex index) -> QModelIndex
{
  do
    index = m_view->indexAbove(index); while (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable));
  return index;
}

auto TreeViewComboBox::indexBelow(QModelIndex index) -> QModelIndex
{
  do
    index = m_view->indexBelow(index); while (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable));
  return index;
}

auto TreeViewComboBox::lastIndex(const QModelIndex &index) -> QModelIndex
{
  if (index.isValid() && !m_view->isExpanded(index))
    return index;

  int rows = m_view->model()->rowCount(index);
  if (rows == 0)
    return index;
  return lastIndex(m_view->model()->index(rows - 1, 0, index));
}

auto TreeViewComboBox::wheelEvent(QWheelEvent *e) -> void
{
  QModelIndex index = m_view->currentIndex();
  if (e->angleDelta().y() > 0)
    index = indexAbove(index);
  else if (e->angleDelta().y() < 0)
    index = indexBelow(index);

  e->accept();
  if (!index.isValid())
    return;

  setCurrentIndex(index);

  // for compatibility we emit activated with a useless row parameter
  emit activated(index.row());
}

auto TreeViewComboBox::keyPressEvent(QKeyEvent *e) -> void
{
  if (e->key() == Qt::Key_Up || e->key() == Qt::Key_PageUp) {
    setCurrentIndex(indexAbove(m_view->currentIndex()));
  } else if (e->key() == Qt::Key_Down || e->key() == Qt::Key_PageDown) {
    setCurrentIndex(indexBelow(m_view->currentIndex()));
  } else if (e->key() == Qt::Key_Home) {
    QModelIndex index = m_view->model()->index(0, 0);
    if (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable))
      index = indexBelow(index);
    setCurrentIndex(index);
  } else if (e->key() == Qt::Key_End) {
    QModelIndex index = lastIndex(m_view->rootIndex());
    if (index.isValid() && !(model()->flags(index) & Qt::ItemIsSelectable))
      index = indexAbove(index);
    setCurrentIndex(index);
  } else {
    QComboBox::keyPressEvent(e);
    return;
  }

  e->accept();
}

auto TreeViewComboBox::setCurrentIndex(const QModelIndex &index) -> void
{
  if (!index.isValid())
    return;
  setRootModelIndex(model()->parent(index));
  QComboBox::setCurrentIndex(index.row());
  setRootModelIndex(QModelIndex());
  m_view->setCurrentIndex(index);
}

auto TreeViewComboBox::eventFilter(QObject *object, QEvent *event) -> bool
{
  if (event->type() == QEvent::MouseButtonPress && object == view()->viewport()) {
    auto *mouseEvent = static_cast<QMouseEvent*>(event);
    QModelIndex index = view()->indexAt(mouseEvent->pos());
    if (!view()->visualRect(index).contains(mouseEvent->pos()))
      m_skipNextHide = true;
  }
  return false;
}

auto TreeViewComboBox::showPopup() -> void
{
  m_view->adjustWidth(topLevelWidget()->geometry().width());
  QComboBox::showPopup();
}

auto TreeViewComboBox::hidePopup() -> void
{
  if (m_skipNextHide)
    m_skipNextHide = false;
  else
    QComboBox::hidePopup();
}

auto TreeViewComboBox::view() const -> TreeViewComboBoxView*
{
  return m_view;
}
