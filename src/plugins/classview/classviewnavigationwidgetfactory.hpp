// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-navigation-widget-factory-interface.hpp>

namespace ClassView {
namespace Internal {

class NavigationWidgetFactory : public Orca::Plugin::Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  NavigationWidgetFactory();

  //! \implements Orca::Plugin::Core::INavigationWidgetFactory::createWidget
  auto createWidget() -> Orca::Plugin::Core::NavigationView override;
  //! \implements Orca::Plugin::Core::INavigationWidgetFactory::saveSettings
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
  //! \implements Orca::Plugin::Core::INavigationWidgetFactory::restoreSettings
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;
};

} // namespace Internal
} // namespace ClassView
