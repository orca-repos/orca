// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "statuslabel.h"

#include <QTimer>

/*!
    \class Utils::StatusLabel

    \brief The StatusLabel class displays messages for a while with a timeout.
*/

namespace Utils {

StatusLabel::StatusLabel(QWidget *parent) : QLabel(parent)
{
  // A manual size let's us shrink below minimum text width which is what
  // we want in [fake] status bars.
  setMinimumSize(QSize(30, 10));
}

auto StatusLabel::stopTimer() -> void
{
  if (m_timer && m_timer->isActive())
    m_timer->stop();
}

auto StatusLabel::showStatusMessage(const QString &message, int timeoutMS) -> void
{
  setText(message);
  if (timeoutMS > 0) {
    if (!m_timer) {
      m_timer = new QTimer(this);
      m_timer->setSingleShot(true);
      connect(m_timer, &QTimer::timeout, this, &StatusLabel::slotTimeout);
    }
    m_timer->start(timeoutMS);
  } else {
    m_lastPermanentStatusMessage = message;
    stopTimer();
  }
}

auto StatusLabel::slotTimeout() -> void
{
  setText(m_lastPermanentStatusMessage);
}

auto StatusLabel::clearStatusMessage() -> void
{
  stopTimer();
  m_lastPermanentStatusMessage.clear();
  clear();
}

} // namespace Utils
