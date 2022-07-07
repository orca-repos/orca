// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/utils_global.hpp>

#include <QAction>
#include <QByteArray>
#include <QIcon>
#include <QString>

namespace Utils {

namespace Internal {
class TouchBarPrivate;
}

class ORCA_UTILS_EXPORT TouchBar {
public:
  TouchBar(const QByteArray &id, const QIcon &icon, const QString &title);
  TouchBar(const QByteArray &id, const QIcon &icon);
  TouchBar(const QByteArray &id, const QString &title);
  TouchBar(const QByteArray &id);

  ~TouchBar();

  auto id() const -> QByteArray;
  auto touchBarAction() const -> QAction*;
  auto insertAction(QAction *before, const QByteArray &id, QAction *action) -> void;
  auto insertTouchBar(QAction *before, TouchBar *touchBar) -> void;
  auto removeAction(QAction *action) -> void;
  auto removeTouchBar(TouchBar *touchBar) -> void;
  auto clear() -> void;
  auto setApplicationTouchBar() -> void;

private:
  Internal::TouchBarPrivate *d;
};

} // namespace Utils
