// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "elidinglabel.h"
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>

/*!
    \class Utils::ElidingLabel

    \brief The ElidingLabel class is a label suitable for displaying elided
    text.
*/

namespace Utils {

ElidingLabel::ElidingLabel(QWidget *parent) : ElidingLabel({}, parent) {}

ElidingLabel::ElidingLabel(const QString &text, QWidget *parent) : QLabel(text, parent)
{
  setElideMode(Qt::ElideRight);
}

auto ElidingLabel::elideMode() const -> Qt::TextElideMode
{
  return m_elideMode;
}

auto ElidingLabel::setElideMode(const Qt::TextElideMode &elideMode) -> void
{
  m_elideMode = elideMode;
  if (elideMode == Qt::ElideNone)
    setToolTip({});

  setSizePolicy(QSizePolicy(m_elideMode == Qt::ElideNone ? QSizePolicy::Preferred : QSizePolicy::Ignored, QSizePolicy::Preferred, QSizePolicy::Label));
  update();
}

auto ElidingLabel::paintEvent(QPaintEvent *) -> void
{
  if (m_elideMode == Qt::ElideNone) {
    QLabel::paintEvent(nullptr);
    return;
  }

  const int m = margin();
  QRect contents = contentsRect().adjusted(m, m, -m, -m);
  QFontMetrics fm = fontMetrics();
  QString txt = text();
  if (txt.length() > 4 && fm.horizontalAdvance(txt) > contents.width()) {
    setToolTip(txt);
    txt = fm.elidedText(txt, m_elideMode, contents.width());
  } else {
    setToolTip(QString());
  }
  int flags = QStyle::visualAlignment(layoutDirection(), alignment()) | Qt::TextSingleLine;

  QPainter painter(this);
  drawFrame(&painter);
  painter.drawText(contents, flags, txt);
}

} // namespace Utils
