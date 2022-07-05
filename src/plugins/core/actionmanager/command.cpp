// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "command.h"
#include "command_p.h"

#include <core/coreconstants.h>
#include <core/icontext.h>

#include <utils/stringutils.h>

#include <QAction>
#include <QToolButton>
#include <QTextStream>

/*!
    \class Core::Command
    \inheaderfile coreplugin/actionmanager/command.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The Command class represents an action, such as a menu item, tool button, or shortcut.

    You do not create Command objects directly, but use \l{ActionManager::registerAction()}
    to register an action and retrieve a Command. The Command object represents the user visible
    action and its properties. If multiple actions are registered with the same ID (but
    different contexts) the returned Command is the shared one between these actions.

    A Command has two basic properties: a list of default shortcuts and a
    default text. The default shortcuts are key sequence that the user can use
    to trigger the active action that the Command represents. The first
    shortcut in that list is the main shortcut that is for example also shown
    in tool tips and menus. The default text is used for representing the
    Command in the keyboard shortcut preference pane. If the default text is
    empty, the text of the visible action is used.

    The user visible action is updated to represent the state of the active action (if any).
    For performance reasons only the enabled and visible state are considered by default though.
    You can tell a Command to also update the actions icon and text by setting the
    corresponding \l{Command::CommandAttribute}{attribute}.

    If there is no active action, the default behavior of the visible action is to be disabled.
    You can change that behavior to make the visible action hide instead via the Command's
    \l{Command::CommandAttribute}{attributes}.

    See \l{The Action Manager and Commands} for an overview of how
    Core::Command and Core::ActionManager interact.

    \sa Core::ActionManager
    \sa {The Action Manager and Commands}
*/

/*!
    \enum Core::Command::CommandAttribute
    This enum defines how the user visible action is updated when the active action changes.
    The default is to update the enabled and visible state, and to disable the
    user visible action when there is no active action.
    \value CA_UpdateText
        Also update the actions text.
    \value CA_UpdateIcon
        Also update the actions icon.
    \value CA_Hide
        When there is no active action, hide the user-visible action, instead of just
        disabling it.
    \value CA_NonConfigurable
        Flag to indicate that the keyboard shortcuts of this Command should not
        be configurable by the user.
*/

/*!
    \fn void Core::Command::setDefaultKeySequence(const QKeySequence &key)

    Sets the default keyboard shortcut that can be used to activate this
    command to \a key. This is used if the user didn't customize the shortcut,
    or resets the shortcut to the default.
*/

/*!
    \fn void Core::Command::setDefaultKeySequences(const QList<QKeySequence> &keys)

    Sets the default keyboard shortcuts that can be used to activate this
    command to \a keys. This is used if the user didn't customize the
    shortcuts, or resets the shortcuts to the default.
*/

/*!
    \fn QList<QKeySequence> Core::Command::defaultKeySequences() const

    Returns the default keyboard shortcuts that can be used to activate this
    command.
    \sa setDefaultKeySequences()
*/

/*!
    \fn void Core::Command::keySequenceChanged()
    Sent when the keyboard shortcuts assigned to this Command change, e.g.
    when the user sets them in the keyboard shortcut settings dialog.
*/

/*!
    \fn QList<QKeySequence> Core::Command::keySequences() const

    Returns the current keyboard shortcuts assigned to this Command.
    \sa defaultKeySequences()
*/

/*!
    \fn QKeySequence Core::Command::keySequence() const

    Returns the current main keyboard shortcut assigned to this Command.
    \sa defaultKeySequences()
*/

/*!
    \fn void Core::Command::setKeySequences(const QList<QKeySequence> &keys)
    \internal
*/

/*!
    \fn void Core::Command::setDescription(const QString &text)
    Sets the \a text that is used to represent the Command in the
    keyboard shortcut settings dialog. If you do not set this,
    the current text from the user visible action is taken (which
    is fine in many cases).
*/

/*!
    \fn QString Core::Command::description() const
    Returns the text that is used to present this Command to the user.
    \sa setDescription()
*/

/*!
    \fn int Core::Command::id() const
    \internal
*/

/*!
    \fn QString Core::Command::stringWithAppendedShortcut(const QString &string) const

    Returns the \a string with an appended representation of the main keyboard
    shortcut that is currently assigned to this Command.
*/

/*!
    \fn QAction *Core::Command::action() const

    Returns the user visible action for this Command. Use this action to put it
    on e.g. tool buttons. The action automatically forwards \c triggered() and
    \c toggled() signals to the action that is currently active for this
    Command. It also shows the current main keyboard shortcut in its tool tip
    (in addition to the tool tip of the active action) and gets disabled/hidden
    when there is no active action for the current context.
*/

/*!
    \fn Core::Context Core::Command::context() const

    Returns the context for this command.
*/

/*!
    \fn void Core::Command::setAttribute(Core::Command::CommandAttribute attribute)
    Adds \a attribute to the attributes of this Command.
    \sa CommandAttribute
    \sa removeAttribute()
    \sa hasAttribute()
*/

