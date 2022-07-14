// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"
#include "core-welcome-page-interface.hpp"

#include <utils/optional.hpp>

#include <QElapsedTimer>
#include <QListView>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

namespace Utils {
class FancyLineEdit;
}

namespace Orca::Plugin::Core {

constexpr int G_H_SPACING = 20;
constexpr int G_ITEM_GAP = 4;

CORE_EXPORT auto brandFont() -> QFont;
CORE_EXPORT auto panelBar(QWidget *parent = nullptr) -> QWidget*;

class CORE_EXPORT SearchBox final : public WelcomePageFrame {
public:
  explicit SearchBox(QWidget *parent);

  Utils::FancyLineEdit *m_line_edit = nullptr;
};

class CORE_EXPORT GridView final : public QListView {
public:
  explicit GridView(QWidget *parent);

protected:
  auto leaveEvent(QEvent *) -> void final;
};

using OptModelIndex = Utils::optional<QModelIndex>;

class CORE_EXPORT ListItem {
public:
  virtual ~ListItem() {}

  QString name;
  QString description;
  QString image_url;
  QStringList tags;
};

class CORE_EXPORT ListModel : public QAbstractListModel {
public:
  enum ListDataRole {
    ItemRole = Qt::UserRole,
    ItemImageRole,
    ItemTagsRole
  };

  explicit ListModel(QObject *parent);
  ~ListModel() override;

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int final;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  virtual auto fetchPixmapAndUpdatePixmapCache(const QString &url) const -> QPixmap = 0;

  static const QSize default_image_size;

protected:
  QList<ListItem*> m_items;
};

class CORE_EXPORT ListModelFilter : public QSortFilterProxyModel {
public:
  ListModelFilter(ListModel *source_model, QObject *parent);

  auto setSearchString(const QString &arg) -> void;

protected:
  virtual auto leaveFilterAcceptsRowBeforeFiltering(const ListItem *item, bool *early_exit_result) const -> bool;

private:
  auto filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool override;
  auto timerEvent(QTimerEvent *timer_event) -> void override;
  auto delayedUpdateFilter() -> void;

  QString m_search_string;
  QStringList m_filter_tags;
  QStringList m_filter_strings;
  int m_timer_id = 0;
};

class CORE_EXPORT ListItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  ListItemDelegate();

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;

  static constexpr int grid_item_gap = 3 * G_ITEM_GAP;
  static constexpr int grid_item_width = 240 + grid_item_gap;
  static constexpr int grid_item_height = grid_item_width;
  static constexpr int tags_separator_y = grid_item_height - grid_item_gap - 52;

signals:
  auto tagClicked(const QString &tag) -> void;

protected:
  auto editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) -> bool override;
  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override;
  virtual auto drawPixmapOverlay(const ListItem *item, QPainter *painter, const QStyleOptionViewItem &option, const QRect &currentPixmapRect) const -> void;
  virtual auto clickAction(const ListItem *item) const -> void;
  auto goon() const -> void;

  const QColor background_primary_color;
  const QColor background_secondary_color;
  const QColor foreground_primary_color;
  const QColor hover_color;
  const QColor text_color;

private:
  mutable QPersistentModelIndex m_previous_index;
  mutable QElapsedTimer m_start_time;
  mutable QPointer<QAbstractItemView> m_current_widget;
  mutable QVector<QPair<QString, QRect>> m_current_tag_rects;
  mutable QPixmap m_blurred_thumbnail;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_METATYPE(Orca::Plugin::Core::ListItem *)
