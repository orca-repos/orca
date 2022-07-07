// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>
#include <core/icontext.hpp>

#include <QObject>

QT_BEGIN_NAMESPACE
class QMenu;
class QMenuBar;
class QAction;
QT_END_NAMESPACE

namespace Utils {
class TouchBar;
}

namespace Core {

class Command;

class CORE_EXPORT ActionContainer : public QObject {
  Q_OBJECT

public:
  enum class on_all_disabled_behavior {
    disable,
    hide,
    show
  };

  virtual auto setOnAllDisabledBehavior(on_all_disabled_behavior behavior) -> void = 0;
  virtual auto onAllDisabledBehavior() const -> on_all_disabled_behavior = 0;
  virtual auto id() const -> Utils::Id = 0;
  virtual auto menu() const -> QMenu* = 0;
  virtual auto menuBar() const -> QMenuBar* = 0;
  virtual auto touchBar() const -> Utils::TouchBar* = 0;
  virtual auto insertLocation(Utils::Id group) const -> QAction* = 0;
  virtual auto appendGroup(Utils::Id group) -> void = 0;
  virtual auto insertGroup(Utils::Id before, Utils::Id group) -> void = 0;
  virtual auto addAction(Command *action, Utils::Id group = {}) -> void = 0;
  virtual auto addMenu(ActionContainer *menu, Utils::Id group = {}) -> void = 0;
  virtual auto addMenu(ActionContainer *before, ActionContainer *menu) -> void = 0;
  auto addSeparator(Utils::Id group_id = {}) -> Command*;
  virtual auto addSeparator(const Context &context, Utils::Id group = {}, QAction **out_separator = nullptr) -> Command* = 0;

  // This clears this menu and submenus from all actions and submenus.
  // It does not destroy the submenus and commands, just removes them from their parents.
  virtual auto clear() -> void = 0;
};

} // namespace Core