/*!
    \fn void Core::Command::removeAttribute(Core::Command::CommandAttribute attribute)
    Removes \a attribute from the attributes of this Command.
    \sa CommandAttribute
    \sa setAttribute()
*/

/*!
    \fn bool Core::Command::hasAttribute(Core::Command::CommandAttribute attribute) const
    Returns whether the Command has the \a attribute set.
    \sa CommandAttribute
    \sa removeAttribute()
    \sa setAttribute()
*/

/*!
    \fn bool Core::Command::isActive() const

    Returns whether the Command has an active action for the current context.
*/

/*!
    \fn bool Core::Command::isScriptable() const
    Returns whether the Command is scriptable. A scriptable command can be called
    from a script without the need for the user to interact with it.
*/

/*!
    \fn bool Core::Command::isScriptable(const Core::Context &) const
    \internal

    Returns whether the Command is scriptable.
*/

/*!
    \fn void Core::Command::activeStateChanged()

    This signal is emitted when the active state of the command changes.
*/

/*!
    \fn virtual void Core::Command::setTouchBarText(const QString &text)

    Sets the text for the action on the touch bar to \a text.
*/

/*!
    \fn virtual QString Core::Command::touchBarText() const

    Returns the text for the action on the touch bar.
*/

/*!
    \fn virtual void Core::Command::setTouchBarIcon(const QIcon &icon)

    Sets the icon for the action on the touch bar to \a icon.
*/

/*! \fn virtual QIcon Core::Command::touchBarIcon() const

    Returns the icon for the action on the touch bar.
*/

/*! \fn virtual QAction *Core::Command::touchBarAction() const

    \internal
*/


