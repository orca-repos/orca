// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QItemDelegate>

namespace Orca::Plugin::Core {

struct LayoutInfo {
  QRect check_rect;
  QRect pixmap_rect;
  QRect text_rect;
  QRect line_number_rect;
  QIcon icon;
  Qt::CheckState check_state{};
  QStyleOptionViewItem option;
};

class SearchResultTreeItemDelegate final : public QItemDelegate {
public:
  explicit SearchResultTreeItemDelegate(int tab_width, QObject *parent = nullptr);
  auto paint(QPainter *painter, const QStyleOptionViewItem &option_view_item, const QModelIndex &index) const -> void override;
  auto setTabWidth(int width) -> void;
  auto sizeHint(const QStyleOptionViewItem &option_view_item, const QModelIndex &index) const -> QSize override;

private:
  auto getLayoutInfo(const QStyleOptionViewItem &option, const QModelIndex &index) const -> LayoutInfo;
  auto drawLineNumber(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QModelIndex &index) const -> int;
  auto drawText(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QModelIndex &index) const -> void;

  QString m_tab_string;
};

} // namespace Orca::Plugin::Core
