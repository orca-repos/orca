// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "touchbar.hpp"

namespace Utils {
namespace Internal {

class TouchBarPrivate {
public:
  QByteArray m_id;
  QAction m_action;
};

} // namespace Internal

TouchBar::TouchBar(const QByteArray &id, const QIcon &icon, const QString &title) : d(new Internal::TouchBarPrivate)
{
  d->m_id = id;
  d->m_action.setIcon(icon);
  d->m_action.setText(title);
}

TouchBar::TouchBar(const QByteArray &id, const QIcon &icon) : TouchBar(id, icon, {}) {}
TouchBar::TouchBar(const QByteArray &id, const QString &title) : TouchBar(id, {}, title) {}
TouchBar::TouchBar(const QByteArray &id) : TouchBar(id, {}, {}) {}

TouchBar::~TouchBar()
{
  delete d;
}

auto TouchBar::id() const -> QByteArray
{
  return d->m_id;
}

auto TouchBar::touchBarAction() const -> QAction*
{
  return &d->m_action;
}

auto TouchBar::insertAction(QAction *, const QByteArray &, QAction *) -> void {}
auto TouchBar::insertTouchBar(QAction *, TouchBar *) -> void {}
auto TouchBar::removeAction(QAction *) -> void {}
auto TouchBar::removeTouchBar(TouchBar *) -> void {}
auto TouchBar::clear() -> void {}
auto TouchBar::setApplicationTouchBar() -> void {}

} // namespace Utils
