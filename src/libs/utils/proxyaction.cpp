// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "proxyaction.h"

#include "stringutils.h"

using namespace Utils;

ProxyAction::ProxyAction(QObject *parent) : QAction(parent)
{
  connect(this, &QAction::changed, this, &ProxyAction::updateToolTipWithKeySequence);
  updateState();
}

auto ProxyAction::setAction(QAction *action) -> void
{
  if (m_action == action)
    return;
  disconnectAction();
  m_action = action;
  connectAction();
  updateState();
  emit currentActionChanged(action);
}

auto ProxyAction::updateState() -> void
{
  if (m_action) {
    update(m_action, false);
  } else {
    // no active/delegate action, "visible" action is not enabled/visible
    if (hasAttribute(Hide))
      setVisible(false);
    setEnabled(false);
  }
}

auto ProxyAction::disconnectAction() -> void
{
  if (m_action) {
    disconnect(m_action.data(), &QAction::changed, this, &ProxyAction::actionChanged);
    disconnect(this, &ProxyAction::triggered, m_action.data(), &QAction::triggered);
    disconnect(this, &ProxyAction::toggled, m_action.data(), &QAction::setChecked);
  }
}

auto ProxyAction::connectAction() -> void
{
  if (m_action) {
    connect(m_action.data(), &QAction::changed, this, &ProxyAction::actionChanged);
    connect(this, &ProxyAction::triggered, m_action.data(), &QAction::triggered);
    connect(this, &ProxyAction::toggled, m_action.data(), &QAction::setChecked);
  }
}

auto ProxyAction::action() const -> QAction*
{
  return m_action;
}

auto ProxyAction::setAttribute(ProxyAction::Attribute attribute) -> void
{
  m_attributes |= attribute;
  updateState();
}

auto ProxyAction::removeAttribute(ProxyAction::Attribute attribute) -> void
{
  m_attributes &= ~attribute;
  updateState();
}

auto ProxyAction::hasAttribute(ProxyAction::Attribute attribute) -> bool
{
  return (m_attributes & attribute);
}

auto ProxyAction::actionChanged() -> void
{
  update(m_action, false);
}

auto ProxyAction::initialize(QAction *action) -> void
{
  update(action, true);
}

auto ProxyAction::update(QAction *action, bool initialize) -> void
{
  if (!action)
    return;
  disconnect(this, &ProxyAction::changed, this, &ProxyAction::updateToolTipWithKeySequence);
  if (initialize) {
    setSeparator(action->isSeparator());
    setMenuRole(action->menuRole());
  }
  if (hasAttribute(UpdateIcon) || initialize) {
    setIcon(action->icon());
    setIconText(action->iconText());
    setIconVisibleInMenu(action->isIconVisibleInMenu());
  }
  if (hasAttribute(UpdateText) || initialize) {
    setText(action->text());
    m_toolTip = action->toolTip();
    updateToolTipWithKeySequence();
    setStatusTip(action->statusTip());
    setWhatsThis(action->whatsThis());
  }

  setCheckable(action->isCheckable());

  if (!initialize) {
    if (isChecked() != action->isChecked()) {
      if (m_action)
        disconnect(this, &ProxyAction::toggled, m_action.data(), &QAction::setChecked);
      setChecked(action->isChecked());
      if (m_action)
        connect(this, &ProxyAction::toggled, m_action.data(), &QAction::setChecked);
    }
    setEnabled(action->isEnabled());
    setVisible(action->isVisible());
  }
  connect(this, &ProxyAction::changed, this, &ProxyAction::updateToolTipWithKeySequence);
}

auto ProxyAction::shortcutVisibleInToolTip() const -> bool
{
  return m_showShortcut;
}

auto ProxyAction::setShortcutVisibleInToolTip(bool visible) -> void
{
  m_showShortcut = visible;
  updateToolTipWithKeySequence();
}

auto ProxyAction::updateToolTipWithKeySequence() -> void
{
  if (m_block)
    return;
  m_block = true;
  if (!m_showShortcut || shortcut().isEmpty())
    setToolTip(m_toolTip);
  else
    setToolTip(stringWithAppendedShortcut(m_toolTip, shortcut()));
  m_block = false;
}

auto ProxyAction::stringWithAppendedShortcut(const QString &str, const QKeySequence &shortcut) -> QString
{
  const QString s = stripAccelerator(str);
  return QString::fromLatin1("%1 <span style=\"color: gray; font-size: small\">%2</span>").arg(s, shortcut.toString(QKeySequence::NativeText));
}

auto ProxyAction::proxyActionWithIcon(QAction *original, const QIcon &newIcon) -> ProxyAction*
{
  auto proxyAction = new ProxyAction(original);
  proxyAction->setAction(original);
  proxyAction->setIcon(newIcon);
  proxyAction->setAttribute(UpdateText);
  return proxyAction;
}
