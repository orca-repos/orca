// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fancyactionbar.hpp"
#include "coreconstants.hpp"

#include <utils/hostosinfo.hpp>
#include <utils/stringutils.hpp>
#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>
#include <utils/tooltip/tooltip.hpp>

#include <QAction>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QPropertyAnimation>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

using namespace Utils;

namespace Core {
namespace Internal {

FancyToolButton::FancyToolButton(QAction *action, QWidget *parent) : QToolButton(parent)
{
  setDefaultAction(action);
  connect(action, &QAction::changed, this, &FancyToolButton::actionChanged);
  actionChanged();
  setAttribute(Qt::WA_Hover, true);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

auto FancyToolButton::event(QEvent *e) -> bool
{
  switch (e->type()) {
  case QEvent::Enter: {
    const auto animation = new QPropertyAnimation(this, "fader");
    animation->setDuration(125);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  case QEvent::Leave: {
    const auto animation = new QPropertyAnimation(this, "fader");
    animation->setDuration(125);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  case QEvent::ToolTip: {
    const auto he = dynamic_cast<QHelpEvent*>(e);
    ToolTip::show(mapToGlobal(he->pos()), toolTip(), this);
    return true;
  }
  default:
    break;
  }
  return QToolButton::event(e);
}

static auto findSplitPos(const QString &text, const QFontMetrics &font_metrics, const qreal available_width) -> int
{
  if (text.length() == 0)
    return -1;

  auto split_pos = -1;
  auto first_whitespace = static_cast<int>(text.length());
  do {
    // search backwards for ranges of whitespaces
    // search first whitespace (backwards)
    auto last_whitespace = first_whitespace - 1; // start before last blob (or at end of text)
    while (last_whitespace >= 0) {
      if (text.at(last_whitespace).isSpace())
        break;
      --last_whitespace;
    }

    // search last whitespace (backwards)
    first_whitespace = last_whitespace;
    while (first_whitespace > 0) {
      if (!text.at(first_whitespace - 1).isSpace())
        break;
      --first_whitespace;
    }

    // if the text after the whitespace range fits into the available width, that's a great
    // position for splitting, but look if we can fit more
    if (first_whitespace != -1) {
      if (font_metrics.horizontalAdvance(text.mid(last_whitespace + 1)) <= available_width)
        split_pos = last_whitespace + 1;
      else
        break;
    }
  } while (first_whitespace > 0 && font_metrics.horizontalAdvance(text.left(first_whitespace)) > available_width);

  return split_pos;
}

static auto splitInTwoLines(const QString &text, const QFontMetrics &font_metrics, const qreal available_width) -> QVector<QString>
{
  // split in two lines.
  // this looks if full words can be split off at the end of the string,
  // to put them in the second line. First line is drawn with ellipsis,
  // second line gets ellipsis if it couldn't split off full words.
  QVector<QString> split_lines(2);

  // check if we could split at white space at all
  if (const auto split_pos = findSplitPos(text, font_metrics, available_width); split_pos < 0) {
    split_lines[0] = font_metrics.elidedText(text, Qt::ElideRight, int(available_width));
    const auto common = commonPrefix(QStringList({split_lines[0], text}));
    split_lines[1] = text.mid(common.length());
    // elide the second line even if it fits, since it is cut off in mid-word
    while (font_metrics.horizontalAdvance(QChar(0x2026) /*'...'*/ + split_lines[1]) > available_width && split_lines[1].length() > 3
      /*keep at least three original characters (should not happen)*/) {
      split_lines[1].remove(0, 1);
    }
    split_lines[1] = QChar(0x2026) /*'...'*/ + split_lines[1];
  } else {
    split_lines[0] = font_metrics.elidedText(text.left(split_pos).trimmed(), Qt::ElideRight, int(available_width));
    split_lines[1] = text.mid(split_pos);
  }

  return split_lines;
}

auto FancyToolButton::paintEvent(QPaintEvent *event) -> void
{
  Q_UNUSED(event)
  QPainter painter(this);

  // draw borders
  if (!HostOsInfo::isMacHost() && m_fader > 0 && isEnabled() && !isDown() && !isChecked()) {
    painter.save();
    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      const auto hover_color = orcaTheme()->color(Theme::FancyToolButtonHoverColor);
      auto faded_hover_color = hover_color;
      faded_hover_color.setAlpha(static_cast<int>(m_fader * hover_color.alpha()));
      painter.fillRect(rect(), faded_hover_color);
    } else {
      painter.setOpacity(m_fader);
      hoverOverlay(&painter, rect());
    }
    painter.restore();
  } else if (isDown() || isChecked()) {
    painter.save();
    const auto selected_color = orcaTheme()->color(Theme::FancyToolButtonSelectedColor);
    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      painter.fillRect(rect(), selected_color);
    } else {
      QLinearGradient grad(rect().topLeft(), rect().topRight());
      grad.setColorAt(0, Qt::transparent);
      grad.setColorAt(0.5, selected_color);
      grad.setColorAt(1, Qt::transparent);
      painter.fillRect(rect(), grad);
      painter.setPen(QPen(grad, 1.0));
      const auto border_rect_f(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
      painter.drawLine(border_rect_f.topLeft(), border_rect_f.topRight());
      painter.drawLine(border_rect_f.topLeft(), border_rect_f.topRight());
      painter.drawLine(border_rect_f.topLeft() + QPointF(0, 1), border_rect_f.topRight() + QPointF(0, 1));
      painter.drawLine(border_rect_f.bottomLeft(), border_rect_f.bottomRight());
      painter.drawLine(border_rect_f.bottomLeft(), border_rect_f.bottomRight());
    }
    painter.restore();
  }

  const auto icon_mode = isEnabled() ? ((isDown() || isChecked()) ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;
  QRect icon_rect(0, 0, Constants::MODEBAR_ICON_SIZE, Constants::MODEBAR_ICON_SIZE);
  const auto is_titled_action = defaultAction() && defaultAction()->property("titledAction").toBool();

  // draw popup texts
  if (is_titled_action && !m_icons_only) {
    auto normal_font(painter.font());
    auto center_rect = rect();
    normal_font.setPointSizeF(StyleHelper::sidebarFontSize());
    auto bold_font(normal_font);
    bold_font.setBold(true);
    const QFontMetrics fm(normal_font);
    const QFontMetrics bold_fm(bold_font);
    const auto line_height = bold_fm.height();
    constexpr int text_flags = Qt::AlignVCenter | Qt::AlignHCenter;
    const auto project_name = defaultAction()->property("heading").toString();

    if (!project_name.isNull())
      center_rect.adjust(0, line_height + 4, 0, 0);

    center_rect.adjust(0, 0, 0, -line_height * 2 - 4);
    icon_rect.moveCenter(center_rect.center());
    StyleHelper::drawIconWithShadow(icon(), icon_rect, &painter, icon_mode);
    painter.setFont(normal_font);

    auto text_offset = center_rect.center() - QPoint(icon_rect.width() / 2, icon_rect.height() / 2);
    text_offset = text_offset - QPoint(0, line_height + 3);
    const QRectF r(0, text_offset.y(), rect().width(), line_height);
    painter.setPen(orcaTheme()->color(isEnabled() ? Theme::PanelTextColorLight : Theme::IconsDisabledColor));

    // draw project name
    constexpr auto margin = 6;
    const auto available_width = r.width() - margin;
    const auto ellided_project_name = fm.elidedText(project_name, Qt::ElideMiddle, static_cast<int>(available_width));
    painter.drawText(r, text_flags, ellided_project_name);

    // draw build configuration name
    text_offset = icon_rect.center() + QPoint(icon_rect.width() / 2, icon_rect.height() / 2);
    QRectF build_config_rect[2];
    build_config_rect[0] = QRectF(0, text_offset.y() + 4, rect().width(), line_height);
    build_config_rect[1] = QRectF(0, text_offset.y() + 4 + line_height, rect().width(), line_height);
    painter.setFont(bold_font);
    QVector<QString> split_build_configuration(2);

    if (const auto build_configuration = defaultAction()->property("subtitle").toString(); bold_fm.horizontalAdvance(build_configuration) <= available_width)
      // text fits in one line
      split_build_configuration[0] = build_configuration;
    else
      split_build_configuration = splitInTwoLines(build_configuration, bold_fm, available_width);

    // draw the two text lines for the build configuration
    painter.setPen(orcaTheme()->color(isEnabled()
                                           // Intentionally using the "Unselected" colors,
                                           // because the text color won't change in the pressed
                                           // state as they would do on the mode buttons.
                                           ? Theme::FancyTabWidgetEnabledUnselectedTextColor
                                           : Theme::FancyTabWidgetDisabledUnselectedTextColor));

    for (auto i = 0; i < 2; ++i) {
      const auto &build_config_text = split_build_configuration[i];
      if (build_config_text.isEmpty())
        continue;
      painter.drawText(build_config_rect[i], text_flags, build_config_text);
    }
  } else {
    icon_rect.moveCenter(rect().center());
    StyleHelper::drawIconWithShadow(icon(), icon_rect, &painter, icon_mode);
  }

  // pop up arrow next to icon
  if (is_titled_action && isEnabled() && !icon().isNull()) {
    QStyleOption opt;
    opt.initFrom(this);
    opt.rect = rect().adjusted(rect().width() - (m_icons_only ? 6 : 16), 0, -(m_icons_only ? 0 : 8), 0);
    StyleHelper::drawArrow(QStyle::PE_IndicatorArrowRight, &painter, &opt);
  }
}

auto FancyActionBar::paintEvent(QPaintEvent *event) -> void
{
  QPainter painter(this);
  const auto border_rect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  if (orcaTheme()->flag(Theme::FlatToolBars)) {
    // this paints the background of the bottom portion of the
    // left tab bar
    painter.fillRect(event->rect(), StyleHelper::baseColor());
    painter.setPen(orcaTheme()->color(Theme::FancyToolBarSeparatorColor));
    painter.drawLine(border_rect.topLeft(), border_rect.topRight());
  } else {
    painter.setPen(StyleHelper::sidebarShadow());
    painter.drawLine(border_rect.topLeft(), border_rect.topRight());
    painter.setPen(StyleHelper::sidebarHighlight());
    painter.drawLine(border_rect.topLeft() + QPointF(1, 1), border_rect.topRight() + QPointF(0, 1));
  }
}

auto FancyToolButton::sizeHint() const -> QSize
{
  if (m_icons_only) {
    return {Constants::MODEBAR_ICONSONLY_BUTTON_SIZE, Constants::MODEBAR_ICONSONLY_BUTTON_SIZE};
  }

  QSizeF button_size = iconSize().expandedTo(QSize(64, 38));

  if (defaultAction() && defaultAction()->property("titledAction").toBool()) {
    auto bold_font(font());
    bold_font.setPointSizeF(StyleHelper::sidebarFontSize());
    bold_font.setBold(true);
    const QFontMetrics fm(bold_font);
    const qreal line_height = fm.height();
    const auto project_name = defaultAction()->property("heading").toString();
    button_size += QSizeF(0, 10);
    if (!project_name.isEmpty())
      button_size += QSizeF(0, line_height + 2);
    button_size += QSizeF(0, line_height * 2 + 2);
  }

  return button_size.toSize();
}

auto FancyToolButton::minimumSizeHint() const -> QSize
{
  return {8, 8};
}

auto FancyToolButton::setIconsOnly(const bool icons_only) -> void
{
  m_icons_only = icons_only;
  updateGeometry();
}

auto FancyToolButton::hoverOverlay(QPainter *painter, const QRect &span_rect) -> void
{
  const auto logical_size = span_rect.size();
  const QString cache_key = QLatin1String(Q_FUNC_INFO) + QString::number(logical_size.width()) + QLatin1Char('x') + QString::number(logical_size.height());
  QPixmap overlay;

  if (!QPixmapCache::find(cache_key, &overlay)) {
    const auto dpr = static_cast<int>(painter->device()->devicePixelRatio());

    overlay = QPixmap(logical_size * dpr);
    overlay.fill(Qt::transparent);
    overlay.setDevicePixelRatio(dpr);

    const auto hover_color = orcaTheme()->color(Theme::FancyToolButtonHoverColor);
    const QRect rect(QPoint(), logical_size);
    const auto border_rect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);

    QLinearGradient grad(rect.topLeft(), rect.topRight());
    grad.setColorAt(0, Qt::transparent);
    grad.setColorAt(0.5, hover_color);
    grad.setColorAt(1, Qt::transparent);

    QPainter p(&overlay);
    p.fillRect(rect, grad);
    p.setPen(QPen(grad, 1.0));
    p.drawLine(border_rect.topLeft(), border_rect.topRight());
    p.drawLine(border_rect.bottomLeft(), border_rect.bottomRight());
    p.end();

    QPixmapCache::insert(cache_key, overlay);
  }
  painter->drawPixmap(span_rect.topLeft(), overlay);
}

auto FancyToolButton::actionChanged() -> void
{
  // the default action changed in some way, e.g. it might got hidden
  // since we inherit a tool button we won't get invisible, so do this here
  if (const auto action = defaultAction())
    setVisible(action->isVisible());
}

FancyActionBar::FancyActionBar(QWidget *parent) : QWidget(parent)
{
  setObjectName("actionbar");
  m_actions_layout = new QVBoxLayout;
  m_actions_layout->setContentsMargins(0, 0, 0, 0);
  m_actions_layout->setSpacing(0);
  setLayout(m_actions_layout);
  setContentsMargins(0, 2, 0, 8);
}

auto FancyActionBar::addProjectSelector(QAction *action) -> void
{
  insertAction(0, action);
}

auto FancyActionBar::insertAction(const int index, QAction *action) -> void
{
  auto *button = new FancyToolButton(action, this);

  if (!action->objectName().isEmpty())
    button->setObjectName(action->objectName() + ".Button"); // used for UI introduction

  button->setIconsOnly(m_icons_only);
  m_actions_layout->insertWidget(index, button);
}

auto FancyActionBar::actionsLayout() const -> QLayout*
{
  return m_actions_layout;
}

auto FancyActionBar::minimumSizeHint() const -> QSize
{
  return sizeHint();
}

auto FancyActionBar::setIconsOnly(const bool icons_only) -> void
{
  m_icons_only = icons_only;

  for (auto i = 0, c = m_actions_layout->count(); i < c; ++i) {
    if (auto *button = qobject_cast<FancyToolButton*>(m_actions_layout->itemAt(i)->widget()))
      button->setIconsOnly(icons_only);
  }

  setContentsMargins(0, icons_only ? 7 : 2, 0, icons_only ? 2 : 8);
}

} // namespace Internal
} // namespace Core
