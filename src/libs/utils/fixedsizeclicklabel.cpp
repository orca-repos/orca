// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fixedsizeclicklabel.h"

#include <QMouseEvent>

/*!
    \class Utils::FixedSizeClickLabel

    \brief The FixedSizeClickLabel class is a label with a size hint derived from a sample text
    that can be different to the text that is shown.

    For convenience it also has a clicked signal that is emitted whenever the label receives a mouse
    click.

    \inmodule Orca
*/

/*!
    \fn Utils::FixedSizeClickLabel::clicked()

    This signal is emitted when the label is clicked with the left mouse button.
*/

/*!
    \property Utils::FixedSizeClickLabel::maxText

    This property holds the text that is used to calculate the label's size hint.
*/

namespace Utils {

/*!
    Constructs a FixedSizeClickLabel with the parent \a parent.
*/
FixedSizeClickLabel::FixedSizeClickLabel(QWidget *parent) : QLabel(parent) {}

/*!
    Sets the label's text to \a text, and changes the size hint of the label to the size of
    \a maxText.

    \sa maxText
    \sa setMaxText
*/
auto FixedSizeClickLabel::setText(const QString &text, const QString &maxText) -> void
{
  QLabel::setText(text);
  m_maxText = maxText;
}

/*!
    \reimp
*/
auto FixedSizeClickLabel::sizeHint() const -> QSize
{
  return fontMetrics().boundingRect(m_maxText).size();
}

auto FixedSizeClickLabel::maxText() const -> QString
{
  return m_maxText;
}

auto FixedSizeClickLabel::setMaxText(const QString &maxText) -> void
{
  m_maxText = maxText;
}

/*!
    \reimp
*/
auto FixedSizeClickLabel::mousePressEvent(QMouseEvent *ev) -> void
{
  QLabel::mousePressEvent(ev);
  if (ev->button() == Qt::LeftButton)
    m_pressed = true;
}

/*!
    \reimp
*/
auto FixedSizeClickLabel::mouseReleaseEvent(QMouseEvent *ev) -> void
{
  QLabel::mouseReleaseEvent(ev);
  if (ev->button() != Qt::LeftButton)
    return;
  if (m_pressed && rect().contains(ev->pos())) emit clicked();
  m_pressed = false;
}

} // namespace Utils
