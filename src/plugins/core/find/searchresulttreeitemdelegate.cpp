// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchresulttreeitemdelegate.h"
#include "searchresulttreeitemroles.h"

#include <QPainter>
#include <QApplication>
#include <QModelIndex>

using namespace Core::Internal;

SearchResultTreeItemDelegate::SearchResultTreeItemDelegate(int tab_width, QObject *parent) : QItemDelegate(parent)
{
  setTabWidth(tab_width);
}

constexpr int line_number_area_horizontal_padding = 4;
constexpr int minimum_line_number_digits = 6;

static auto lineNumberInfo(const QStyleOptionViewItem &option, const QModelIndex &index) -> std::pair<int, QString>
{
  const auto line_number = index.data(ItemDataRoles::ResultBeginLineNumberRole).toInt();

  if (line_number < 1)
    return {0, {}};

  const auto line_number_text = QString::number(line_number);
  const auto line_number_digits = qMax(minimum_line_number_digits, line_number_text.count());
  const auto font_width = option.fontMetrics.horizontalAdvance(QString(line_number_digits, QLatin1Char('0')));
  const QStyle *style = option.widget ? option.widget->style() : QApplication::style();

  return {line_number_area_horizontal_padding + font_width + line_number_area_horizontal_padding + style->pixelMetric(QStyle::PM_FocusFrameHMargin), line_number_text};
}

static auto itemText(const QModelIndex &index) -> QString
{
  auto text = index.data(Qt::DisplayRole).toString();

  // show number of subresults in displayString
  if (index.model()->hasChildren(index)) {
    return text + QLatin1String(" (") + QString::number(index.model()->rowCount(index)) + QLatin1Char(')');
  }

  return text;
}

auto SearchResultTreeItemDelegate::getLayoutInfo(const QStyleOptionViewItem &option, const QModelIndex &index) const -> LayoutInfo
{
  LayoutInfo info;
  info.option = setOptions(index, option);

  // check mark
  const bool checkable = index.model()->flags(index) & Qt::ItemIsUserCheckable;
  info.check_state = Qt::Unchecked;
  if (checkable) {
    auto check_state_data = index.data(Qt::CheckStateRole);
    info.check_state = static_cast<Qt::CheckState>(check_state_data.toInt());
    info.check_rect = doCheck(info.option, info.option.rect, check_state_data);
  }

  // icon
  info.icon = index.data(ItemDataRoles::ResultIconRole).value<QIcon>();
  if (!info.icon.isNull()) {
    static constexpr auto icon_size = 16;
    const auto size = info.icon.actualSize(QSize(icon_size, icon_size));
    info.pixmap_rect = QRect(0, 0, size.width(), size.height());
  }

  // text
  info.text_rect = info.option.rect.adjusted(0, 0, info.check_rect.width() + info.pixmap_rect.width(), 0);

  // do basic layout
  doLayout(info.option, &info.check_rect, &info.pixmap_rect, &info.text_rect, false);

  // adapt for line numbers
  const auto line_number_width = lineNumberInfo(info.option, index).first;

  info.line_number_rect = info.text_rect;
  info.line_number_rect.setWidth(line_number_width);
  info.text_rect.adjust(line_number_width, 0, 0, 0);

  return info;
}

auto SearchResultTreeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option_view_item, const QModelIndex &index) const -> void
{
  painter->save();

  const auto [checkRect, pixmapRect, textRect, lineNumberRect, icon, checkState, option] = getLayoutInfo(option_view_item, index);

  painter->setFont(option.font);

  drawBackground(painter, option, index);

  // ---- draw the items
  // icon
  if (!icon.isNull())
    icon.paint(painter, pixmapRect, option.decorationAlignment);

  // line numbers
  drawLineNumber(painter, option, lineNumberRect, index);

  // text and focus/selection
  drawText(painter, option, textRect, index);
  QItemDelegate::drawFocus(painter, option, option.rect);

  // check mark
  if (checkRect.isValid())
    QItemDelegate::drawCheck(painter, option, checkRect, checkState);

  painter->restore();
}

auto SearchResultTreeItemDelegate::setTabWidth(const int width) -> void
{
  m_tab_string = QString(width, QLatin1Char(' '));
}

auto SearchResultTreeItemDelegate::sizeHint(const QStyleOptionViewItem &option_view_item, const QModelIndex &index) const -> QSize
{
  const auto [checkRect, pixmapRect, textRect, lineNumberRect, icon, checkState, option] = getLayoutInfo(option_view_item, index);
  const auto height = index.data(Qt::SizeHintRole).value<QSize>().height();

  // get text width, see QItemDelegatePrivate::displayRect
  const auto text = itemText(index).replace('\t', m_tab_string);
  const QRect text_max_rect(0, 0, INT_MAX / 256, height);
  const auto text_layout_rect = textRectangle(nullptr, text_max_rect, option.font, text);
  const QRect text_rect(textRect.x(), textRect.y(), text_layout_rect.width(), height);
  const auto layout_rect = checkRect | pixmapRect | lineNumberRect | text_rect;

  return QSize(layout_rect.x(), layout_rect.y()) + layout_rect.size();
}

