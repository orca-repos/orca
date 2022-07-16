// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-context-interface.hpp"

#include <QDebug>

namespace Orca::Plugin::Core {

auto operator<<(QDebug debug, const Context &context) -> QDebug
{
  debug.nospace() << "Context(";
  auto it = context.begin();
  const auto end = context.end();

  if (it != end) {
    debug << *it;
    ++it;
  }

  while (it != end) {
    debug << ", " << *it;
    ++it;
  }

  debug << ')';

  return debug;
}

} // namespace Orca::Plugin::Core

/*!
    \class Orca::Plugin::Core::Context
    \inheaderfile coreplugin/icontext.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The Context class implements a list of context IDs.

    Contexts are used for registering actions with Orca::Plugin::Core::ActionManager, and
    when creating UI elements that provide a context for actions.

    See \l{The Action Manager and Commands} for an overview of how contexts are
    used.

    \sa Orca::Plugin::Core::IContext
    \sa Orca::Plugin::Core::ActionManager
    \sa {The Action Manager and Commands}
*/

/*!
    \typedef Orca::Plugin::Core::Context::const_iterator

    \brief The Context::const_iterator provides an STL-style const interator for
    Context.
*/

/*!
    \fn Orca::Plugin::Core::Context::Context()

    Creates a context list that represents the global context.
*/

/*!
    \fn Orca::Plugin::Core::Context::Context(Utils::Id c1)

    Creates a context list with a single ID \a c1.
*/

/*!
    \fn Orca::Plugin::Core::Context::Context(Utils::Id c1, Utils::Id c2)

    Creates a context list with IDs \a c1 and \a c2.
*/

/*!
    \fn Orca::Plugin::Core::Context::Context(Utils::Id c1, Utils::Id c2, Utils::Id c3)

    Creates a context list with IDs \a c1, \a c2 and \a c3.
*/

/*!
    \fn bool Orca::Plugin::Core::Context::contains(Utils::Id c) const

    Returns whether this context list contains the ID \a c.
*/

/*!
    \fn int Orca::Plugin::Core::Context::size() const

    Returns the number of IDs in the context list.
*/

/*!
    \fn bool Orca::Plugin::Core::Context::isEmpty() const

    Returns whether this context list is empty and therefore default
    constructed.
*/

/*!
    \fn Utils::Id Orca::Plugin::Core::Context::at(int i) const

    Returns the ID at index \a i in the context list.
*/

/*!
    \fn Orca::Plugin::Core::Context::const_iterator Orca::Plugin::Core::Context::begin() const

    Returns an STL-style iterator pointing to the first ID in the context list.
*/

/*!
    \fn Orca::Plugin::Core::Context::const_iterator Orca::Plugin::Core::Context::end() const

    Returns an STL-style iterator pointing to the imaginary item after the last
    ID in the context list.
*/

/*!
    \fn int Orca::Plugin::Core::Context::indexOf(Utils::Id c) const

    Returns the index position of the ID \a c in the context list. Returns -1
    if no item matched.
*/

/*!
    \fn void Orca::Plugin::Core::Context::removeAt(int i)

    Removes the ID at index \a i from the context list.
*/

/*!
    \fn void Orca::Plugin::Core::Context::prepend(Utils::Id c)

    Adds the ID \a c as the first item to the context list.
*/

/*!
    \fn void Orca::Plugin::Core::Context::add(const Orca::Plugin::Core::Context &c)

    Adds the context list \a c at the end of this context list.
*/

/*!
    \fn void Orca::Plugin::Core::Context::add(Utils::Id c)

    Adds the ID \a c at the end of the context list.
*/

/*!
    \fn bool Orca::Plugin::Core::Context::operator==(const Orca::Plugin::Core::Context &c) const
    \internal
*/

/*!
    \class Orca::Plugin::Core::IContext
    \inheaderfile coreplugin/icontext.h
    \inmodule Orca
    \ingroup mainclasses

    \brief The IContext class associates a widget with a context list and
    context help.

    An instance of IContext must be registered with
    Orca::Plugin::Core::ICore::addContextObject() to have an effect. For many subclasses of
    IContext, like Orca::Plugin::Core::IEditor and Orca::Plugin::Core::IMode, this is done automatically.
    But instances of IContext can be created manually to associate a context
    and context help for an arbitrary widget, too. IContext instances are
    automatically unregistered when they are deleted. Use
    Orca::Plugin::Core::ICore::removeContextObject() if you need to unregister an IContext
    instance manually.

    Whenever the widget is part of the application wide focus widget's parent
    chain, the associated context list is made active. This makes actions active
    that were registered for any of the included context IDs. If the user
    requests context help, the top-most IContext instance in the focus widget's
    parent hierarchy is asked to provide it.

    See \l{The Action Manager and Commands} for an overview of how contexts are
    used for managing actions.

    \sa Orca::Plugin::Core::ICore
    \sa Orca::Plugin::Core::Context
    \sa Orca::Plugin::Core::ActionManager
    \sa {The Action Manager and Commands}
*/

/*!
    \fn Orca::Plugin::Core::IContext::IContext(QObject *parent)

    Creates an IContext with an optional \a parent.
*/

/*!
    \fn Orca::Plugin::Core::Context Orca::Plugin::Core::IContext::context() const

    Returns the context list associated with this IContext.

    \sa setContext()
*/

/*!
    \fn QWidget *Core::IContext::widget() const

    Returns the widget associated with this IContext.

    \sa setWidget()
*/

/*!
    \typedef Orca::Plugin::Core::IContext::HelpCallback

    The HelpCallback class defines the callback function that is used to report
    the help item to show when the user requests context help.
*/

/*!
    \fn void Orca::Plugin::Core::IContext::contextHelp(const Orca::Plugin::Core::IContext::HelpCallback &callback) const

    Called when the user requests context help and this IContext is the top-most
    in the application focus widget's parent hierarchy. Implementations must
    call the passed \a callback with the resulting help item.
    The default implementation returns an help item with the help ID that was
    set with setContextHelp().

    \sa setContextHelp()
*/

/*!
    \fn void Orca::Plugin::Core::IContext::setContext(const Orca::Plugin::Core::Context &context)

    Sets the context list associated with this IContext to \a context.

    \sa context()
*/

/*!
    \fn void Orca::Plugin::Core::IContext::setWidget(QWidget *widget)

    Sets the widget associated with this IContext to \a widget.

    \sa widget()
*/

/*!
    \fn void Orca::Plugin::Core::IContext::setContextHelp(const Orca::Plugin::Core::HelpItem &id)

    Sets the context help item associated with this IContext to \a id.

    \sa contextHelp()
*/
