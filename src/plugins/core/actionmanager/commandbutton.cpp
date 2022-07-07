// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "commandbutton.hpp"
#include "actionmanager.hpp"
#include "command.hpp"

#include <utils/proxyaction.hpp>

using namespace Utils;

namespace Core {

/*!
    \class Core::CommandButton
    \inheaderfile coreplugin/actionmanager/commandbutton.h
    \inmodule Orca

    \brief The CommandButton class is a tool button associated with one of
    the registered Command objects.

    Tooltip of this button consists of toolTipBase property value and Command's
    key sequence which is automatically updated when user changes it.
 */

/*!
    \property CommandButton::toolTipBase
    \brief The tool tip base for the command button.
*/

/*!
    \internal
*/
CommandButton::CommandButton(QWidget *parent) : QToolButton(parent), m_command(nullptr)
{
}

/*!
    \internal
*/
CommandButton::CommandButton(const Id id, QWidget *parent) : QToolButton(parent), m_command(nullptr)
{
  setCommandId(id);
}

/*!
    Sets the ID of the command associated with this tool button to \a id.
*/
auto CommandButton::setCommandId(const Id id) -> void
{
  if (m_command)
    disconnect(m_command.data(), &Command::keySequenceChanged, this, &CommandButton::updateToolTip);

  m_command = ActionManager::command(id);

  if (m_tool_tip_base.isEmpty())
    m_tool_tip_base = m_command->description();

  updateToolTip();
  connect(m_command.data(), &Command::keySequenceChanged, this, &CommandButton::updateToolTip);
}

auto CommandButton::toolTipBase() const -> QString
{
  return m_tool_tip_base;
}

auto CommandButton::setToolTipBase(const QString &tool_tip_base) -> void
{
  m_tool_tip_base = tool_tip_base;
  updateToolTip();
}

auto CommandButton::updateToolTip() -> void
{
  if (m_command)
    setToolTip(ProxyAction::stringWithAppendedShortcut(m_tool_tip_base, m_command->keySequence()));
}

} // namespace Core
