// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "overlaywidget.h"

#include "qtcassert.h"

#include <QEvent>
#include <QPainter>

Utils::OverlayWidget::OverlayWidget(QWidget *parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  if (parent)
    attachToWidget(parent);
}

auto Utils::OverlayWidget::setPaintFunction(const Utils::OverlayWidget::PaintFunction &paint) -> void
{
  m_paint = paint;
}

auto Utils::OverlayWidget::eventFilter(QObject *obj, QEvent *ev) -> bool
{
  if (obj == parent() && ev->type() == QEvent::Resize)
    resizeToParent();
  return QWidget::eventFilter(obj, ev);
}

auto Utils::OverlayWidget::paintEvent(QPaintEvent *ev) -> void
{
  if (m_paint) {
    QPainter p(this);
    m_paint(this, p, ev);
  }
}

auto Utils::OverlayWidget::attachToWidget(QWidget *parent) -> void
{
  if (parentWidget())
    parentWidget()->removeEventFilter(this);
  setParent(parent);
  if (parent) {
    parent->installEventFilter(this);
    resizeToParent();
    raise();
  }
}

auto Utils::OverlayWidget::resizeToParent() -> void
{
  QTC_ASSERT(parentWidget(), return);
  setGeometry(QRect(QPoint(0, 0), parentWidget()->size()));
}
