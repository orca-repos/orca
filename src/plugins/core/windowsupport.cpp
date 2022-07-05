// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "windowsupport.h"
#include "coreconstants.h"
#include "icore.h"

#include <core/actionmanager/actioncontainer.h>
#include <core/actionmanager/actionmanager.h>
#include <core/actionmanager/command.h>

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#include <app/app_version.h>

#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QWidget>
#include <QWindowStateChangeEvent>

using namespace Utils;

namespace Core {
namespace Internal {

Q_GLOBAL_STATIC(WindowList, m_windowList)

WindowSupport::WindowSupport(QWidget *window, const Context &context) : QObject(window), m_window(window)
{
  m_window->installEventFilter(this);

  m_context_object = new IContext(this);
  m_context_object->setWidget(window);
  m_context_object->setContext(context);
  ICore::addContextObject(m_context_object);

  if constexpr (use_mac_shortcuts) {
    m_minimize_action = new QAction(this);
    ActionManager::registerAction(m_minimize_action, Constants::MINIMIZE_WINDOW, context);
    connect(m_minimize_action, &QAction::triggered, m_window, &QWidget::showMinimized);

    m_zoom_action = new QAction(this);
    ActionManager::registerAction(m_zoom_action, Constants::ZOOM_WINDOW, context);
    connect(m_zoom_action, &QAction::triggered, m_window, [this] {
      if (m_window->isMaximized()) {
        // similar to QWidget::showMaximized
        m_window->ensurePolished();
        m_window->setWindowState(m_window->windowState() & ~Qt::WindowMaximized);
        m_window->setVisible(true);
      } else {
        m_window->showMaximized();
      }
    });

    m_close_action = new QAction(this);
    ActionManager::registerAction(m_close_action, Constants::CLOSE_WINDOW, context);
    connect(m_close_action, &QAction::triggered, m_window, &QWidget::close, Qt::QueuedConnection);
  }

  m_toggle_full_screen_action = new QAction(this);
  updateFullScreenAction();
  ActionManager::registerAction(m_toggle_full_screen_action, Constants::TOGGLE_FULLSCREEN, context);
  connect(m_toggle_full_screen_action, &QAction::triggered, this, &WindowSupport::toggleFullScreen);

  m_windowList->addWindow(window);

  connect(ICore::instance(), &ICore::coreAboutToClose, this, [this]() { m_shutdown = true; });
}

WindowSupport::~WindowSupport()
{
  if (!m_shutdown) {
    // don't update all that stuff if we are shutting down anyhow
    if constexpr (use_mac_shortcuts) {
      ActionManager::unregisterAction(m_minimize_action, Constants::MINIMIZE_WINDOW);
      ActionManager::unregisterAction(m_zoom_action, Constants::ZOOM_WINDOW);
      ActionManager::unregisterAction(m_close_action, Constants::CLOSE_WINDOW);
    }
    ActionManager::unregisterAction(m_toggle_full_screen_action, Constants::TOGGLE_FULLSCREEN);
    m_windowList->removeWindow(m_window);
  }
}

auto WindowSupport::setCloseActionEnabled(bool enabled) const -> void
{
  if constexpr (use_mac_shortcuts)
    m_close_action->setEnabled(enabled);
}

auto WindowSupport::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (obj != m_window)
    return false;

  if (event->type() == QEvent::WindowStateChange) {
    if constexpr (HostOsInfo::isMacHost()) {
      auto minimized = m_window->isMinimized();
      m_minimize_action->setEnabled(!minimized);
      m_zoom_action->setEnabled(!minimized);
    }
    m_previous_window_state = dynamic_cast<QWindowStateChangeEvent*>(event)->oldState();
    updateFullScreenAction();
  } else if (event->type() == QEvent::WindowActivate) {
    m_windowList->setActiveWindow(m_window);
  } else if (event->type() == QEvent::Hide) {
    // minimized windows are hidden, but we still want to show them
    m_windowList->setWindowVisible(m_window, m_window->isMinimized());
  } else if (event->type() == QEvent::Show) {
    m_windowList->setWindowVisible(m_window, true);
  }

