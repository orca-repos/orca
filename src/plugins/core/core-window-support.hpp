// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-context-interface.hpp"

#include <QObject>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QWidget;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class WindowList {
public:
  ~WindowList();

  auto addWindow(QWidget *window) -> void;
  auto removeWindow(QWidget *window) -> void;
  auto setActiveWindow(const QWidget *window) const -> void;
  auto setWindowVisible(QWidget *window, bool visible) const -> void;

private:
  auto activateWindow(QAction *action) const -> void;
  auto updateTitle(QWidget *window) const -> void;

  QMenu *m_dock_menu = nullptr;
  QList<QWidget*> m_windows;
  QList<QAction*> m_window_actions;
  QList<Utils::Id> m_window_action_ids;
};

class WindowSupport final : public QObject {
  Q_OBJECT

public:
  WindowSupport(QWidget *window, const Context &context);
  ~WindowSupport() override;

  auto setCloseActionEnabled(bool enabled) const -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  auto toggleFullScreen() const -> void;
  auto updateFullScreenAction() const -> void;

  QWidget *m_window;
  IContext *m_context_object;
  QAction *m_minimize_action;
  QAction *m_zoom_action;
  QAction *m_close_action;
  QAction *m_toggle_full_screen_action;
  Qt::WindowStates m_previous_window_state;
  bool m_shutdown = false;
};

} // namespace Orca::Plugin::Core
