// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "detailsbutton.hpp"
#include "hostosinfo.hpp"
#include "theme/theme.hpp"

#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QPaintEvent>
#include <QPainter>
#include <QStyleOption>

using namespace Utils;

FadingWidget::FadingWidget(QWidget *parent) : FadingPanel(parent), m_opacityEffect(new QGraphicsOpacityEffect)
{
  m_opacityEffect->setOpacity(0);
  setGraphicsEffect(m_opacityEffect);

  // Workaround for issue with QGraphicsEffect. GraphicsEffect
  // currently clears with Window color. Remove if flickering
  // no longer occurs on fade-in
  QPalette pal;
  pal.setBrush(QPalette::All, QPalette::Window, Qt::transparent);
  setPalette(pal);
}

auto FadingWidget::setOpacity(qreal value) -> void
{
  m_opacityEffect->setOpacity(value);
}

auto FadingWidget::fadeTo(qreal value) -> void
{
  QPropertyAnimation *animation = new QPropertyAnimation(m_opacityEffect, "opacity");
  animation->setDuration(200);
  animation->setEndValue(value);
  animation->start(QAbstractAnimation::DeleteWhenStopped);
}

auto FadingWidget::opacity() -> qreal
{
  return m_opacityEffect->opacity();
}

DetailsButton::DetailsButton(QWidget *parent) : QAbstractButton(parent), m_fader(0)
{
  setCheckable(true);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
  setText(tr("Details"));
}

auto DetailsButton::sizeHint() const -> QSize
{
  // TODO: Adjust this when icons become available!
  const int w = fontMetrics().horizontalAdvance(text()) + 32;
  if (HostOsInfo::isMacHost())
    return QSize(w, 34);
  return QSize(w, 22);
}

auto DetailsButton::event(QEvent *e) -> bool
{
  switch (e->type()) {
  case QEvent::Enter: {
    QPropertyAnimation *animation = new QPropertyAnimation(this, "fader");
    animation->setDuration(200);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  case QEvent::Leave: {
    QPropertyAnimation *animation = new QPropertyAnimation(this, "fader");
    animation->setDuration(200);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  }
  break;
  default:
    return QAbstractButton::event(e);
  }
  return false;
}

auto DetailsButton::changeEvent(QEvent *e) -> void
{
  if (e->type() == QEvent::EnabledChange) {
    m_checkedPixmap = QPixmap();
    m_uncheckedPixmap = QPixmap();
  }
}

auto DetailsButton::paintEvent(QPaintEvent *e) -> void
{
  QWidget::paintEvent(e);

  QPainter p(this);

  // draw hover animation
  if (!HostOsInfo::isMacHost() && !isDown() && m_fader > 0) {
    QColor c = orcaTheme()->color(Theme::DetailsButtonBackgroundColorHover);
    c.setAlpha(int(m_fader * c.alpha()));

    QRect r = rect();
    if (!orcaTheme()->flag(Theme::FlatProjectsMode))
      r.adjust(1, 1, -2, -2);
    p.fillRect(r, c);
  }

  if (isChecked()) {
    if (m_checkedPixmap.isNull() || m_checkedPixmap.size() / m_checkedPixmap.devicePixelRatio() != contentsRect().size())
      m_checkedPixmap = cacheRendering(contentsRect().size(), true);
    p.drawPixmap(contentsRect(), m_checkedPixmap);
  } else {
    if (m_uncheckedPixmap.isNull() || m_uncheckedPixmap.size() / m_uncheckedPixmap.devicePixelRatio() != contentsRect().size())
      m_uncheckedPixmap = cacheRendering(contentsRect().size(), false);
    p.drawPixmap(contentsRect(), m_uncheckedPixmap);
  }
  if (isDown()) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 20));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 1, 1);
  }
  if (hasFocus()) {
    QStyleOptionFocusRect option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &p, this);
  }
}

