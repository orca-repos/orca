// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fancytabwidget.h"
#include "coreconstants.h"
#include "fancyactionbar.h"

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/styledbar.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QStackedLayout>
#include <QStatusBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolTip>

using namespace Utils;

namespace Core {
namespace Internal {

static const int kMenuButtonWidth = 16;

auto FancyTab::fadeIn() -> void
{
  m_animator.stop();
  m_animator.setDuration(80);
  m_animator.setEndValue(1);
  m_animator.start();
}

auto FancyTab::fadeOut() -> void
{
  m_animator.stop();
  m_animator.setDuration(160);
  m_animator.setEndValue(0);
  m_animator.start();
}

auto FancyTab::setFader(const qreal value) -> void
{
  m_fader = value;
  m_tabbar->update();
}

FancyTabBar::FancyTabBar(QWidget *parent) : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  setAttribute(Qt::WA_Hover, true);
  setFocusPolicy(Qt::NoFocus);
  setMouseTracking(true); // Needed for hover events
}

auto FancyTabBar::tabSizeHint(const bool minimum) const -> QSize
{
  if (m_icons_only) {
    return {Constants::MODEBAR_ICONSONLY_BUTTON_SIZE, Constants::MODEBAR_ICONSONLY_BUTTON_SIZE / (minimum ? 3 : 1)};
  }

  auto bold_font(font());
  bold_font.setPointSizeF(StyleHelper::sidebarFontSize());
  bold_font.setBold(true);

  const QFontMetrics fm(bold_font);
  constexpr auto spacing = 8;
  constexpr auto width = 60 + spacing + 2;
  auto max_labelwidth = 0;

  for (const auto tab : qAsConst(m_tabs)) {
    const auto width = fm.horizontalAdvance(tab->text);
    if (width > max_labelwidth)
      max_labelwidth = width;
  }

  const auto icon_height = minimum ? 0 : 32;
  return {qMax(width, max_labelwidth + 4), icon_height + spacing + fm.height()};
}

// Handle hover events for mouse fade ins
auto FancyTabBar::mouseMoveEvent(QMouseEvent *event) -> void
{
  auto new_hover = -1;
  for (auto i = 0; i < count(); ++i) {
    if (const auto area = tabRect(i); area.contains(event->pos())) {
      new_hover = i;
      break;
    }
  }

  if (new_hover == m_hover_index)
    return;

  if (validIndex(m_hover_index))
    m_tabs[m_hover_index]->fadeOut();

  m_hover_index = new_hover;

  if (validIndex(m_hover_index)) {
    m_tabs[m_hover_index]->fadeIn();
    m_hover_rect = tabRect(m_hover_index);
  }
}

auto FancyTabBar::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ToolTip) {
    if (validIndex(m_hover_index)) {
      if (const auto tt = tabToolTip(m_hover_index); !tt.isEmpty()) {
        QToolTip::showText(dynamic_cast<QHelpEvent*>(event)->globalPos(), tt, this);
        return true;
      }
    }
  }

  return QWidget::event(event);
}

// Resets hover animation on mouse enter
auto FancyTabBar::enterEvent(EnterEvent *event) -> void
{
  Q_UNUSED(event)
  m_hover_rect = QRect();
  m_hover_index = -1;
}

// Resets hover animation on mouse enter
auto FancyTabBar::leaveEvent(QEvent *event) -> void
{
  Q_UNUSED(event)
  m_hover_index = -1;
  m_hover_rect = QRect();

  for (const auto tab : qAsConst(m_tabs))
    tab->fadeOut();
}

auto FancyTabBar::sizeHint() const -> QSize
{
  const auto sh = tabSizeHint();
  return {sh.width(), sh.height() * static_cast<int>(m_tabs.count())};
}

auto FancyTabBar::minimumSizeHint() const -> QSize
{
  const auto sh = tabSizeHint(true);
  return {sh.width(), sh.height() * static_cast<int>(m_tabs.count())};
}

auto FancyTabBar::tabRect(int index) const -> QRect
{
  auto sh = tabSizeHint();

  if (sh.height() * m_tabs.count() > height())
    sh.setHeight(height() / static_cast<int>(m_tabs.count()));
  
  return {0, index * sh.height(), sh.width(), sh.height()};
}

