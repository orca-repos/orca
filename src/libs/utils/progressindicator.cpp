// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "progressindicator.hpp"

#include "icon.hpp"
#include "qtcassert.hpp"
#include "stylehelper.hpp"

#include <QEvent>
#include <QPainter>
#include <QPixmap>

/*!
    \class Utils::ProgressIndicator
    \brief The ProgressIndicator class shows an circular, endlessly animated progress indicator.

    Use it if you want to indicate that some work is being done, but you do not have the detailed
    progress information needed for a progress bar. You can either create the widget on demand,
    or create the widget once and only show it on demand. The animation only runs while the widget
    is visible.

    \inmodule Orca
*/

/*!
    \class Utils::ProgressIndicatorPainter
    \brief The ProgressIndicatorPainter class is the painting backend for the ProgressIndicator
    class.

    You can use it to paint a circular, endlessly animated progress indicator directly onto a
    QPaintDevice, for example, if you want to show a progress indicator where you cannot use
    a QWidget.

    \inmodule Orca
*/

/*!
    \enum Utils::ProgressIndicatorSize

    Size of a progress indicator.
    \sa Utils::ProgressIndicator
    \sa Utils::ProgressIndicatorPainter

    \value Small
           Small icon size. Useful for tool bars, status bars, rows in tree views,
           and so on.
    \value Medium
           Larger progress indicator useful for covering whole medium sized widgets.
    \value Large
           Very large progress indicator that can be used to cover large parts of a UI.
*/

namespace Utils {

static auto imageFileNameForIndicatorSize(ProgressIndicatorSize size) -> FilePath
{
  switch (size) {
  case ProgressIndicatorSize::Large:
    return ":/utils/images/progressindicator_big.png";
  case ProgressIndicatorSize::Medium:
    return ":/utils/images/progressindicator_medium.png";
  case ProgressIndicatorSize::Small: default:
    return ":/utils/images/progressindicator_small.png";
  }
}


/*!
    Constructs a progress indicator painter for the indicator \a size.

    \sa setUpdateCallback
*/
ProgressIndicatorPainter::ProgressIndicatorPainter(ProgressIndicatorSize size)
{
  m_timer.setSingleShot(false);
  QObject::connect(&m_timer, &QTimer::timeout, [this]() {
    nextAnimationStep();
    if (m_callback)
      m_callback();
  });

  setIndicatorSize(size);
}

/*!
    Changes the size of the progress indicator to \a size. Users of the class need
    to adapt their painting or layouting code to the change in resulting pixel size.

    \sa indicatorSize
    \sa size
*/
auto ProgressIndicatorPainter::setIndicatorSize(ProgressIndicatorSize size) -> void
{
  m_size = size;
  m_rotationStep = size == ProgressIndicatorSize::Small ? 45 : 30;
  m_timer.setInterval(size == ProgressIndicatorSize::Small ? 100 : 80);
  m_pixmap = Icon({{imageFileNameForIndicatorSize(size), Theme::PanelTextColorMid}}, Icon::Tint).pixmap();
}

/*!
    Returns the current indicator size. Use \l size to get the resulting
    pixel size.

    \sa setIndicatorSize
*/
auto ProgressIndicatorPainter::indicatorSize() const -> ProgressIndicatorSize
{
  return m_size;
}

/*!
    Sets the callback \a cb that is called whenever the progress indicator needs a repaint, because
    its animation progressed. The callback is a void function taking no parameters, and should
    usually trigger a QWidget::update on the widget that does the actual painting.
*/
auto ProgressIndicatorPainter::setUpdateCallback(const UpdateCallback &cb) -> void
{
  m_callback = cb;
}

/*!
    Returns the size of the progress indicator in device independent pixels.

    \sa setIndicatorSize
    \sa paint
*/
auto ProgressIndicatorPainter::size() const -> QSize
{
  return m_pixmap.size() / m_pixmap.devicePixelRatio();
}

/*!
    Paints the progress indicator centered in the \a rect on the given \a painter.

    \sa size
*/
auto ProgressIndicatorPainter::paint(QPainter &painter, const QRect &rect) const -> void
{
  painter.save();
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  QPoint translate(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
  QTransform t;
  t.translate(translate.x(), translate.y());
  t.rotate(m_rotation);
  t.translate(-translate.x(), -translate.y());
  painter.setTransform(t);
  QSize pixmapUserSize(m_pixmap.size() / m_pixmap.devicePixelRatio());
  painter.drawPixmap(QPoint(rect.x() + ((rect.width() - pixmapUserSize.width()) / 2), rect.y() + ((rect.height() - pixmapUserSize.height()) / 2)), m_pixmap);
  painter.restore();
}

/*!
    Starts the progress indicator animation.

    \sa setUpdateCallback
    \sa stopAnimation
*/
auto ProgressIndicatorPainter::startAnimation() -> void
{
  QTC_ASSERT(m_callback, return);
  m_timer.start();
}

/*!
    Stops the progress indicator animation.

    \sa setUpdateCallback
    \sa startAnimation
*/
auto ProgressIndicatorPainter::stopAnimation() -> void
{
  m_timer.stop();
}

/*!
    \internal
*/
auto ProgressIndicatorPainter::nextAnimationStep() -> void
{
  m_rotation = (m_rotation + m_rotationStep + 360) % 360;
}

/*!
    Constructs a ProgressIndicator of the size \a size and with the parent \a parent.

    Use \l attachToWidget to make the progress indicator automatically resize and center on the
    parent widget.

    \sa attachToWidget
    \sa setIndicatorSize
*/
ProgressIndicator::ProgressIndicator(ProgressIndicatorSize size, QWidget *parent) : OverlayWidget(parent), m_paint(size)
{
  setPaintFunction([this](QWidget *w, QPainter &p, QPaintEvent *) { m_paint.paint(p, w->rect()); });
  m_paint.setUpdateCallback([this]() { update(); });
  updateGeometry();
}

/*!
    Changes the size of the progress indicator to \a size.

    \sa indicatorSize
*/
auto ProgressIndicator::setIndicatorSize(ProgressIndicatorSize size) -> void
{
  m_paint.setIndicatorSize(size);
  updateGeometry();
}

/*!
    Returns the size of the indicator in device independent pixels.

    \sa indicatorSize
*/
auto ProgressIndicator::sizeHint() const -> QSize
{
  return m_paint.size();
}

/*!
    \internal
*/
auto ProgressIndicator::showEvent(QShowEvent *) -> void
{
  m_paint.startAnimation();
}

/*!
    \internal
*/
auto ProgressIndicator::hideEvent(QHideEvent *) -> void
{
  m_paint.stopAnimation();
}

} // namespace Utils