auto DetailsButton::cacheRendering(const QSize &size, bool checked) -> QPixmap
{
  const qreal pixelRatio = devicePixelRatio();
  QPixmap pixmap(size * pixelRatio);
  pixmap.setDevicePixelRatio(pixelRatio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.translate(0.5, 0.5);

  if (!orcaTheme()->flag(Theme::FlatProjectsMode)) {
    QLinearGradient lg;
    lg.setCoordinateMode(QGradient::ObjectBoundingMode);
    lg.setFinalStop(0, 1);
    if (!checked) {
      lg.setColorAt(0, QColor(0, 0, 0, 10));
      lg.setColorAt(1, QColor(0, 0, 0, 16));
    } else {
      lg.setColorAt(0, QColor(255, 255, 255, 0));
      lg.setColorAt(1, QColor(255, 255, 255, 50));
    }
    p.setBrush(lg);
    p.setPen(QColor(255, 255, 255, 140));
    p.drawRoundedRect(1, 1, size.width() - 3, size.height() - 3, 1, 1);
    p.setPen(QPen(QColor(0, 0, 0, 40)));
    p.drawLine(0, 1, 0, size.height() - 2);
    if (checked)
      p.drawLine(1, size.height() - 1, size.width() - 1, size.height() - 1);
  } else {
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, size.width(), size.height(), 1, 1);
  }

  p.setPen(palette().color(QPalette::Text));

  QRect textRect = p.fontMetrics().boundingRect(text());
  textRect.setWidth(textRect.width() + 15);
  textRect.setHeight(textRect.height() + 4);
  textRect.moveCenter(rect().center());

  p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());

  int arrowsize = 15;
  QStyleOption arrowOpt;
  arrowOpt.initFrom(this);
  QPalette pal = arrowOpt.palette;
  pal.setBrush(QPalette::All, QPalette::Text, QColor(0, 0, 0));
  arrowOpt.rect = QRect(size.width() - arrowsize - 6, height() / 2 - arrowsize / 2, arrowsize, arrowsize);
  arrowOpt.palette = pal;
  style()->drawPrimitive(checked ? QStyle::PE_IndicatorArrowUp : QStyle::PE_IndicatorArrowDown, &arrowOpt, &p, this);
  return pixmap;
}

ExpandButton::ExpandButton(QWidget *parent) : QAbstractButton(parent)
{
  setCheckable(true);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
}

auto ExpandButton::sizeHint() const -> QSize
{
  return {fontMetrics().horizontalAdvance(text()) + 26, HostOsInfo::isMacHost() ? 34 : 22};
}

auto ExpandButton::paintEvent(QPaintEvent *e) -> void
{
  QWidget::paintEvent(e);
  QPainter p(this);

  QPixmap &pixmap = isChecked() ? m_checkedPixmap : m_uncheckedPixmap;
  if (pixmap.isNull() || pixmap.size() / pixmap.devicePixelRatio() != contentsRect().size())
    pixmap = cacheRendering();
  p.drawPixmap(contentsRect(), pixmap);

  if (isDown()) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 20));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 1, 1);
  }
  if (hasFocus()) {
    QStyleOptionFocusRect option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &p, this);
  }
}

auto ExpandButton::cacheRendering() -> QPixmap
{
  const QSize size = contentsRect().size();
  const qreal pixelRatio = devicePixelRatio();
  QPixmap pixmap(size * pixelRatio);
  pixmap.setDevicePixelRatio(pixelRatio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.translate(0.5, 0.5);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(0, 0, size.width(), size.height(), 1, 1);
  int arrowsize = 15;
  QStyleOption arrowOpt;
  arrowOpt.initFrom(this);
  QPalette pal = arrowOpt.palette;
  pal.setBrush(QPalette::All, QPalette::Text, QColor(0, 0, 0));
  arrowOpt.rect = QRect(size.width() - arrowsize - 6, height() / 2 - arrowsize / 2, arrowsize, arrowsize);
  arrowOpt.palette = pal;
  style()->drawPrimitive(isChecked() ? QStyle::PE_IndicatorArrowUp : QStyle::PE_IndicatorArrowDown, &arrowOpt, &p, this);
  return pixmap;
}