  return false;
}

auto WindowSupport::toggleFullScreen() const -> void
{
  if (m_window->isFullScreen()) {
    m_window->setWindowState(m_previous_window_state & ~Qt::WindowFullScreen);
  } else {
    m_window->setWindowState(m_window->windowState() | Qt::WindowFullScreen);
  }
}

auto WindowSupport::updateFullScreenAction() const -> void
{
  if (m_window->isFullScreen()) {
    if constexpr (HostOsInfo::isMacHost())
      m_toggle_full_screen_action->setText(tr("Exit Full Screen"));
    else
      m_toggle_full_screen_action->setChecked(true);
  } else {
    if constexpr (HostOsInfo::isMacHost())
      m_toggle_full_screen_action->setText(tr("Enter Full Screen"));
    else
      m_toggle_full_screen_action->setChecked(false);
  }
}

WindowList::~WindowList()
{
  qDeleteAll(m_window_actions);
}

auto WindowList::addWindow(QWidget *window) -> void
{
  #ifdef Q_OS_OSX
  if (!m_dockMenu) {
    m_dockMenu = new QMenu;
    m_dockMenu->setAsDockMenu();
  }
  #endif

  m_windows.append(window);
  const auto id = Id("Orca.Window.").withSuffix(static_cast<int>(m_windows.size()));
  m_window_action_ids.append(id);
  auto action = new QAction(window->windowTitle());
  m_window_actions.append(action);
  QObject::connect(action, &QAction::triggered, [action, this]() { activateWindow(action); });
  action->setCheckable(true);
  action->setChecked(false);
  const auto cmd = ActionManager::registerAction(action, id);
  cmd->setAttribute(Command::ca_update_text);
  ActionManager::actionContainer(Constants::M_WINDOW)->addAction(cmd, Constants::G_WINDOW_LIST);
  action->setVisible(window->isVisible() || window->isMinimized()); // minimized windows are hidden but should be shown
  QObject::connect(window, &QWidget::windowTitleChanged, [window, this]() { updateTitle(window); });

  if (m_dock_menu)
    m_dock_menu->addAction(action);

  if (window->isActiveWindow())
    setActiveWindow(window);
}

auto WindowList::activateWindow(QAction *action) const -> void
{
  const auto index = static_cast<int>(m_window_actions.indexOf(action));
  QTC_ASSERT(index >= 0, return);
  QTC_ASSERT(index < m_windows.size(), return);
  ICore::raiseWindow(m_windows.at(index));
}

auto WindowList::updateTitle(QWidget *window) const -> void
{
  const auto index = static_cast<int>(m_windows.indexOf(window));
  QTC_ASSERT(index >= 0, return);
  QTC_ASSERT(index < m_window_actions.size(), return);
  auto title = window->windowTitle();

  if (title.endsWith(QStringLiteral("- ") + Constants::IDE_DISPLAY_NAME))
    title.chop(12);

  m_window_actions.at(index)->setText(quoteAmpersands(title.trimmed()));
}

auto WindowList::removeWindow(QWidget *window) -> void
{
  // remove window from list,
  // remove last action from menu(s)
  // and update all action titles, starting with the index where the window was
  const auto index = static_cast<int>(m_windows.indexOf(window));
  QTC_ASSERT(index >= 0, return);

  ActionManager::unregisterAction(m_window_actions.last(), m_window_action_ids.last());
  delete m_window_actions.takeLast();
  m_window_action_ids.removeLast();

  m_windows.removeOne(window);

  for (auto i = index; i < m_windows.size(); ++i)
    updateTitle(m_windows.at(i));
}

auto WindowList::setActiveWindow(const QWidget *window) const -> void
{
  for (auto i = 0; i < m_windows.size(); ++i)
    m_window_actions.at(i)->setChecked(m_windows.at(i) == window);
}

auto WindowList::setWindowVisible(QWidget *window, const bool visible) const -> void
{
  const auto index = static_cast<int>(m_windows.indexOf(window));
  QTC_ASSERT(index >= 0, return);
  QTC_ASSERT(index < m_window_actions.size(), return);
  m_window_actions.at(index)->setVisible(visible);
}

} // Internal
} // Core
