// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "progressbar.h"

#include <utils/stylehelper.h>
#include <utils/theme/theme.h>

#include <QPropertyAnimation>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QMouseEvent>

namespace Core {
namespace Internal {

static constexpr int g_progressbar_height = 13;
static constexpr int g_cancelbutton_width = 16;
static constexpr int g_separator_height = 2;

ProgressBar::ProgressBar(QWidget *parent) : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  setMouseTracking(true);
}

auto ProgressBar::event(QEvent *e) -> bool
{
  switch (e->type()) {
  case QEvent::Enter: {
    const auto animation = new QPropertyAnimation(this, "cancelButtonFader");
    animation->setDuration(125);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  case QEvent::Leave: {
    const auto animation = new QPropertyAnimation(this, "cancelButtonFader");
    animation->setDuration(225);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  default:
    return QWidget::event(e);
  }
  return false;
}

auto ProgressBar::reset() -> void
{
  m_value = m_minimum;
  update();
}

auto ProgressBar::setRange(const int minimum, const int maximum) -> void
{
  m_minimum = minimum;
  m_maximum = maximum;

  if (m_value < m_minimum || m_value > m_maximum)
    m_value = m_minimum;

  update();
}

auto ProgressBar::setValue(const int value) -> void
{
  if (m_value == value || m_value < m_minimum || m_value > m_maximum) {
    return;
  }

  m_value = value;
  update();
}

auto ProgressBar::setFinished(const bool b) -> void
{
  if (b == m_finished)
    return;

  m_finished = b;
  update();
}

auto ProgressBar::title() const -> QString
{
  return m_title;
}

auto ProgressBar::hasError() const -> bool
{
  return m_error;
}

auto ProgressBar::setTitle(const QString &title) -> void
{
  m_title = title;
  updateGeometry();
  update();
}

auto ProgressBar::setTitleVisible(const bool visible) -> void
{
  if (m_title_visible == visible)
    return;

  m_title_visible = visible;
  updateGeometry();
  update();
}

auto ProgressBar::isTitleVisible() const -> bool
{
  return m_title_visible;
}

auto ProgressBar::setSubtitle(const QString &subtitle) -> void
{
  m_subtitle = subtitle;
  updateGeometry();
  update();
}

auto ProgressBar::subtitle() const -> QString
{
  return m_subtitle;
}

auto ProgressBar::setSeparatorVisible(const bool visible) -> void
{
  if (m_separator_visible == visible)
    return;

  m_separator_visible = visible;
  update();
}

auto ProgressBar::isSeparatorVisible() const -> bool
{
  return m_separator_visible;
}

auto ProgressBar::setCancelEnabled(const bool enabled) -> void
{
  if (m_cancel_enabled == enabled)
    return;

  m_cancel_enabled = enabled;
  update();
}

auto ProgressBar::isCancelEnabled() const -> bool
{
  return m_cancel_enabled;
}

auto ProgressBar::setError(const bool on) -> void
{
  m_error = on;
  update();
}

auto ProgressBar::sizeHint() const -> QSize
{
  auto width = 50;
  auto height = g_progressbar_height + 5;

  if (m_title_visible) {
    const QFontMetrics fm(titleFont());
    width = qMax(width, fm.horizontalAdvance(m_title) + 16);
    height += fm.height() + 5;
    if (!m_subtitle.isEmpty()) {
      width = qMax(width, fm.horizontalAdvance(m_subtitle) + 16);
      height += fm.height() + 5;
    }
  }

  if (m_separator_visible)
    height += g_separator_height;

  return {width, height};
}

namespace {
constexpr int indent = 6;
}

auto ProgressBar::mousePressEvent(QMouseEvent *event) -> void
{
  if (m_cancel_enabled) {
    if (event->modifiers() == Qt::NoModifier && m_cancel_rect.contains(event->pos())) {
      event->accept();
      emit clicked();
      return;
    }
  }

  QWidget::mousePressEvent(event);
}

auto ProgressBar::titleFont() const -> QFont
{
  auto bold_font(font());
  bold_font.setPointSizeF(Utils::StyleHelper::sidebarFontSize());
  bold_font.setBold(true);
  return bold_font;
}

auto ProgressBar::mouseMoveEvent(QMouseEvent *) -> void
{
  update();
}

auto ProgressBar::paintEvent(QPaintEvent *) -> void
{
  // TODO move font into Utils::StyleHelper
  // TODO use Utils::StyleHelper white

  double range = maximum() - minimum();
  auto percent = 0.;

  if (!qFuzzyIsNull(range))
    percent = qBound(0., (value() - minimum()) / range, 1.);

  if (finished())
    percent = 1;

  QPainter p(this);
  const auto fnt(titleFont());
  const QFontMetrics fm(fnt);
  const auto title_height = m_title_visible ? fm.height() + 5 : 4;
  const auto separator_height = m_separator_visible ? g_separator_height : 0;

  if (m_separator_visible) {
    auto inner_rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Utils::StyleHelper::sidebarShadow());
    p.drawLine(inner_rect.topLeft(), inner_rect.topRight());

    if (Utils::orcaTheme()->flag(Utils::Theme::DrawToolBarHighlights)) {
      p.setPen(Utils::StyleHelper::sidebarHighlight());
      p.drawLine(inner_rect.topLeft() + QPointF(1, 1), inner_rect.topRight() + QPointF(0, 1));
    }
  }

  constexpr auto progress_height = g_progressbar_height + (g_progressbar_height % 2 + 1) % 2; // make odd
  const auto progress_y = title_height + g_separator_height;

  if (m_title_visible) {
    constexpr int alignment = Qt::AlignHCenter;
    const auto text_space = rect().width() - 8;
    // If there is not enough room when centered, we left align and
    // elide the text
    const auto elidedtitle = fm.elidedText(m_title, Qt::ElideRight, text_space);
    auto text_rect = rect().adjusted(3, g_separator_height - 1, -3, 0);
    text_rect.setHeight(fm.height() + 4);

    p.setFont(fnt);
    p.setPen(Utils::orcaTheme()->color(Utils::Theme::ProgressBarTitleColor));
    p.drawText(text_rect, alignment | Qt::AlignBottom, elidedtitle);

    if (!m_subtitle.isEmpty()) {
      const auto elidedsubtitle = fm.elidedText(m_subtitle, Qt::ElideRight, text_space);
      auto subtext_rect = text_rect;
      subtext_rect.moveTop(progress_y + progress_height);

      p.setFont(fnt);
      p.setPen(Utils::orcaTheme()->color(Utils::Theme::ProgressBarTitleColor));
      p.drawText(subtext_rect, alignment | Qt::AlignBottom, elidedsubtitle);
    }
  }

  // draw outer rect
  const QRect rect(indent - 1, progress_y, size().width() - 2 * indent + 1, progress_height);
  QRectF inner = rect.adjusted(2, 2, -2, -2);

  inner.adjust(0, 0, qRound((percent - 1) * inner.width()), 0);
  inner.setWidth(qMax(qMin(3.0, static_cast<qreal>(rect.width())), inner.width()));   // Show at least a hint of progress. Non-flat needs more pixels due to the borders.


  auto theme_color = Utils::Theme::ProgressBarColorNormal;

  if (m_error)
    theme_color = Utils::Theme::ProgressBarColorError;
  else if (m_finished)
    theme_color = Utils::Theme::ProgressBarColorFinished;

  const auto c = Utils::orcaTheme()->color(theme_color);

  //draw the progress bar
  if (Utils::orcaTheme()->flag(Utils::Theme::FlatToolBars)) {
    p.fillRect(rect.adjusted(2, 2, -2, -2), Utils::orcaTheme()->color(Utils::Theme::ProgressBarBackgroundColor));
    p.fillRect(inner, c);
  } else {
    const static QImage bar(Utils::StyleHelper::dpiSpecificImageFile(":/utils/images/progressbar.png"));
    Utils::StyleHelper::drawCornerImage(bar, &p, rect, 3, 3, 3, 3);

    // Draw line and shadow after the gradient fill
    if (value() > 0 && value() < maximum()) {
      p.fillRect(QRect(static_cast<int>(inner.right()), static_cast<int>(inner.top()), 2, static_cast<int>(inner.height())), QColor(0, 0, 0, 20));
      p.fillRect(QRect(static_cast<int>(inner.right()), static_cast<int>(inner.top()), 1, static_cast<int>(inner.height())), QColor(0, 0, 0, 60));
    }

    QLinearGradient grad(inner.topLeft(), inner.bottomLeft());
    grad.setColorAt(0, c.lighter(130));
    grad.setColorAt(0.4, c.lighter(106));
    grad.setColorAt(0.41, c.darker(106));
    grad.setColorAt(1, c.darker(130));

    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRect(inner);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 30), 1));
    p.drawLine(inner.topLeft() + QPointF(0.5, 0.5), inner.topRight() + QPointF(-0.5, 0.5));
    p.drawLine(inner.topLeft() + QPointF(0.5, 0.5), inner.bottomLeft() + QPointF(0.5, -0.5));
    p.drawLine(inner.topRight() + QPointF(-0.5, 0.5), inner.bottomRight() + QPointF(-0.5, -0.5));
    p.drawLine(inner.bottomLeft() + QPointF(0.5, -0.5), inner.bottomRight() + QPointF(-0.5, -0.5));
  }

  if (m_cancel_enabled) {
    // Draw cancel button
    p.setOpacity(m_cancel_button_fader);

    if (value() < maximum() && !m_error) {
      m_cancel_rect = QRect(rect.adjusted(rect.width() - g_cancelbutton_width + 2, 1, 0, 0));
      const auto hover = m_cancel_rect.contains(mapFromGlobal(QCursor::pos()));
      const QRectF cancel_visual_rect(m_cancel_rect.adjusted(0, 1, -2, -2));
      auto intensity = hover ? 90 : 70;

      if (!Utils::orcaTheme()->flag(Utils::Theme::FlatToolBars)) {
        QLinearGradient grad(cancel_visual_rect.topLeft(), cancel_visual_rect.bottomLeft());
        QColor button_color(intensity, intensity, intensity, 255);

        grad.setColorAt(0, button_color.lighter(130));
        grad.setColorAt(1, button_color.darker(130));

        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRect(cancel_visual_rect);
        p.setPen(QPen(QColor(0, 0, 0, 30)));
        p.drawLine(cancel_visual_rect.topLeft() + QPointF(-0.5, 0.5), cancel_visual_rect.bottomLeft() + QPointF(-0.5, -0.5));
        p.setPen(QPen(QColor(0, 0, 0, 120)));
        p.drawLine(cancel_visual_rect.topLeft() + QPointF(0.5, 0.5), cancel_visual_rect.bottomLeft() + QPointF(0.5, -0.5));
        p.setPen(QPen(QColor(255, 255, 255, 30)));
        p.drawLine(cancel_visual_rect.topLeft() + QPointF(1.5, 0.5), cancel_visual_rect.bottomLeft() + QPointF(1.5, -0.5));
      }

      p.setPen(QPen(hover ? Utils::StyleHelper::panelTextColor() : QColor(180, 180, 180), 1.2, Qt::SolidLine, Qt::FlatCap));
      p.setRenderHint(QPainter::Antialiasing, true);
      p.drawLine(cancel_visual_rect.topLeft() + QPointF(4.0, 2.0), cancel_visual_rect.bottomRight() + QPointF(-3.0, -2.0));
      p.drawLine(cancel_visual_rect.bottomLeft() + QPointF(4.0, -2.0), cancel_visual_rect.topRight() + QPointF(-3.0, 2.0));
    }
  }
}

} // namespace Internal
} // namespace Core
