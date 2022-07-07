// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "actioncontainer_p.hpp"
#include "actionmanager.hpp"

#include <core/coreconstants.hpp>
#include <core/icontext.hpp>

#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QAction>
#include <QMenuBar>

Q_DECLARE_METATYPE(Core::Internal::MenuActionContainer*)

using namespace Utils;

namespace Core {

/*!
    \class Core::ActionContainer
    \inheaderfile coreplugin/actionmanager/actioncontainer.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The ActionContainer class represents a menu or menu bar in \QC.

    You don't create instances of this class directly, but instead use the
    \l{ActionManager::createMenu()}, \l{ActionManager::createMenuBar()} and
    \l{ActionManager::createTouchBar()} functions.
    Retrieve existing action containers for an ID with
    \l{ActionManager::actionContainer()}.

    Within a menu or menu bar you can group menus and items together by defining groups
    (the order of the groups is defined by the order of the \l{ActionContainer::appendGroup()} calls), and
    adding menus or actions to these groups. If no custom groups are defined, an action container
    has three default groups \c{Core::Constants::G_DEFAULT_ONE}, \c{Core::Constants::G_DEFAULT_TWO}
    and \c{Core::Constants::G_DEFAULT_THREE}.

    You can specify whether the menu represented by this action container should
    be automatically disabled or hidden whenever it only contains disabled items
    and submenus by setting the corresponding \l setOnAllDisabledBehavior(). The
    default is ActionContainer::Disable for menus, and ActionContainer::Show for
    menu bars.
*/

/*!
    \enum Core::ActionContainer::OnAllDisabledBehavior
    Defines what happens when the represented menu is empty or contains only
    disabled or invisible items.
    \value Disable
        The menu will be visible but disabled.
    \value Hide
        The menu will not be visible until the state of the subitems changes.
    \value Show
        The menu will still be visible and active.
*/

/*!
    \fn Core::ActionContainer::setOnAllDisabledBehavior(OnAllDisabledBehavior behavior)
    Defines the \a behavior of the menu represented by this action container for the case
    whenever it only contains disabled items and submenus.
    The default is ActionContainer::Disable for menus, and ActionContainer::Show for menu bars.
    \sa ActionContainer::OnAllDisabledBehavior
    \sa ActionContainer::onAllDisabledBehavior()
*/

/*!
    \fn Core::ActionContainer::onAllDisabledBehavior() const
    Returns the behavior of the menu represented by this action container for the case
    whenever it only contains disabled items and submenus.
    The default is ActionContainer::Disable for menus, and ActionContainer::Show for menu bars.
    \sa OnAllDisabledBehavior
    \sa setOnAllDisabledBehavior()
*/

/*!
    \fn int Core::ActionContainer::id() const
    \internal
*/

/*!
    \fn QMenu *Core::ActionContainer::menu() const
    Returns the QMenu instance that is represented by this action container, or
    0 if this action container represents a menu bar.
*/

/*!
    \fn QMenuBar *Core::ActionContainer::menuBar() const
    Returns the QMenuBar instance that is represented by this action container, or
    0 if this action container represents a menu.
*/

/*!
    \fn QAction *Core::ActionContainer::insertLocation(Utils::Id group) const
    Returns an action representing the \a group,
    that could be used with \c{QWidget::insertAction}.
*/

/*!
    \fn void Core::ActionContainer::appendGroup(Utils::Id group)
    Adds \a group to the action container.

    Use groups to segment your action container into logical parts. You can add
    actions and menus directly into groups.
    \sa addAction()
    \sa addMenu()
*/

/*!
    \fn void Core::ActionContainer::addAction(Core::Command *action, Utils::Id group = Id())
    Add the \a action as a menu item to this action container. The action is added as the
    last item of the specified \a group.
    \sa appendGroup()
    \sa addMenu()
*/

/*!
    \fn void Core::ActionContainer::addMenu(Core::ActionContainer *menu, Utils::Id group = Utils::Id())
    Add the \a menu as a submenu to this action container. The menu is added as the
    last item of the specified \a group.
    \sa appendGroup()
    \sa addAction()
*/

/*!
    \fn void Core::ActionContainer::addMenu(Core::ActionContainer *before, Core::ActionContainer *menu)
    Add \a menu as a submenu to this action container before the menu specified
    by \a before.
    \sa appendGroup()
    \sa addAction()
*/

/*!
    \fn Core::ActionContainer::clear()

    Clears this menu and submenus from all actions and submenus. However, does
    does not destroy the submenus and commands, just removes them from their
    parents.
*/

/*!
    \fn Core::ActionContainer::insertGroup(Utils::Id before, Utils::Id group)

    Inserts \a group to the action container before the group specified by
    \a before.
*/

/*!
    \fn virtual Utils::TouchBar *Core::ActionContainer::touchBar() const

    Returns the touch bar that is represented by this action container.
*/

/*!
    \fn Core::ActionContainer::addSeparator(const Core::Context &context, Utils::Id group, QAction **outSeparator)

    Adds a separator to the end of the given \a group to the action container,
    which is enabled for a given \a context. Returns the created separator
    action, \a outSeparator.
*/

namespace Internal {

// ---------- ActionContainerPrivate ------------

/*!
    \class Core::Internal::ActionContainerPrivate
    \internal
*/

ActionContainerPrivate::ActionContainerPrivate(const Id id) : m_on_all_disabled_behavior(on_all_disabled_behavior::disable), m_id(id), m_update_requested(false)
{
  ActionContainerPrivate::appendGroup(Constants::G_DEFAULT_ONE);
  ActionContainerPrivate::appendGroup(Constants::G_DEFAULT_TWO);
  ActionContainerPrivate::appendGroup(Constants::G_DEFAULT_THREE);
  scheduleUpdate();
}

auto ActionContainerPrivate::setOnAllDisabledBehavior(const on_all_disabled_behavior behavior) -> void
{
  m_on_all_disabled_behavior = behavior;
}

auto ActionContainerPrivate::onAllDisabledBehavior() const -> on_all_disabled_behavior
{
  return m_on_all_disabled_behavior;
}

auto ActionContainerPrivate::appendGroup(const Id group_id) -> void
{
  m_groups.append(Group(group_id));
}

auto ActionContainerPrivate::insertGroup(const Id before, const Id group_id) -> void
{
  auto it = m_groups.begin();
  while (it != m_groups.end()) {
    if (it->id == before) {
      m_groups.insert(it, Group(group_id));
      break;
    }
    ++it;
  }
}

auto ActionContainerPrivate::findGroup(const Id group_id) const -> QList<Group>::const_iterator
{
  auto it = m_groups.constBegin();
  while (it != m_groups.constEnd()) {
    if (it->id == group_id)
      break;
    ++it;
  }
  return it;
}

auto ActionContainerPrivate::insertLocation(const Id group_id) const -> QAction*
{
  const auto it = findGroup(group_id);
  QTC_ASSERT(it != m_groups.constEnd(), return nullptr);
  return insertLocation(it);
}

auto ActionContainerPrivate::actionForItem(QObject *item) const -> QAction*
{
  if (const auto cmd = qobject_cast<Command*>(item)) {
    return cmd->action();
  }

  if (const auto container = qobject_cast<ActionContainerPrivate*>(item)) {
    if (container->containerAction())
      return container->containerAction();
  }

  QTC_ASSERT(false, return nullptr);
}

auto ActionContainerPrivate::insertLocation(QList<Group>::const_iterator group) const -> QAction*
{
  if (group == m_groups.constEnd())
    return nullptr;

  ++group;

  while (group != m_groups.constEnd()) {
    if (!group->items.isEmpty()) {
      const auto item = group->items.first();
      if (const auto action = actionForItem(item))
        return action;
    }
    ++group;
  }
  return nullptr;
}

auto ActionContainerPrivate::addAction(Command *command, const Id group_id) -> void
{
  if (!canAddAction(command))
    return;

  const auto actual_group_id = group_id.isValid() ? group_id : Id(Constants::G_DEFAULT_TWO);
  const auto group_it = findGroup(actual_group_id);

  QTC_ASSERT(group_it != m_groups.constEnd(), qDebug() << "Can't find group" << group_id.name() << "in container" << id().name(); return);
  m_groups[group_it - m_groups.constBegin()].items.append(command);

  connect(command, &Command::activeStateChanged, this, &ActionContainerPrivate::scheduleUpdate);
  connect(command, &QObject::destroyed, this, &ActionContainerPrivate::itemDestroyed);

  const auto before_action = insertLocation(group_it);
  insertAction(before_action, command);

  scheduleUpdate();
}

auto ActionContainerPrivate::addMenu(ActionContainer *menu, const Id group_id) -> void
{
  const auto container_private = dynamic_cast<ActionContainerPrivate*>(menu);
  QTC_ASSERT(container_private->canBeAddedToContainer(this), return);

  const auto actual_group_id = group_id.isValid() ? group_id : Id(Constants::G_DEFAULT_TWO);
  const auto group_it = findGroup(actual_group_id);

  QTC_ASSERT(group_it != m_groups.constEnd(), return);
  m_groups[group_it - m_groups.constBegin()].items.append(menu);

  connect(menu, &QObject::destroyed, this, &ActionContainerPrivate::itemDestroyed);

  const auto before_action = insertLocation(group_it);
  insertMenu(before_action, menu);

  scheduleUpdate();
}

auto ActionContainerPrivate::addMenu(ActionContainer *before, ActionContainer *menu) -> void
{
  const auto container_private = dynamic_cast<ActionContainerPrivate*>(menu);
  QTC_ASSERT(container_private->canBeAddedToContainer(this), return);

  for (auto &group : m_groups) {
    if (const auto insertion_point = group.items.indexOf(before); insertion_point >= 0) {
      group.items.insert(insertion_point, menu);
      break;
    }
  }

  connect(menu, &QObject::destroyed, this, &ActionContainerPrivate::itemDestroyed);

  const auto before_private = dynamic_cast<ActionContainerPrivate*>(before);
  if (const auto before_action = before_private->containerAction())
    insertMenu(before_action, menu);

  scheduleUpdate();
}

auto ActionContainerPrivate::addSeparator(const Context &context, const Id group_id, QAction **out_separator) -> Command*
{

  const auto separator = new QAction(this);
  separator->setSeparator(true);

  static auto separator_id_count = 0;
  const auto sep_id = id().withSuffix(".Separator.").withSuffix(++separator_id_count);
  const auto cmd = ActionManager::registerAction(separator, sep_id, context);

  addAction(cmd, group_id);

  if (out_separator)
    *out_separator = separator;

  return cmd;
}

auto ActionContainerPrivate::clear() -> void
{
  for (auto &group : m_groups) {
    for (const auto item : qAsConst(group.items)) {
      if (const auto command = qobject_cast<Command*>(item)) {
        removeAction(command);
        disconnect(command, &Command::activeStateChanged, this, &ActionContainerPrivate::scheduleUpdate);
        disconnect(command, &QObject::destroyed, this, &ActionContainerPrivate::itemDestroyed);
      } else if (const auto container = qobject_cast<ActionContainer*>(item)) {
        container->clear();
        disconnect(container, &QObject::destroyed, this, &ActionContainerPrivate::itemDestroyed);
        removeMenu(container);
      }
    }
    group.items.clear();
  }
  scheduleUpdate();
}

auto ActionContainerPrivate::itemDestroyed() -> void
{
  const auto obj = sender();
  for (auto &group : m_groups) {
    if (group.items.removeAll(obj) > 0)
      break;
  }
}

auto ActionContainerPrivate::id() const -> Id
{
  return m_id;
}

auto ActionContainerPrivate::menu() const -> QMenu*
{
  return nullptr;
}

auto ActionContainerPrivate::menuBar() const -> QMenuBar*
{
  return nullptr;
}

auto ActionContainerPrivate::touchBar() const -> TouchBar*
{
  return nullptr;
}

auto ActionContainerPrivate::canAddAction(Command *command) -> bool
{
  return command && command->action();
}

auto ActionContainerPrivate::scheduleUpdate() -> void
{
  if (m_update_requested)
    return;

  m_update_requested = true;

  QMetaObject::invokeMethod(this, &ActionContainerPrivate::update, Qt::QueuedConnection);
}

auto ActionContainerPrivate::update() -> void
{
  updateInternal();
  m_update_requested = false;
}

// ---------- MenuActionContainer ------------

/*!
    \class Core::Internal::MenuActionContainer
    \internal
*/

MenuActionContainer::MenuActionContainer(const Id id) : ActionContainerPrivate(id), m_menu(new QMenu)
{
  m_menu->setObjectName(id.toString());
  m_menu->menuAction()->setMenuRole(QAction::NoRole);
  ActionContainerPrivate::setOnAllDisabledBehavior(on_all_disabled_behavior::disable);
}

MenuActionContainer::~MenuActionContainer()
{
  delete m_menu;
}

auto MenuActionContainer::menu() const -> QMenu*
{
  return m_menu;
}

auto MenuActionContainer::containerAction() const -> QAction*
{
  return m_menu->menuAction();
}

auto MenuActionContainer::insertAction(QAction *before, Command *command) -> void
{
  m_menu->insertAction(before, command->action());
}

auto MenuActionContainer::insertMenu(QAction *before, ActionContainer *container) -> void
{
  const auto menu = container->menu();
  QTC_ASSERT(menu, return);
  menu->setParent(m_menu, menu->windowFlags()); // work around issues with Qt Wayland (QTBUG-68636)
  m_menu->insertMenu(before, menu);
}

auto MenuActionContainer::removeAction(Command *command) -> void
{
  m_menu->removeAction(command->action());
}

auto MenuActionContainer::removeMenu(ActionContainer *container) -> void
{
  const auto menu = container->menu();
  QTC_ASSERT(menu, return);
  m_menu->removeAction(menu->menuAction());
}

auto MenuActionContainer::updateInternal() -> bool
{
  if (onAllDisabledBehavior() == on_all_disabled_behavior::show)
    return true;

  auto hasitems = false;
  auto actions = m_menu->actions();

  for (const auto &group : qAsConst(m_groups)) {
    for (const auto item : qAsConst(group.items)) {
      if (const auto container = qobject_cast<ActionContainerPrivate*>(item)) {
        actions.removeAll(container->menu()->menuAction());
        if (container == this) {
          QByteArray warning = Q_FUNC_INFO + QByteArray(" container '");
          if (this->menu())
            warning += this->menu()->title().toLocal8Bit();
          warning += "' contains itself as subcontainer";
          qWarning("%s", warning.constData());
          continue;
        }
        if (container->updateInternal()) {
          hasitems = true;
          break;
        }
      } else if (const auto command = qobject_cast<Command*>(item)) {
        actions.removeAll(command->action());
        if (command->isActive()) {
          hasitems = true;
          break;
        }
      } else {
        QTC_ASSERT(false, continue);
      }
    }
    if (hasitems)
      break;
  }

  if (!hasitems) {
    // look if there were actions added that we don't control and check if they are enabled
    for (const auto action : qAsConst(actions)) {
      if (!action->isSeparator() && action->isEnabled()) {
        hasitems = true;
        break;
      }
    }
  }

  if (onAllDisabledBehavior() == on_all_disabled_behavior::hide)
    m_menu->menuAction()->setVisible(hasitems);
  else if (onAllDisabledBehavior() == on_all_disabled_behavior::disable)
    m_menu->menuAction()->setEnabled(hasitems);

  return hasitems;
}

auto MenuActionContainer::canBeAddedToContainer(ActionContainerPrivate *container) const -> bool
{
  return qobject_cast<MenuActionContainer*>(container) || qobject_cast<MenuBarActionContainer*>(container);
}

// ---------- MenuBarActionContainer ------------

/*!
    \class Core::Internal::MenuBarActionContainer
    \internal
*/

MenuBarActionContainer::MenuBarActionContainer(const Id id) : ActionContainerPrivate(id), m_menu_bar(nullptr)
{
  ActionContainerPrivate::setOnAllDisabledBehavior(on_all_disabled_behavior::show);
}

auto MenuBarActionContainer::setMenuBar(QMenuBar *menu_bar) -> void
{
  m_menu_bar = menu_bar;
}

auto MenuBarActionContainer::menuBar() const -> QMenuBar*
{
  return m_menu_bar;
}

auto MenuBarActionContainer::containerAction() const -> QAction*
{
  return nullptr;
}

auto MenuBarActionContainer::insertAction(QAction *before, Command *command) -> void
{
  m_menu_bar->insertAction(before, command->action());
}

auto MenuBarActionContainer::insertMenu(QAction *before, ActionContainer *container) -> void
{
  const auto menu = container->menu();
  QTC_ASSERT(menu, return);
  menu->setParent(m_menu_bar, menu->windowFlags()); // work around issues with Qt Wayland (QTBUG-68636)
  m_menu_bar->insertMenu(before, menu);
}

auto MenuBarActionContainer::removeAction(Command *command) -> void
{
  m_menu_bar->removeAction(command->action());
}

auto MenuBarActionContainer::removeMenu(ActionContainer *container) -> void
{
  const auto menu = container->menu();
  QTC_ASSERT(menu, return);
  m_menu_bar->removeAction(menu->menuAction());
}

auto MenuBarActionContainer::updateInternal() -> bool
{
  if (onAllDisabledBehavior() == on_all_disabled_behavior::show)
    return true;

  auto hasitems = false;
  for (auto actions = m_menu_bar->actions(); const auto action : actions) {
    if (action->isVisible()) {
      hasitems = true;
      break;
    }
  }

  if (onAllDisabledBehavior() == on_all_disabled_behavior::hide)
    m_menu_bar->setVisible(hasitems);
  else if (onAllDisabledBehavior() == on_all_disabled_behavior::disable)
    m_menu_bar->setEnabled(hasitems);

  return hasitems;
}

auto MenuBarActionContainer::canBeAddedToContainer(ActionContainerPrivate *) const -> bool
{
  return false;
}

constexpr char id_prefix[] = "io.qt.orca.";

TouchBarActionContainer::TouchBarActionContainer(const Id id, const QIcon &icon, const QString &text) : ActionContainerPrivate(id), m_touch_bar(std::make_unique<TouchBar>(id.withPrefix(id_prefix).name(), icon, text))
{
}

TouchBarActionContainer::~TouchBarActionContainer() = default;

auto TouchBarActionContainer::touchBar() const -> TouchBar*
{
  return m_touch_bar.get();
}

auto TouchBarActionContainer::containerAction() const -> QAction*
{
  return m_touch_bar->touchBarAction();
}

auto TouchBarActionContainer::actionForItem(QObject *item) const -> QAction*
{
  if (const auto command = qobject_cast<Command*>(item))
    return command->touchBarAction();

  return ActionContainerPrivate::actionForItem(item);
}

auto TouchBarActionContainer::insertAction(QAction *before, Command *command) -> void
{
  m_touch_bar->insertAction(before, command->id().withPrefix(id_prefix).name(), command->touchBarAction());
}

auto TouchBarActionContainer::insertMenu(QAction *before, ActionContainer *container) -> void
{
  const auto touch_bar = container->touchBar();
  QTC_ASSERT(touch_bar, return);
  m_touch_bar->insertTouchBar(before, touch_bar);
}

auto TouchBarActionContainer::removeAction(Command *command) -> void
{
  m_touch_bar->removeAction(command->touchBarAction());
}

auto TouchBarActionContainer::removeMenu(ActionContainer *container) -> void
{
  const auto touch_bar = container->touchBar();
  QTC_ASSERT(touch_bar, return);
  m_touch_bar->removeTouchBar(touch_bar);
}

auto TouchBarActionContainer::canBeAddedToContainer(ActionContainerPrivate *container) const -> bool
{
  return qobject_cast<TouchBarActionContainer*>(container);
}

auto TouchBarActionContainer::updateInternal() -> bool
{
  return false;
}

} // namespace Internal

/*!
    Adds a separator to the end of \a group to the action container.

    Returns the created separator.
*/
auto ActionContainer::addSeparator(const Id group_id) -> Command*
{
  static const Context context(Constants::C_GLOBAL);
  return addSeparator(context, group_id);
}

} // namespace Core