// returns the width of the line number area
auto SearchResultTreeItemDelegate::drawLineNumber(QPainter *painter, const QStyleOptionViewItem &option_view_item, const QRect &rect, const QModelIndex &index) const -> int
{
  const bool is_selected = option_view_item.state & QStyle::State_Selected;
  const auto [fst, snd] = lineNumberInfo(option_view_item, index);

  if (fst == 0)
    return 0;

  auto line_number_area_rect(rect);
  line_number_area_rect.setWidth(fst);

  auto cg = QPalette::Normal;

  if (!(option_view_item.state & QStyle::State_Active))
    cg = QPalette::Inactive;
  else if (!(option_view_item.state & QStyle::State_Enabled))
    cg = QPalette::Disabled;

  painter->fillRect(line_number_area_rect, QBrush(is_selected ? option_view_item.palette.brush(cg, QPalette::Highlight) : option_view_item.palette.color(cg, QPalette::Base).darker(111)));

  auto opt = option_view_item;
  opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
  opt.palette.setColor(cg, QPalette::Text, Qt::darkGray);

  const QStyle *style = QApplication::style();
  const auto text_margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, nullptr) + 1;
  const auto row_rect = line_number_area_rect.adjusted(-text_margin, 0, text_margin - line_number_area_horizontal_padding, 0);

  QItemDelegate::drawDisplay(painter, opt, row_rect, snd);
  return fst;
}

auto SearchResultTreeItemDelegate::drawText(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QModelIndex &index) const -> void
{
  const auto text = itemText(index);
  const auto search_term_start = index.model()->data(index, ItemDataRoles::ResultBeginColumnNumberRole).toInt();
  auto search_term_length = index.model()->data(index, ItemDataRoles::SearchTermLengthRole).toInt();

  if (search_term_start < 0 || search_term_start >= text.length() || search_term_length < 1) {
    QItemDelegate::drawDisplay(painter, option, rect, QString(text).replace(QLatin1Char('\t'), m_tab_string));
    return;
  }

  // clip searchTermLength to end of line
  search_term_length = static_cast<int>(qMin(search_term_length, text.length() - search_term_start));

  const auto text_margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
  const auto text_before = text.left(search_term_start).replace(QLatin1Char('\t'), m_tab_string);
  const auto text_highlight = text.mid(search_term_start, search_term_length).replace(QLatin1Char('\t'), m_tab_string);
  const auto text_after = text.mid(search_term_start + search_term_length).replace(QLatin1Char('\t'), m_tab_string);

  auto search_term_start_pixels = option.fontMetrics.horizontalAdvance(text_before);
  auto search_term_length_pixels = option.fontMetrics.horizontalAdvance(text_highlight);
  
  // rects
  auto before_highlight_rect(rect);
  before_highlight_rect.setRight(before_highlight_rect.left() + search_term_start_pixels);

  auto result_highlight_rect(rect);
  result_highlight_rect.setLeft(before_highlight_rect.right());
  result_highlight_rect.setRight(result_highlight_rect.left() + search_term_length_pixels);

  auto after_highlight_rect(rect);
  after_highlight_rect.setLeft(result_highlight_rect.right());

  // paint all highlight backgrounds
  // qitemdelegate has problems with painting background when highlighted
  // (highlighted background at wrong position because text is offset with textMargin)
  // so we duplicate a lot here, see qitemdelegate for reference
  bool is_selected = option.state & QStyle::State_Selected;
  auto cg = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;

  if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
    cg = QPalette::Inactive;

  auto base_option = option;
  base_option.state &= ~QStyle::State_Selected;

  if (is_selected) {
    painter->fillRect(before_highlight_rect.adjusted(text_margin, 0, text_margin, 0), option.palette.brush(cg, QPalette::Highlight));
    painter->fillRect(after_highlight_rect.adjusted(text_margin, 0, text_margin, 0), option.palette.brush(cg, QPalette::Highlight));
  }

  const auto highlight_background = index.model()->data(index, ItemDataRoles::ResultHighlightBackgroundColor).value<QColor>();
  painter->fillRect(result_highlight_rect.adjusted(text_margin, 0, text_margin - 1, 0), QBrush(highlight_background));

  // Text before the highlighting
  auto no_highlight_opt = base_option;
  no_highlight_opt.rect = before_highlight_rect;
  no_highlight_opt.textElideMode = Qt::ElideNone;

  if (is_selected)
    no_highlight_opt.palette.setColor(QPalette::Text, no_highlight_opt.palette.color(cg, QPalette::HighlightedText));
  QItemDelegate::drawDisplay(painter, no_highlight_opt, before_highlight_rect, text_before);

  // Highlight text
  auto highlightOpt = no_highlight_opt;
  const auto highlightForeground = index.model()->data(index, ItemDataRoles::ResultHighlightForegroundColor).value<QColor>();
  highlightOpt.palette.setColor(QPalette::Text, highlightForeground);
  QItemDelegate::drawDisplay(painter, highlightOpt, result_highlight_rect, text_highlight);

  // Text after the Highlight
  no_highlight_opt.rect = after_highlight_rect;
  QItemDelegate::drawDisplay(painter, no_highlight_opt, after_highlight_rect, text_after);
}
