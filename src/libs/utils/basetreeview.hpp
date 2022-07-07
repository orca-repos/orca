// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "itemviews.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {

namespace Internal { class BaseTreeViewPrivate; }

class ORCA_UTILS_EXPORT BaseTreeView : public TreeView {
  Q_OBJECT

public:
  enum {
    ExtraIndicesForColumnWidth = 12734,
    ItemViewEventRole = Qt::UserRole + 12735,
    ItemActivatedRole,
    ItemClickedRole,
    ItemDelegateRole,
  };

  BaseTreeView(QWidget *parent = nullptr);
  ~BaseTreeView() override;

  auto setSettings(QSettings *settings, const QByteArray &key) -> void;

  auto setModel(QAbstractItemModel *model) -> void override;
  auto mousePressEvent(QMouseEvent *ev) -> void override;
  auto mouseMoveEvent(QMouseEvent *ev) -> void override;
  auto mouseReleaseEvent(QMouseEvent *ev) -> void override;
  auto contextMenuEvent(QContextMenuEvent *ev) -> void override;
  auto showEvent(QShowEvent *ev) -> void override;
  auto keyPressEvent(QKeyEvent *ev) -> void override;
  auto dragEnterEvent(QDragEnterEvent *ev) -> void override;
  auto dropEvent(QDropEvent *ev) -> void override;
  auto dragMoveEvent(QDragMoveEvent *ev) -> void override;
  auto mouseDoubleClickEvent(QMouseEvent *ev) -> void override;
  auto resizeEvent(QResizeEvent *event) -> void override;
  auto showProgressIndicator() -> void;
  auto hideProgressIndicator() -> void;
  auto resizeColumns() -> void;
  auto spanColumn() const -> int;
  auto setSpanColumn(int column) -> void;
  auto enableColumnHiding() -> void;

  // In some situations this needs to be called when manually resizing columns when the span
  // column is set.
  auto refreshSpanColumn() -> void;

signals:
  auto aboutToShow() -> void;

private:
  auto rowActivated(const QModelIndex &index) -> void;
  auto rowClicked(const QModelIndex &index) -> void;

  Internal::BaseTreeViewPrivate *d;
};

template <typename Event>
struct EventCode;

template <>
struct EventCode<QDragEnterEvent> {
  enum { code = QEvent::DragEnter };
};

template <>
struct EventCode<QDragLeaveEvent> {
  enum { code = QEvent::DragLeave };
};

template <>
struct EventCode<QDragMoveEvent> {
  enum { code = QEvent::DragMove };
};

template <>
struct EventCode<QDropEvent> {
  enum { code = QEvent::Drop };
};

template <>
struct EventCode<QContextMenuEvent> {
  enum { code = QEvent::ContextMenu };
};

template <>
struct EventCode<QMouseEvent> {
  enum { code = QEvent::MouseButtonPress };
};

template <>
struct EventCode<QKeyEvent> {
  enum { code = QEvent::KeyPress };
};

template <class T>
auto checkEventType(QEvent *ev) -> T*
{
  const int cc = EventCode<T>::code;
  const int tt = ev->type();
  if (cc == tt)
    return static_cast<T*>(ev);
  if (cc == QEvent::MouseButtonPress) {
    if (tt == QEvent::MouseButtonDblClick || tt == QEvent::MouseButtonRelease || tt == QEvent::MouseMove)
      return static_cast<T*>(ev);
  }
  if (cc == QEvent::KeyPress && tt == QEvent::KeyRelease)
    return static_cast<T*>(ev);
  return nullptr;
}

class ORCA_UTILS_EXPORT ItemViewEvent {
public:
  ItemViewEvent() = default;
  ItemViewEvent(QEvent *ev, QAbstractItemView *view);

  template <class T>
  auto as() const -> T*
  {
    return checkEventType<T>(m_event);
  }

  template <class T>
  auto as(QEvent::Type t) const -> T*
  {
    return m_event->type() == t ? as<T>() : nullptr;
  }

  auto type() const -> QEvent::Type { return m_event->type(); }
  auto view() const -> QWidget* { return m_view; }
  auto pos() const -> QPoint { return m_pos; }
  auto globalPos() const -> QPoint { return m_view->mapToGlobal(m_pos); }
  auto index() const -> QModelIndex { return m_index; }
  auto sourceModelIndex() const -> QModelIndex { return m_sourceModelIndex; }
  auto selectedRows() const -> QModelIndexList { return m_selectedRows; }
  auto currentOrSelectedRows() const -> QModelIndexList;

private:
  QEvent *m_event = nullptr;
  QWidget *m_view = nullptr;
  QPoint m_pos;
  QModelIndex m_index;
  QModelIndex m_sourceModelIndex;
  QModelIndexList m_selectedRows;
};

} // namespace Utils

Q_DECLARE_METATYPE(Utils::ItemViewEvent);
