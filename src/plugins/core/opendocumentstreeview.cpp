// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "opendocumentstreeview.h"

#include <utils/utilsicons.h>

#include <QApplication>
#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>

namespace Core {
namespace Internal {

class OpenDocumentsDelegate final : public QStyledItemDelegate {
public:
  explicit OpenDocumentsDelegate(QObject *parent = nullptr);

  auto setCloseButtonVisible(bool visible) -> void;
  auto handlePressed(const QModelIndex &index) const -> void;
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;

  mutable QModelIndex pressed_index;
  bool close_button_visible = true;
};

OpenDocumentsDelegate::OpenDocumentsDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

auto OpenDocumentsDelegate::setCloseButtonVisible(const bool visible) -> void
{
  close_button_visible = visible;
}

auto OpenDocumentsDelegate::handlePressed(const QModelIndex &index) const -> void
{
  if (index.column() == 1)
    pressed_index = index;
}

auto OpenDocumentsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  if (option.state & QStyle::State_MouseOver) {
    if ((QApplication::mouseButtons() & Qt::LeftButton) == 0)
      pressed_index = QModelIndex();
    auto brush = option.palette.alternateBase();
    if (index == pressed_index)
      brush = option.palette.dark();
    painter->fillRect(option.rect, brush);
  }

  QStyledItemDelegate::paint(painter, option, index);

  if (close_button_visible && index.column() == 1 && option.state & QStyle::State_MouseOver) {
    const auto icon = (option.state & QStyle::State_Selected) ? Utils::Icons::CLOSE_BACKGROUND.icon() : Utils::Icons::CLOSE_FOREGROUND.icon();
    const QRect icon_rect(option.rect.right() - option.rect.height(), option.rect.top(), option.rect.height(), option.rect.height());
    icon.paint(painter, icon_rect, Qt::AlignRight | Qt::AlignVCenter);
  }
}

} // namespace Internal

OpenDocumentsTreeView::OpenDocumentsTreeView(QWidget *parent) : TreeView(parent)
{
  m_delegate = new Internal::OpenDocumentsDelegate(this);
  setItemDelegate(m_delegate);
  setRootIsDecorated(false);
  setUniformRowHeights(true);
  setTextElideMode(Qt::ElideMiddle);
  setFrameStyle(NoFrame);
  setAttribute(Qt::WA_MacShowFocusRect, false);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  viewport()->setAttribute(Qt::WA_Hover);
  setSelectionMode(SingleSelection);
  setSelectionBehavior(SelectRows);
  setActivationMode(Utils::SingleClickActivation);
  installEventFilter(this);
  viewport()->installEventFilter(this);
  connect(this, &OpenDocumentsTreeView::pressed, m_delegate, &Internal::OpenDocumentsDelegate::handlePressed);
}

auto OpenDocumentsTreeView::setModel(QAbstractItemModel *model) -> void
{
  TreeView::setModel(model);
  header()->hide();
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::Fixed);
  header()->setMinimumSectionSize(0);
  header()->resizeSection(1, 16);
}

auto OpenDocumentsTreeView::setCloseButtonVisible(const bool visible) const -> void
{
  m_delegate->setCloseButtonVisible(visible);
}

auto OpenDocumentsTreeView::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj == this && event->type() == QEvent::KeyPress && currentIndex().isValid()) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) && ke->modifiers() == 0) {
      emit closeActivated(currentIndex());
    }
  } else if (obj == viewport() && event->type() == QEvent::MouseButtonRelease) {
    if (const auto me = dynamic_cast<QMouseEvent*>(event); me->button() == Qt::MiddleButton && me->modifiers() == Qt::NoModifier) {
      if (const auto index = indexAt(me->pos()); index.isValid()) {
        emit closeActivated(index);
        return true;
      }
    }
  }
  return false;
}

} // namespace Core
