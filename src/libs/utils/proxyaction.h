// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QPointer>
#include <QAction>

namespace Utils {

class ORCA_UTILS_EXPORT ProxyAction : public QAction {
  Q_OBJECT

public:
  enum Attribute {
    Hide = 0x01,
    UpdateText = 0x02,
    UpdateIcon = 0x04
  };

  Q_DECLARE_FLAGS(Attributes, Attribute)

  explicit ProxyAction(QObject *parent = nullptr);

  auto initialize(QAction *action) -> void;
  auto setAction(QAction *action) -> void;
  auto action() const -> QAction*;
  auto shortcutVisibleInToolTip() const -> bool;
  auto setShortcutVisibleInToolTip(bool visible) -> void;
  auto setAttribute(Attribute attribute) -> void;
  auto removeAttribute(Attribute attribute) -> void;
  auto hasAttribute(Attribute attribute) -> bool;

  static auto stringWithAppendedShortcut(const QString &str, const QKeySequence &shortcut) -> QString;
  static auto proxyActionWithIcon(QAction *original, const QIcon &newIcon) -> ProxyAction*;

signals:
  auto currentActionChanged(QAction *action) -> void;

private:
  auto actionChanged() -> void;
  auto updateState() -> void;
  auto updateToolTipWithKeySequence() -> void;
  auto disconnectAction() -> void;
  auto connectAction() -> void;
  auto update(QAction *action, bool initialize) -> void;

  QPointer<QAction> m_action;
  Attributes m_attributes;
  bool m_showShortcut = false;
  QString m_toolTip;
  bool m_block = false;
};

} // namespace Utils

Q_DECLARE_OPERATORS_FOR_FLAGS(Utils::ProxyAction::Attributes)
