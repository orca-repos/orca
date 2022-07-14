// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-highlight-scroll-bar-controller.hpp"

#include <QAbstractScrollArea>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>

using namespace Utils;

namespace Orca::Plugin::Core {

/*!
    \class Core::Highlight
    \inmodule Orca
    \internal
*/

/*!
    \class Core::HighlightScrollBarController
    \inmodule Orca
    \internal
*/

class HighlightScrollBarOverlay final : public QWidget {
public:
  explicit HighlightScrollBarOverlay(HighlightScrollBarController *scroll_bar_controller) : QWidget(scroll_bar_controller->scrollArea()), m_scroll_bar(scroll_bar_controller->scrollBar()), m_highlight_controller(scroll_bar_controller)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_scroll_bar->parentWidget()->installEventFilter(this);
    doResize();
    doMove();
    show();
  }

  auto doResize() -> void
  {
    resize(m_scroll_bar->size());
  }

  auto doMove() -> void
  {
    move(parentWidget()->mapFromGlobal(m_scroll_bar->mapToGlobal(m_scroll_bar->pos())));
  }

  auto scheduleUpdate() -> void;

protected:
  auto paintEvent(QPaintEvent *paint_event) -> void override;
  auto eventFilter(QObject *object, QEvent *event) -> bool override;

private:
  auto drawHighlights(QPainter *painter, int doc_start, int doc_size, double doc_size_to_handle_size_ratio, int handle_offset, const QRect &viewport) -> void;
  auto updateCache() -> void;
  auto overlayRect() const -> QRect;
  auto handleRect() const -> QRect;

  // line start to line end
  QMap<Highlight::Priority, QMap<Theme::Color, QMap<int, int>>> m_highlight_cache;
  QScrollBar *m_scroll_bar;
  HighlightScrollBarController *m_highlight_controller;
  bool m_is_cache_update_scheduled = true;
};

auto HighlightScrollBarOverlay::scheduleUpdate() -> void
{
  if (m_is_cache_update_scheduled)
    return;

  m_is_cache_update_scheduled = true;
  QMetaObject::invokeMethod(this, QOverload<>::of(&QWidget::update), Qt::QueuedConnection);
}

auto HighlightScrollBarOverlay::paintEvent(QPaintEvent *paint_event) -> void
{
  QWidget::paintEvent(paint_event);

  updateCache();

  if (m_highlight_cache.isEmpty())
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const auto &g_rect = overlayRect();
  const auto &h_rect = handleRect();

  constexpr auto margin_x = 3;
  constexpr auto margin_h = -2 * margin_x + 1;
  const auto above_handle_rect = QRect(g_rect.x() + margin_x, g_rect.y(), g_rect.width() + margin_h, h_rect.y() - g_rect.y());
  const auto handle_rect = QRect(g_rect.x() + margin_x, h_rect.y(), g_rect.width() + margin_h, h_rect.height());
  const auto below_handle_rect = QRect(g_rect.x() + margin_x, h_rect.y() + h_rect.height(), g_rect.width() + margin_h, g_rect.height() - h_rect.height() + g_rect.y() - h_rect.y());
  const auto above_value = m_scroll_bar->value();
  const auto below_value = m_scroll_bar->maximum() - m_scroll_bar->value();
  const auto size_doc_above = static_cast<int>(above_value * m_highlight_controller->lineHeight());
  const auto size_doc_below = static_cast<int>(below_value * m_highlight_controller->lineHeight());
  const auto size_doc_visible = static_cast<int>(m_highlight_controller->visibleRange());
  const auto scroll_bar_background_height = above_handle_rect.height() + below_handle_rect.height();
  const auto size_doc_invisible = size_doc_above + size_doc_below;
  const auto background_ratio = size_doc_invisible ? static_cast<double>(scroll_bar_background_height) / size_doc_invisible : 0;

  if (above_value) {
    drawHighlights(&painter, 0, size_doc_above, background_ratio, 0, above_handle_rect);
  }

  if (below_value) {
    // This is the hypothetical handle height if the handle would
    // be stretched using the background ratio.
    const auto handle_virtual_height = size_doc_visible * background_ratio;
    // Skip the doc above and visible part.
    const auto offset = qRound(above_handle_rect.height() + handle_virtual_height);
    drawHighlights(&painter, size_doc_above + size_doc_visible, size_doc_below, background_ratio, offset, below_handle_rect);
  }

  const auto handle_ratio = size_doc_visible ? static_cast<double>(handle_rect.height()) / size_doc_visible : 0;
  // This is the hypothetical handle position if the background would
  // be stretched using the handle ratio.
  const auto above_virtual_height = size_doc_above * handle_ratio;
  // This is the accurate handle position (double)
  const auto accurate_handle_pos = size_doc_above * background_ratio;
  // The correction between handle position (int) and accurate position (double)
  const auto correction = above_handle_rect.height() - accurate_handle_pos;
  // Skip the doc above and apply correction
  const auto offset = qRound(above_virtual_height + correction);

  drawHighlights(&painter, size_doc_above, size_doc_visible, handle_ratio, offset, handle_rect);
}

