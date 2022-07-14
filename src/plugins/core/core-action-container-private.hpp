// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-action-container.hpp"
#include "core-command.hpp"

#include <utils/touchbar/touchbar.hpp>

namespace Orca::Plugin::Core {

struct Group {
  explicit Group(const Utils::Id id) : id(id) {}
  Utils::Id id;
  QList<QObject*> items; // Command * or ActionContainer *
};

class ActionContainerPrivate : public ActionContainer {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ActionContainerPrivate)

public:
  explicit ActionContainerPrivate(Utils::Id id);
  ~ActionContainerPrivate() override = default;

  auto setOnAllDisabledBehavior(on_all_disabled_behavior behavior) -> void override;
  auto onAllDisabledBehavior() const -> on_all_disabled_behavior override;
  auto insertLocation(Utils::Id group_id) const -> QAction* override;
  auto appendGroup(Utils::Id group_id) -> void override;
  auto insertGroup(Utils::Id before, Utils::Id group_id) -> void override;
  auto addAction(Command *command, Utils::Id group_id = {}) -> void override;
  auto addMenu(ActionContainer *menu, Utils::Id group_id = {}) -> void override;
  auto addMenu(ActionContainer *before, ActionContainer *menu) -> void override;
  auto addSeparator(const Context &context, Utils::Id group_id = {}, QAction **out_separator = nullptr) -> Command* override;
  auto clear() -> void override;
  auto id() const -> Utils::Id override;
  auto menu() const -> QMenu* override;
  auto menuBar() const -> QMenuBar* override;
  auto touchBar() const -> Utils::TouchBar* override;

  virtual auto containerAction() const -> QAction* = 0;
  virtual auto actionForItem(QObject *item) const -> QAction*;
  virtual auto insertAction(QAction *before, Command *command) -> void = 0;
  virtual auto insertMenu(QAction *before, ActionContainer *container) -> void = 0;
  virtual auto removeAction(Command *command) -> void = 0;
  virtual auto removeMenu(ActionContainer *container) -> void = 0;
  virtual auto updateInternal() -> bool = 0;

protected:
  static auto canAddAction(Command *command) -> bool;
  auto canAddMenu(ActionContainer *menu) const -> bool;
  virtual auto canBeAddedToContainer(ActionContainerPrivate *container) const -> bool = 0;

  // groupId --> list of Command* and ActionContainer*
  QList<Group> m_groups;

private:
  auto scheduleUpdate() -> void;
  auto update() -> void;
  auto itemDestroyed() -> void;
  auto findGroup(Utils::Id group_id) const -> QList<Group>::const_iterator;
  auto insertLocation(QList<Group>::const_iterator group) const -> QAction*;

  on_all_disabled_behavior m_on_all_disabled_behavior;
  Utils::Id m_id;
  bool m_update_requested;
};

class MenuActionContainer final : public ActionContainerPrivate {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(MenuActionContainer)

public:
  explicit MenuActionContainer(Utils::Id id);
  ~MenuActionContainer() override;

  auto menu() const -> QMenu* override;
  auto containerAction() const -> QAction* override;
  auto insertAction(QAction *before, Command *command) -> void override;
  auto insertMenu(QAction *before, ActionContainer *container) -> void override;
  auto removeAction(Command *command) -> void override;
  auto removeMenu(ActionContainer *container) -> void override;

protected:
  auto canBeAddedToContainer(ActionContainerPrivate *container) const -> bool override;
  auto updateInternal() -> bool override;

private:
  QPointer<QMenu> m_menu;
};

class MenuBarActionContainer final : public ActionContainerPrivate {
  Q_OBJECT

public:
  explicit MenuBarActionContainer(Utils::Id id);

  auto setMenuBar(QMenuBar *menu_bar) -> void;
  auto menuBar() const -> QMenuBar* override;
  auto containerAction() const -> QAction* override;
  auto insertAction(QAction *before, Command *command) -> void override;
  auto insertMenu(QAction *before, ActionContainer *container) -> void override;
  auto removeAction(Command *command) -> void override;
  auto removeMenu(ActionContainer *container) -> void override;

protected:
  auto canBeAddedToContainer(ActionContainerPrivate *container) const -> bool override;
  auto updateInternal() -> bool override;

private:
  QMenuBar *m_menu_bar;
};

class TouchBarActionContainer final : public ActionContainerPrivate {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TouchBarActionContainer)

public:
  TouchBarActionContainer(Utils::Id id, const QIcon &icon, const QString &text);
  ~TouchBarActionContainer() override;

  auto touchBar() const -> Utils::TouchBar* override;
  auto containerAction() const -> QAction* override;
  auto actionForItem(QObject *item) const -> QAction* override;
  auto insertAction(QAction *before, Command *command) -> void override;
  auto insertMenu(QAction *before, ActionContainer *container) -> void override;
  auto removeAction(Command *command) -> void override;
  auto removeMenu(ActionContainer *container) -> void override;
  auto canBeAddedToContainer(ActionContainerPrivate *container) const -> bool override;
  auto updateInternal() -> bool override;

private:
  std::unique_ptr<Utils::TouchBar> m_touch_bar;
};

} // namespace Orca::Plugin::Core
