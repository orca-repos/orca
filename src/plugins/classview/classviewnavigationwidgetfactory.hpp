// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/inavigationwidgetfactory.hpp>

namespace ClassView {
namespace Internal {

class NavigationWidgetFactory : public Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  NavigationWidgetFactory();

  //! \implements Core::INavigationWidgetFactory::createWidget
  auto createWidget() -> Core::NavigationView override;
  //! \implements Core::INavigationWidgetFactory::saveSettings
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
  //! \implements Core::INavigationWidgetFactory::restoreSettings
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;
};

} // namespace Internal
} // namespace ClassView