namespace Core {

using namespace Internal;
using namespace Utils;

Command::Command(const Id id) : d(new CommandPrivate(this))
{
  d->m_id = id;
}

Command::~Command()
{
  delete d;
}

auto Command::id() const -> Id
{
  return d->m_id;
}

auto Command::setDefaultKeySequence(const QKeySequence &key) -> void
{
  if (!d->m_is_key_initialized)
    setKeySequences({key});
  d->m_default_keys = {key};
}

auto Command::setDefaultKeySequences(const QList<QKeySequence> &keys) -> void
{
  if (!d->m_is_key_initialized)
    setKeySequences(keys);
  d->m_default_keys = keys;
}

auto Command::defaultKeySequences() const -> QList<QKeySequence>
{
  return d->m_default_keys;
}

auto Command::action() const -> QAction*
{
  return d->m_action;
}

auto Command::stringWithAppendedShortcut(const QString &str) const -> QString
{
  return ProxyAction::stringWithAppendedShortcut(str, keySequence());
}

auto Command::context() const -> Context
{
  return d->m_context;
}

auto Command::setKeySequences(const QList<QKeySequence> &keys) -> void
{
  d->m_is_key_initialized = true;
  d->m_action->setShortcuts(keys);
  emit keySequenceChanged();
}

auto Command::keySequences() const -> QList<QKeySequence>
{
  return d->m_action->shortcuts();
}

auto Command::keySequence() const -> QKeySequence
{
  return d->m_action->shortcut();
}

auto Command::setDescription(const QString &text) const -> void
{
  d->m_default_text = text;
}

auto Command::description() const -> QString
{
  if (!d->m_default_text.isEmpty())
    return d->m_default_text;

  if (const auto act = action()) {
    if (auto text = stripAccelerator(act->text()); !text.isEmpty())
      return text;
  }

  return id().toString();
}

auto Command::isActive() const -> bool
{
  return d->m_active;
}


auto Command::isScriptable() const -> bool
{
  return std::ranges::find(std::as_const(d->m_scriptable_map), true) != d->m_scriptable_map.cend();
}

auto Command::isScriptable(const Context &context) const -> bool
{
  if (context == d->m_context && d->m_scriptable_map.contains(d->m_action->action()))
    return d->m_scriptable_map.value(d->m_action->action());

  return std::ranges::any_of(context, [&](const Id &id) {
    return d->m_scriptable_map.contains(d->m_context_action_map.value(id, nullptr));
  });
}

auto Command::setAttribute(const command_attribute attr) const -> void
{
  d->m_attributes |= attr;
  switch (attr) {
  case ca_hide:
    d->m_action->setAttribute(ProxyAction::Hide);
    break;
  case ca_update_text:
    d->m_action->setAttribute(ProxyAction::UpdateText);
    break;
  case ca_update_icon:
    d->m_action->setAttribute(ProxyAction::UpdateIcon);
    break;
  case ca_non_configurable:
    break;
  }
}

auto Command::removeAttribute(const command_attribute attr) const -> void
{
  d->m_attributes &= ~attr;
  switch (attr) {
  case ca_hide:
    d->m_action->removeAttribute(ProxyAction::Hide);
    break;
  case ca_update_text:
    d->m_action->removeAttribute(ProxyAction::UpdateText);
    break;
  case ca_update_icon:
    d->m_action->removeAttribute(ProxyAction::UpdateIcon);
    break;
  case ca_non_configurable:
    break;
  }
}

auto Command::hasAttribute(const command_attribute attr) const -> bool
{
  return (d->m_attributes & attr);
}

auto Command::setTouchBarText(const QString &text) const -> void
{
  d->m_touch_bar_text = text;
}

auto Command::touchBarText() const -> QString
{
  return d->m_touch_bar_text;
}

auto Command::setTouchBarIcon(const QIcon &icon) const -> void
{
  d->m_touch_bar_icon = icon;
}

auto Command::touchBarIcon() const -> QIcon
{
  return d->m_touch_bar_icon;
}

auto Command::touchBarAction() const -> QAction*
{
  if (!d->m_touch_bar_action) {
    d->m_touch_bar_action = std::make_unique<ProxyAction>();
    d->m_touch_bar_action->initialize(d->m_action);
    d->m_touch_bar_action->setIcon(d->m_touch_bar_icon);
    d->m_touch_bar_action->setText(d->m_touch_bar_text);
    // the touch bar action should be hidden if the command is not valid for the context
    d->m_touch_bar_action->setAttribute(ProxyAction::Hide);
    d->m_touch_bar_action->setAction(d->m_action->action());
    connect(d->m_action, &ProxyAction::currentActionChanged, d->m_touch_bar_action.get(), &ProxyAction::setAction);
  }
  return d->m_touch_bar_action.get();
}

/*!
    Appends the main keyboard shortcut that is currently assigned to the action
    \a a to its tool tip.
*/
auto Command::augmentActionWithShortcutToolTip(QAction *a) const -> void
{
  a->setToolTip(stringWithAppendedShortcut(a->text()));

  connect(this, &Command::keySequenceChanged, a, [this, a]() {
    a->setToolTip(stringWithAppendedShortcut(a->text()));
  });

  connect(a, &QAction::changed, this, [this, a]() {
    a->setToolTip(stringWithAppendedShortcut(a->text()));
  });
}

/*!
    Returns a tool button for \a action.

    Appends the main keyboard shortcut \a cmd to the tool tip of the action.
*/
auto Command::toolButtonWithAppendedShortcut(QAction *action, Command *cmd) -> QToolButton*
{
  const auto button = new QToolButton;
  button->setDefaultAction(action);

  if (cmd)
    cmd->augmentActionWithShortcutToolTip(action);

  return button;
}

CommandPrivate::CommandPrivate(Command *parent) : m_q(parent), m_attributes({}), m_action(new ProxyAction(this))
{
  m_action->setShortcutVisibleInToolTip(true);
  connect(m_action, &QAction::changed, this, &CommandPrivate::updateActiveState);
}

auto CommandPrivate::setCurrentContext(const Context &context) -> void
{
  m_context = context;

  QAction *current_action = nullptr;
  for (auto &i : m_context) {
    if (QAction *a = m_context_action_map.value(i, nullptr)) {
      current_action = a;
      break;
    }
  }

  m_action->setAction(current_action);
  updateActiveState();
}

auto CommandPrivate::updateActiveState() -> void
{
  setActive(m_action->isEnabled() && m_action->isVisible() && !m_action->isSeparator());
}

auto CommandPrivate::addOverrideAction(QAction *action, const Context &context, const bool scriptable) -> void
{
  auto msg_action_warning = [](const QAction *new_action, const Id id, const QAction *old_action) -> QString {
    QString msg;
    QTextStream str(&msg);

    str << "addOverrideAction " << new_action->objectName() << '/' << new_action->text() << ": Action ";

    if (old_action)
      str << old_action->objectName() << '/' << old_action->text();

    str << " is already registered for context " << id.toString() << '.';
    return msg;
  };

  // disallow TextHeuristic menu role, because it doesn't work with translations,
  // e.g. QTCREATORBUG-13101
  if (action->menuRole() == QAction::TextHeuristicRole)
    action->setMenuRole(QAction::NoRole);

  if (isEmpty())
    m_action->initialize(action);

  if (context.isEmpty()) {
    m_context_action_map.insert(Constants::C_GLOBAL, action);
  } else {
    for (const auto &id : context) {
      if (m_context_action_map.contains(id))
        qWarning("%s", qPrintable(msg_action_warning(action, id, m_context_action_map.value(id, nullptr))));
      m_context_action_map.insert(id, action);
    }
  }

  m_scriptable_map[action] = scriptable;
  setCurrentContext(m_context);
}

auto CommandPrivate::removeOverrideAction(QAction *action) -> void
{
  QList<Id> to_remove;

  for (auto it = m_context_action_map.cbegin(), end = m_context_action_map.cend(); it != end; ++it) {
    if (it.value() == nullptr || it.value() == action)
      to_remove.append(it.key());
  }

  for (auto &id : to_remove)
    m_context_action_map.remove(id);

  setCurrentContext(m_context);
}

auto CommandPrivate::setActive(const bool state) -> void
{
  if (state != m_active) {
    m_active = state;
    emit m_q->activeStateChanged();
  }
}

auto CommandPrivate::isEmpty() const -> bool
{
  return m_context_action_map.isEmpty();
}

} // namespace Core