auto FancyTabBar::mousePressEvent(QMouseEvent *event) -> void
{
  event->accept();
  for (auto index = 0; index < m_tabs.count(); ++index) {
    if (const auto rect = tabRect(index); rect.contains(event->pos())) {
      if (isTabEnabled(index)) {
        if (m_tabs.at(index)->has_menu && (!m_icons_only && rect.right() - event->pos().x() <= kMenuButtonWidth || event->button() == Qt::RightButton)) {
          // menu arrow clicked or right-click
          emit menuTriggered(index, event);
        } else {
          if (index != m_current_index) {
            emit currentAboutToChange(index);
            m_current_index = index;
            update();
            // update tab bar before showing widget
            QMetaObject::invokeMethod(this, [this]() {
              emit currentChanged(m_current_index);
            }, Qt::QueuedConnection);
          }
        }
      }
      break;
    }
  }
}

static auto paintSelectedTabBackground(QPainter *painter, const QRect &span_rect) -> void
{
  constexpr auto vertical_overlap = 2; // Grows up and down for the overlaps
  const auto dpr = static_cast<int>(painter->device()->devicePixelRatio());
  const QString cache_key = QLatin1String(Q_FUNC_INFO) + QString::number(span_rect.width()) + QLatin1Char('x') + QString::number(span_rect.height()) + QLatin1Char('@') + QString::number(dpr);
  QPixmap selection;

  if (!QPixmapCache::find(cache_key, &selection)) {
    selection = QPixmap(QSize(span_rect.width(), span_rect.height() + 2 * vertical_overlap) * dpr);
    selection.fill(Qt::transparent);
    selection.setDevicePixelRatio(dpr);

    QPainter p(&selection);
    p.translate(QPoint(0, vertical_overlap));
    const QRect rect(QPoint(), span_rect.size());
    const auto border_rect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);

    //background
    p.save();
    QLinearGradient grad(rect.topLeft(), rect.topRight());
    grad.setColorAt(0, QColor(255, 255, 255, 140));
    grad.setColorAt(1, QColor(255, 255, 255, 210));
    p.fillRect(rect, grad);
    p.restore();

    //shadows
    p.setPen(QColor(0, 0, 0, 110));
    p.drawLine(border_rect.topLeft() + QPointF(1, -1), border_rect.topRight() - QPointF(0, 1));
    p.drawLine(border_rect.bottomLeft(), border_rect.bottomRight());
    p.setPen(QColor(0, 0, 0, 40));
    p.drawLine(border_rect.topLeft(), border_rect.bottomLeft());

    //highlights
    p.setPen(QColor(255, 255, 255, 50));
    p.drawLine(border_rect.topLeft() + QPointF(0, -2), border_rect.topRight() - QPointF(0, 2));
    p.drawLine(border_rect.bottomLeft() + QPointF(0, 1), border_rect.bottomRight() + QPointF(0, 1));
    p.setPen(QColor(255, 255, 255, 40));
    p.drawLine(border_rect.topLeft() + QPointF(0, 0), border_rect.topRight());
    p.drawLine(border_rect.topRight() + QPointF(0, 1), border_rect.bottomRight() - QPointF(0, 1));
    p.drawLine(border_rect.bottomLeft() + QPointF(0, -1), border_rect.bottomRight() - QPointF(0, 1));
    p.end();

    QPixmapCache::insert(cache_key, selection);
  }
  painter->drawPixmap(span_rect.topLeft() + QPoint(0, -vertical_overlap), selection);
}

