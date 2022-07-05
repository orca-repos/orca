// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QItemDelegate>
#include <QTextLayout>

namespace Utils {

enum class HighlightingItemRole {
  LineNumber = Qt::UserRole,
  StartColumn,
  Length,
  Foreground,
  Background,
  User
};

class ORCA_UTILS_EXPORT HighlightingItemDelegate : public QItemDelegate {
public:
  HighlightingItemDelegate(int tabWidth, QObject *parent = nullptr);
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
  auto setTabWidth(int width) -> void;

private:
  auto drawLineNumber(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QModelIndex &index) const -> int;
  auto drawText(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QModelIndex &index) const -> void;
  using QItemDelegate::drawDisplay;
  auto drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QString &text, const QVector<QTextLayout::FormatRange> &format) const -> void;
  QString m_tabString;
};

} // namespace Utils
