// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "command.hpp"

#include <core/icontext.hpp>

#include <utils/id.hpp>
#include <utils/proxyaction.hpp>

#include <QList>
#include <QMultiMap>
#include <QPointer>
#include <QKeySequence>

#include <memory>

namespace Core {
namespace Internal {

class CommandPrivate final : public QObject {
  Q_OBJECT

public:
  explicit CommandPrivate(Command *parent);

  auto setCurrentContext(const Context &context) -> void;
  auto addOverrideAction(QAction *action, const Context &context, bool scriptable) -> void;
  auto removeOverrideAction(QAction *action) -> void;
  auto isEmpty() const -> bool;
  auto updateActiveState() -> void;
  auto setActive(bool state) -> void;

  Command *m_q = nullptr;
  Context m_context;
  Command::CommandAttributes m_attributes;
  Utils::Id m_id;
  QList<QKeySequence> m_default_keys;
  QString m_default_text;
  QString m_touch_bar_text;
  QIcon m_touch_bar_icon;
  bool m_is_key_initialized = false;
  Utils::ProxyAction *m_action = nullptr;
  mutable std::unique_ptr<Utils::ProxyAction> m_touch_bar_action;
  QString m_tool_tip;
  QMap<Utils::Id, QPointer<QAction>> m_context_action_map;
  QMap<QAction*, bool> m_scriptable_map;
  bool m_active = false;
  bool m_context_initialized = false;
};

} // namespace Internal
} // namespace Core
