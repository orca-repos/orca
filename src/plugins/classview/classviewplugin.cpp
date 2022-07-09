// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewplugin.hpp"
#include "classviewmanager.hpp"
#include "classviewnavigationwidgetfactory.hpp"

namespace ClassView {
namespace Internal {

///////////////////////////////// Plugin //////////////////////////////////

/*!
    \class ClassViewPlugin
    \brief The ClassViewPlugin class implements the Class View plugin.

    The Class View shows the namespace and class hierarchy of the currently open
    projects in the sidebar.
*/

class ClassViewPluginPrivate {
public:
  NavigationWidgetFactory navigationWidgetFactory;
  Manager manager;
};

static ClassViewPluginPrivate *dd = nullptr;

ClassViewPlugin::~ClassViewPlugin()
{
  delete dd;
  dd = nullptr;
}

auto ClassViewPlugin::initialize(const QStringList &arguments, QString *errorMessage) -> bool
{
  Q_UNUSED(arguments)
  Q_UNUSED(errorMessage)

  dd = new ClassViewPluginPrivate;

  return true;
}

} // namespace Internal
} // namespace ClassView
