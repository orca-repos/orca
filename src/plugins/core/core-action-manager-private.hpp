// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-context-interface.hpp"

#include <QHash>

namespace Orca::Plugin::Core {

class Command;
class Action;
class ActionContainerPrivate;

class ActionManagerPrivate : public QObject {
  Q_OBJECT

public:
  using IdCmdMap = QHash<Utils::Id, Command*>;
  using IdContainerMap = QHash<Utils::Id, ActionContainerPrivate*>;

  ~ActionManagerPrivate() override;

  auto setContext(const Context &context) -> void;
  auto hasContext(int context) const -> bool;
  auto saveSettings() const -> void;
  static auto saveSettings(const Command *cmd) -> void;
  static auto showShortcutPopup(const QString &shortcut) -> void;
  auto hasContext(const Context &context) const -> bool;
  auto overridableAction(Utils::Id id) -> Command*;
  static auto readUserSettings(Utils::Id id, Command *cmd) -> void;
  auto containerDestroyed() -> void;
  auto actionTriggered() const -> void;

  IdCmdMap m_id_cmd_map;
  IdContainerMap m_id_container_map;
  Context m_context;
  bool m_presentation_mode_enabled = false;
};

} // namespace Orca::Plugin::Core