static auto paintIcon(QPainter *painter, const QRect &rect, const QIcon &icon, const bool enabled, const bool selected) -> void
{
  const auto icon_mode = enabled ? (selected ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;
  QRect icon_rect(0, 0, Constants::MODEBAR_ICON_SIZE, Constants::MODEBAR_ICON_SIZE);
  icon_rect.moveCenter(rect.center());
  icon_rect = icon_rect.intersected(rect);

  if (!enabled && !orcaTheme()->flag(Theme::FlatToolBars))
    painter->setOpacity(0.7);

  StyleHelper::drawIconWithShadow(icon, icon_rect, painter, icon_mode);

  if (selected && orcaTheme()->flag(Theme::FlatToolBars)) {
    painter->setOpacity(1.0);
    auto accent_rect = rect;
    accent_rect.setWidth(2);
    painter->fillRect(accent_rect, orcaTheme()->color(Theme::IconsBaseColor));
  }
}

static auto paintIconAndText(QPainter *painter, const QRect &rect, const QIcon &icon, const QString &text, const bool enabled, const bool selected) -> void
{
  auto bold_font(painter->font());
  bold_font.setPointSizeF(StyleHelper::sidebarFontSize());
  bold_font.setBold(true);
  painter->setFont(bold_font);

  const auto draw_icon = rect.height() > 36;

  if (draw_icon) {
    const auto text_height = painter->fontMetrics().boundingRect(rect, Qt::TextWordWrap, text).height();
    const auto tab_icon_rect(rect.adjusted(0, 4, 0, -text_height));
    const auto icon_mode = enabled ? (selected ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;

    QRect icon_rect(0, 0, Constants::MODEBAR_ICON_SIZE, Constants::MODEBAR_ICON_SIZE);
    icon_rect.moveCenter(tab_icon_rect.center());
    icon_rect = icon_rect.intersected(tab_icon_rect);

    if (!enabled && !orcaTheme()->flag(Theme::FlatToolBars))
      painter->setOpacity(0.7);

    StyleHelper::drawIconWithShadow(icon, icon_rect, painter, icon_mode);
  }

  painter->setOpacity(1.0); //FIXME: was 0.7 before?

  if (selected && orcaTheme()->flag(Theme::FlatToolBars)) {
    auto accent_rect = rect;
    accent_rect.setWidth(2);
    painter->fillRect(accent_rect, orcaTheme()->color(Theme::IconsBaseColor));
  }

  if (enabled) {
    painter->setPen(selected ? orcaTheme()->color(Theme::FancyTabWidgetEnabledSelectedTextColor) : orcaTheme()->color(Theme::FancyTabWidgetEnabledUnselectedTextColor));
  } else {
    painter->setPen(selected ? orcaTheme()->color(Theme::FancyTabWidgetDisabledSelectedTextColor) : orcaTheme()->color(Theme::FancyTabWidgetDisabledUnselectedTextColor));
  }

  painter->translate(0, -1);
  auto tab_text_rect(rect);
  tab_text_rect.translate(0, draw_icon ? -2 : 1);
  const auto text_flags = Qt::AlignCenter | (draw_icon ? Qt::AlignBottom : Qt::AlignVCenter) | Qt::TextWordWrap;
  painter->drawText(tab_text_rect, text_flags, text);
}

auto FancyTabBar::paintTab(QPainter *painter, const int tab_index) const -> void
{
  if (!validIndex(tab_index)) {
    qWarning("invalid index");
    return;
  }

  painter->save();

  const FancyTab *tab = m_tabs.at(tab_index);
  const auto rect = tabRect(tab_index);
  const auto selected = tab_index == m_current_index;
  const auto enabled = isTabEnabled(tab_index);

  if (selected) {
    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      // background color of a fancy tab that is active
      painter->fillRect(rect, orcaTheme()->color(Theme::FancyTabBarSelectedBackgroundColor));
    } else {
      paintSelectedTabBackground(painter, rect);
    }
  }

  if (const auto fader = tab->fader(); fader > 0 && !HostOsInfo::isMacHost() && !selected && enabled) {
    painter->save();
    painter->setOpacity(fader);

    if (orcaTheme()->flag(Theme::FlatToolBars))
      painter->fillRect(rect, orcaTheme()->color(Theme::FancyToolButtonHoverColor));
    else
      FancyToolButton::hoverOverlay(painter, rect);

    painter->restore();
  }

  if (m_icons_only)
    paintIcon(painter, rect, tab->icon, enabled, selected);
  else
    paintIconAndText(painter, rect, tab->icon, tab->text, enabled, selected);

  // menu arrow
  if (tab->has_menu && !m_icons_only) {
    QStyleOption opt;
    opt.initFrom(this);
    opt.rect = rect.adjusted(rect.width() - kMenuButtonWidth, 0, -8, 0);
    StyleHelper::drawArrow(QStyle::PE_IndicatorArrowRight, painter, &opt);
  }
  painter->restore();
}

auto FancyTabBar::setCurrentIndex(const int index) -> void
{
  if (isTabEnabled(index) && index != m_current_index) {
    emit currentAboutToChange(index);
    m_current_index = index;
    update();
    emit currentChanged(m_current_index);
  }
}

auto FancyTabBar::setIconsOnly(const bool icons_only) -> void
{
  m_icons_only = icons_only;
  updateGeometry();
}

auto FancyTabBar::setTabEnabled(const int index, const bool enable) -> void
{
  Q_ASSERT(index < m_tabs.size());
  Q_ASSERT(index >= 0);

  if (index < m_tabs.size() && index >= 0) {
    m_tabs[index]->enabled = enable;
    update(tabRect(index));
  }
}

auto FancyTabBar::isTabEnabled(const int index) const -> bool
{
  Q_ASSERT(index < m_tabs.size());
  Q_ASSERT(index >= 0);

  if (index < m_tabs.size() && index >= 0)
    return m_tabs[index]->enabled;

  return false;
}

class FancyColorButton final : public QWidget {
  Q_OBJECT public:
  explicit FancyColorButton(QWidget *parent = nullptr) : QWidget(parent)
  {
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  }

  auto mousePressEvent(QMouseEvent *ev) -> void override
  {
    emit clicked(ev->button(), ev->modifiers());
  }

  auto paintEvent(QPaintEvent *event) -> void override
  {
    QWidget::paintEvent(event);

    // Some Themes do not want highlights, shadows and borders in the toolbars.
    // But we definitely want a separator between FancyColorButton and FancyTabBar
    if (!orcaTheme()->flag(Theme::DrawToolBarHighlights) && !orcaTheme()->flag(Theme::DrawToolBarBorders)) {
      QPainter p(this);
      p.setPen(StyleHelper::toolBarBorderColor());
      const auto inner_rect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
      p.drawLine(inner_rect.bottomLeft(), inner_rect.bottomRight());
    }
  }

signals:
  auto clicked(Qt::MouseButton button, Qt::KeyboardModifiers modifiers) -> void;
};

FancyTabWidget::FancyTabWidget(QWidget *parent) : QWidget(parent)
{
  m_tab_bar = new FancyTabBar(this);
  m_tab_bar->setObjectName("ModeSelector"); // used for UI introduction

  m_selection_widget = new QWidget(this);
  const auto selection_layout = new QVBoxLayout;
  selection_layout->setSpacing(0);
  selection_layout->setContentsMargins(0, 0, 0, 0);

  const auto bar = new StyledBar;
  const auto layout = new QHBoxLayout(bar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  const auto fancy_button = new FancyColorButton(this);
  connect(fancy_button, &FancyColorButton::clicked, this, &FancyTabWidget::topAreaClicked);
  layout->addWidget(fancy_button);

  selection_layout->addWidget(bar);
  selection_layout->addStretch(1);
  m_selection_widget->setLayout(selection_layout);
  m_selection_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  m_corner_widget_container = new QWidget(this);
  m_corner_widget_container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  m_corner_widget_container->setAutoFillBackground(false);

  const auto corner_widget_layout = new QVBoxLayout;
  corner_widget_layout->setSpacing(0);
  corner_widget_layout->setContentsMargins(0, 0, 0, 0);
  corner_widget_layout->addStretch();
  m_corner_widget_container->setLayout(corner_widget_layout);

  selection_layout->addWidget(m_corner_widget_container, 0);

  m_modes_stack = new QStackedLayout;
  m_status_bar = new QStatusBar;
  m_status_bar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

  const auto vlayout = new QVBoxLayout;
  vlayout->setContentsMargins(0, 0, 0, 0);
  vlayout->setSpacing(0);
  vlayout->addLayout(m_modes_stack);
  vlayout->addWidget(m_status_bar);

  m_info_bar_display.setTarget(vlayout, 1);
  m_info_bar_display.setEdge(Qt::BottomEdge);

  const auto main_layout = new QHBoxLayout;
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(1);
  main_layout->addWidget(m_selection_widget);
  main_layout->addLayout(vlayout);
  setLayout(main_layout);

  connect(m_tab_bar, &FancyTabBar::currentAboutToChange, this, &FancyTabWidget::currentAboutToShow);
  connect(m_tab_bar, &FancyTabBar::currentChanged, this, &FancyTabWidget::showWidget);
  connect(m_tab_bar, &FancyTabBar::menuTriggered, this, &FancyTabWidget::menuTriggered);
}

auto FancyTabWidget::setSelectionWidgetVisible(const bool visible) const -> void
{
  m_selection_widget->setVisible(visible);
}

auto FancyTabWidget::isSelectionWidgetVisible() const -> bool
{
  return m_selection_widget->isVisible();
}

auto FancyTabWidget::insertTab(const int index, QWidget *tab, const QIcon &icon, const QString &label, const bool has_menu) const -> void
{
  m_modes_stack->insertWidget(index, tab);
  m_tab_bar->insertTab(index, icon, label, has_menu);
}

auto FancyTabWidget::removeTab(const int index) const -> void
{
  m_modes_stack->removeWidget(m_modes_stack->widget(index));
  m_tab_bar->removeTab(index);
}

auto FancyTabWidget::setBackgroundBrush(const QBrush &brush) const -> void
{
  QPalette pal;
  pal.setBrush(QPalette::Mid, brush);
  m_tab_bar->setPalette(pal);
  m_corner_widget_container->setPalette(pal);
}

auto FancyTabWidget::paintEvent(QPaintEvent *event) -> void
{
  Q_UNUSED(event)

  if (m_selection_widget->isVisible()) {
    QPainter painter(this);

    auto rect = m_selection_widget->rect().adjusted(0, 0, 1, 0);
    rect = QStyle::visualRect(layoutDirection(), geometry(), rect);
    const auto boder_rect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);

    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      painter.fillRect(rect, StyleHelper::baseColor());
      painter.setPen(StyleHelper::toolBarBorderColor());
      painter.drawLine(boder_rect.topRight(), boder_rect.bottomRight());
    } else {
      StyleHelper::verticalGradient(&painter, rect, rect);
      painter.setPen(StyleHelper::borderColor());
      painter.drawLine(boder_rect.topRight(), boder_rect.bottomRight());

      const auto light = StyleHelper::sidebarHighlight();
      painter.setPen(light);
      painter.drawLine(boder_rect.bottomLeft(), boder_rect.bottomRight());
    }
  }
}

