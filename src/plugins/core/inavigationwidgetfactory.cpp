// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "inavigationwidgetfactory.hpp"

#include <QKeySequence>

/*!
    \class Core::INavigationWidgetFactory
    \inheaderfile coreplugin/inavigationwidgetfactory.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The INavigationWidgetFactory class provides new instances of navigation widgets.

    A navigation widget factory is necessary because there can be more than one navigation widget of
    the same type at a time. Each navigation widget is wrapped in a \l{Core::NavigationView} for
    delivery.
*/

/*!
    \class Core::NavigationView
    \inheaderfile coreplugin/inavigationwidgetfactory.h
    \inmodule Qt Creator
    \brief The NavigationView class is a C struct for wrapping a widget and a list of tool buttons.
    Wrapping the widget that is shown in the content area of the navigation widget and a list of
    tool buttons that is shown in the header above it.
*/

/*!
    \fn QString Core::INavigationWidgetFactory::displayName() const

    Returns the display name of the navigation widget, which is shown in the dropdown menu above the
    navigation widget.
*/

/*!
    \fn int Core::INavigationWidgetFactory::priority() const

    Determines the position of the navigation widget in the dropdown menu.

    0 to 1000 from top to bottom
*/

/*!
    \fn Id Core::INavigationWidgetFactory::id() const

    Returns a unique identifier for referencing the navigation widget factory.
*/

/*!
    \fn Core::NavigationView Core::INavigationWidgetFactory::createWidget()

    Returns a \l{Core::NavigationView} containing the widget and the buttons. The ownership is given
    to the caller.
*/

using namespace Core;

static QList<INavigationWidgetFactory*> g_navigationWidgetFactories;

/*!
    Constructs a navigation widget factory.
*/
INavigationWidgetFactory::INavigationWidgetFactory()
{
  g_navigationWidgetFactories.append(this);
}

INavigationWidgetFactory::~INavigationWidgetFactory()
{
  g_navigationWidgetFactories.removeOne(this);
}

auto INavigationWidgetFactory::allNavigationFactories() -> QList<INavigationWidgetFactory*>
{
  return g_navigationWidgetFactories;
}

/*!
    Sets the display name for the factory to \a displayName.

    \sa displayName()
*/
auto INavigationWidgetFactory::setDisplayName(const QString &display_name) -> void
{
  m_display_name = display_name;
}

/*!
    Sets the \a priority for the factory.

    \sa priority()
*/
auto INavigationWidgetFactory::setPriority(const int priority) -> void
{
  m_priority = priority;
}

/*!
    Sets the \a id for the factory.

    \sa id()
*/
auto INavigationWidgetFactory::setId(const Utils::Id id) -> void
{
  m_id = id;
}

/*!
    Sets the keyboard activation sequence for the factory to \a keys.

    \sa activationSequence()
*/
auto INavigationWidgetFactory::setActivationSequence(const QKeySequence &keys) -> void
{
  m_activation_sequence = keys;
}

/*!
    Returns the keyboard shortcut to activate an instance of a navigation widget.
*/
auto INavigationWidgetFactory::activationSequence() const -> QKeySequence
{
  return m_activation_sequence;
}

/*!
    Stores the \a settings for the \a widget at \a position that was created by this factory
    (the \a position identifies a specific navigation widget).

    \sa INavigationWidgetFactory::restoreSettings()
*/
auto INavigationWidgetFactory::saveSettings(Utils::QtcSettings * /* settings */, int /* position */, QWidget * /* widget */) -> void {}

/*!
    Reads and restores the \a settings for the \a widget at \a position that was created by this
    factory (the \a position identifies a specific navigation widget).

    \sa INavigationWidgetFactory::saveSettings()
*/
auto INavigationWidgetFactory::restoreSettings(QSettings * /* settings */, int /* position */, QWidget * /* widget */) -> void {}
