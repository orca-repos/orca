// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-context-interface.hpp"

#include <utils/id.hpp>

#include <QIcon>
#include <QMenu>

namespace Orca::Plugin::Core {

class CORE_EXPORT IMode : public IContext {
  Q_OBJECT
  Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName) Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
  Q_PROPERTY(int priority READ priority WRITE setPriority) Q_PROPERTY(Utils::Id id READ id WRITE setId)
  Q_PROPERTY(QMenu *menu READ menu WRITE setMenu)
  Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledStateChanged)

public:
  explicit IMode(QObject *parent = nullptr);

  auto displayName() const -> QString { return m_display_name; }
  auto icon() const -> QIcon { return m_icon; }
  auto priority() const -> int { return m_priority; }
  auto id() const -> Utils::Id { return m_id; }
  auto isEnabled() const -> bool;
  auto menu() const -> QMenu* { return m_menu; }
  auto setEnabled(bool enabled) -> void;
  auto setDisplayName(const QString &display_name) -> void { m_display_name = display_name; }
  auto setIcon(const QIcon &icon) -> void { m_icon = icon; }
  auto setPriority(const int priority) -> void { m_priority = priority; }
  auto setId(const Utils::Id id) -> void { m_id = id; }
  auto setMenu(QMenu *menu) -> void { m_menu = menu; }

signals:
  auto enabledStateChanged(bool enabled) -> void;

private:
  QString m_display_name;
  QIcon m_icon;
  QMenu *m_menu = nullptr;
  int m_priority = -1;
  Utils::Id m_id;
  bool m_is_enabled = true;
};

} // namespace Orca::Plugin::Core