auto FancyTabWidget::insertCornerWidget(const int pos, QWidget *widget) const -> void
{
  const auto layout = dynamic_cast<QVBoxLayout*>(m_corner_widget_container->layout());
  layout->insertWidget(pos, widget);
}

auto FancyTabWidget::cornerWidgetCount() const -> int
{
  return m_corner_widget_container->layout()->count();
}

auto FancyTabWidget::addCornerWidget(QWidget *widget) const -> void
{
  m_corner_widget_container->layout()->addWidget(widget);
}

auto FancyTabWidget::currentIndex() const -> int
{
  return m_tab_bar->currentIndex();
}

auto FancyTabWidget::statusBar() const -> QStatusBar*
{
  return m_status_bar;
}

auto FancyTabWidget::infoBar() -> InfoBar*
{
  if (!m_info_bar_display.infoBar())
    m_info_bar_display.setInfoBar(&m_info_bar);
  return &m_info_bar;
}

auto FancyTabWidget::setCurrentIndex(const int index) const -> void
{
  m_tab_bar->setCurrentIndex(index);
}

auto FancyTabWidget::showWidget(const int index) -> void
{
  m_modes_stack->setCurrentIndex(index);
  if (auto w = m_modes_stack->currentWidget(); QTC_GUARD(w)) {
    if (const auto focus_widget = w->focusWidget())
      w = focus_widget;
    w->setFocus();
  }
  emit currentChanged(index);
}

auto FancyTabWidget::setTabToolTip(const int index, const QString &tool_tip) const -> void
{
  m_tab_bar->setTabToolTip(index, tool_tip);
}

auto FancyTabWidget::setTabEnabled(const int index, const bool enable) const -> void
{
  m_tab_bar->setTabEnabled(index, enable);
}

auto FancyTabWidget::isTabEnabled(const int index) const -> bool
{
  return m_tab_bar->isTabEnabled(index);
}

auto FancyTabWidget::setIconsOnly(const bool icons_only) const -> void
{
  m_tab_bar->setIconsOnly(icons_only);
}

} // namespace Internal
} // namespace Core

#include "fancytabwidget.moc"
