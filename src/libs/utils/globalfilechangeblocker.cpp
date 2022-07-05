// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "globalfilechangeblocker.h"
#include "qtcassert.h"

#include <QApplication>

namespace Utils {

GlobalFileChangeBlocker::GlobalFileChangeBlocker()
{
  m_blockedState = QApplication::applicationState() != Qt::ApplicationActive;
  qApp->installEventFilter(this);
}

auto GlobalFileChangeBlocker::instance() -> GlobalFileChangeBlocker*
{
  static GlobalFileChangeBlocker blocker;
  return &blocker;
}

auto GlobalFileChangeBlocker::forceBlocked(bool blocked) -> void
{
  if (blocked)
    ++m_forceBlocked;
  else if (QTC_GUARD(m_forceBlocked > 0))
    --m_forceBlocked;
  emitIfChanged();
}

auto GlobalFileChangeBlocker::eventFilter(QObject *obj, QEvent *e) -> bool
{
  if (obj == qApp && e->type() == QEvent::ApplicationStateChange)
    emitIfChanged();
  return false;
}

auto GlobalFileChangeBlocker::emitIfChanged() -> void
{
  const bool blocked = m_forceBlocked || (QApplication::applicationState() != Qt::ApplicationActive);
  if (blocked != m_blockedState) {
    emit stateChanged(blocked);
    m_blockedState = blocked;
  }
}

} // namespace Utils
