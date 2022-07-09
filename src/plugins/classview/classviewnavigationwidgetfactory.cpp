// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewnavigationwidgetfactory.hpp"
#include "classviewnavigationwidget.hpp"
#include "classviewconstants.hpp"

#include <core/icore.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <utils/qtcassert.hpp>

#include <QKeySequence>
#include <QSettings>

namespace ClassView {
namespace Internal {

///////////////////////////////// NavigationWidgetFactory //////////////////////////////////

/*!
    \class NavigationWidgetFactory
    \brief The NavigationWidgetFactory class implements a singleton instance of
    the INavigationWidgetFactory for the Class View.

    Supports the \c setState public slot for adding the widget factory to or
    removing it from \c ExtensionSystem::PluginManager.
*/

NavigationWidgetFactory::NavigationWidgetFactory()
{
  setDisplayName(tr("Class View"));
  setPriority(500);
  setId("Class View");
}

auto NavigationWidgetFactory::createWidget() -> Core::NavigationView
{
  const auto widget = new NavigationWidget();
  return {widget, widget->createToolButtons()};
}

/*!
   Returns a settings prefix for \a position.
*/
static auto settingsPrefix(int position) -> QString
{
  return QString::fromLatin1("ClassView.Treewidget.%1.FlatMode").arg(position);
}

//! Flat mode settings

auto NavigationWidgetFactory::saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void
{
  const auto pw = qobject_cast<NavigationWidget*>(widget);
  QTC_ASSERT(pw, return);

  // .beginGroup is not used - to prevent simultaneous access
  const auto settingsGroup = settingsPrefix(position);
  settings->setValue(settingsGroup, pw->flatMode());
}

auto NavigationWidgetFactory::restoreSettings(QSettings *settings, int position, QWidget *widget) -> void
{
  const auto pw = qobject_cast<NavigationWidget*>(widget);
  QTC_ASSERT(pw, return);

  // .beginGroup is not used - to prevent simultaneous access
  const auto settingsGroup = settingsPrefix(position);
  pw->setFlatMode(settings->value(settingsGroup, false).toBool());
}

} // namespace Internal
} // namespace ClassView