auto HighlightScrollBarOverlay::drawHighlights(QPainter *painter, const int doc_start, const int doc_size, const double doc_size_to_handle_size_ratio, const int handle_offset, const QRect &viewport) -> void
{
  if (doc_size <= 0)
    return;

  painter->save();
  painter->setClipRect(viewport);

  const auto line_height = m_highlight_controller->lineHeight();

  for (const auto &colors : qAsConst(m_highlight_cache)) {
    const auto it_color_end = colors.constEnd();
    for (auto it_color = colors.constBegin(); it_color != it_color_end; ++it_color) {
      const auto &color = orcaTheme()->color(it_color.key());
      const auto &positions = it_color.value();
      const auto it_pos_end = positions.constEnd();
      const auto first_pos = static_cast<int>(doc_start / line_height);

      auto it_pos = positions.upperBound(first_pos);
      if (it_pos != positions.constBegin())
        --it_pos;

      while (it_pos != it_pos_end) {
        const auto pos_start = it_pos.key() * line_height;
        const auto pos_end = (it_pos.value() + 1) * line_height;

        if (pos_end < doc_start) {
          ++it_pos;
          continue;
        }

        if (pos_start > doc_start + doc_size)
          break;

        const auto height = qMax(qRound((pos_end - pos_start) * doc_size_to_handle_size_ratio), 1);
        const auto top = qRound(pos_start * doc_size_to_handle_size_ratio) - handle_offset + viewport.y();
        const QRect rect(viewport.left(), top, viewport.width(), height);

        painter->fillRect(rect, color);
        ++it_pos;
      }
    }
  }
  painter->restore();
}

auto HighlightScrollBarOverlay::eventFilter(QObject *object, QEvent *event) -> bool
{
  switch (event->type()) {
  case QEvent::Move:
    doMove();
    break;
  case QEvent::Resize:
    doResize();
    break;
  case QEvent::ZOrderChange:
    raise();
    break;
  default:
    break;
  }
  return QWidget::eventFilter(object, event);
}

static auto insertPosition(QMap<int, int> *map, const int position) -> void
{
  auto it_next = map->upperBound(position);
  auto glued_with_prev = false;

  if (it_next != map->begin()) {
    const auto it_prev = std::prev(it_next);
    const auto key_start = it_prev.key();
    const auto key_end = it_prev.value();

    if (position >= key_start && position <= key_end)
      return; // pos is already included

    if (key_end + 1 == position) {
      // glue with prev
      (*it_prev)++;
      glued_with_prev = true;
    }
  }

  if (it_next != map->end() && it_next.key() == position + 1) {
    const auto key_end = it_next.value();
    it_next = map->erase(it_next);
    if (glued_with_prev) {
      // glue with prev and next
      const auto it_prev = std::prev(it_next);
      *it_prev = key_end;
    } else {
      // glue with next
      it_next = map->insert(it_next, position, key_end);
    }
    return; // glued
  }

  if (glued_with_prev)
    return; // glued

  map->insert(position, position);
}

auto HighlightScrollBarOverlay::updateCache() -> void
{
  if (!m_is_cache_update_scheduled)
    return;

  m_highlight_cache.clear();
  for (const auto highlights_for_id = m_highlight_controller->highlights(); const auto &highlights : highlights_for_id) {
    for (const auto &highlight : highlights) {
      auto &highlight_map = m_highlight_cache[highlight.priority][highlight.color];
      insertPosition(&highlight_map, highlight.position);
    }
  }
  m_is_cache_update_scheduled = false;
}

auto HighlightScrollBarOverlay::overlayRect() const -> QRect
{
  const auto opt = qt_qscrollbarStyleOption(m_scroll_bar);
  return m_scroll_bar->style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, m_scroll_bar);
}

auto HighlightScrollBarOverlay::handleRect() const -> QRect
{
  const auto opt = qt_qscrollbarStyleOption(m_scroll_bar);
  return m_scroll_bar->style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, m_scroll_bar);
}

Highlight::Highlight(Id category_, int position_, Theme::Color color_, Priority priority_) : category(category_), position(position_), color(color_), priority(priority_) {}

HighlightScrollBarController::~HighlightScrollBarController()
{
  delete m_overlay;
}

auto HighlightScrollBarController::scrollBar() const -> QScrollBar*
{
  if (m_scroll_area)
    return m_scroll_area->verticalScrollBar();

  return nullptr;
}

auto HighlightScrollBarController::scrollArea() const -> QAbstractScrollArea*
{
  return m_scroll_area;
}

auto HighlightScrollBarController::setScrollArea(QAbstractScrollArea *scroll_area) -> void
{
  if (m_scroll_area == scroll_area)
    return;

  if (m_overlay) {
    delete m_overlay;
    m_overlay = nullptr;
  }

  m_scroll_area = scroll_area;

  if (m_scroll_area) {
    m_overlay = new HighlightScrollBarOverlay(this);
    m_overlay->scheduleUpdate();
  }
}

auto HighlightScrollBarController::lineHeight() const -> double
{
  return m_line_height;
}

auto HighlightScrollBarController::setLineHeight(const double line_height) -> void
{
  m_line_height = line_height;
}

auto HighlightScrollBarController::visibleRange() const -> double
{
  return m_visible_range;
}

auto HighlightScrollBarController::setVisibleRange(const double visible_range) -> void
{
  m_visible_range = visible_range;
}

auto HighlightScrollBarController::margin() const -> double
{
  return m_margin;
}

auto HighlightScrollBarController::setMargin(const double margin) -> void
{
  m_margin = margin;
}

auto HighlightScrollBarController::highlights() const -> QHash<Id, QVector<Highlight>>
{
  return m_highlights;
}

auto HighlightScrollBarController::addHighlight(const Highlight highlight) -> void
{
  if (!m_overlay)
    return;

  m_highlights[highlight.category] << highlight;
  m_overlay->scheduleUpdate();
}

auto HighlightScrollBarController::removeHighlights(const Id category) -> void
{
  if (!m_overlay)
    return;

  m_highlights.remove(category);
  m_overlay->scheduleUpdate();
}

auto HighlightScrollBarController::removeAllHighlights() -> void
{
  if (!m_overlay)
    return;

  m_highlights.clear();
  m_overlay->scheduleUpdate();
}

} // namespace Orca::Plugin::Core
