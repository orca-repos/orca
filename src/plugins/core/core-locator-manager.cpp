// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-locator-manager.hpp"

#include "core-interface.hpp"
#include "core-locator-filter-interface.hpp"
#include "core-locator-widget.hpp"
#include "core-locator.hpp"

#include <aggregation/aggregate.hpp>

#include <utils/qtcassert.hpp>

#include <QApplication>

namespace Orca::Plugin::Core {

/*!
    \class Core::LocatorManager
    \inmodule Orca
    \internal
*/

LocatorManager::LocatorManager() = default;

static auto locatorWidget() -> LocatorWidget*
{
  static QPointer<LocatorPopup> popup;
  auto window = ICore::dialogParent()->window();

  // if that is a popup, try to find a better one
  if (window->windowFlags() & Qt::Popup && window->parentWidget())
    window = window->parentWidget()->window();

  if (auto *widget = Aggregation::query<LocatorWidget>(window)) {
    if (popup)
      popup->close();
    return widget;
  }

  if (!popup) {
    popup = createLocatorPopup(Locator::instance(), window);
    popup->show();
  }

  return popup->inputWidget();
}

auto LocatorManager::showFilter(const ILocatorFilter *filter) -> void
{
  QTC_ASSERT(filter, return);
  auto search_text = tr("<type here>");

  // add shortcut string at front or replace existing shortcut string
  if (const auto current_text = locatorWidget()->currentText().trimmed(); !current_text.isEmpty()) {
    search_text = current_text;
    for (const auto all_filters = Locator::filters(); const auto other_filter : all_filters) {
      if (current_text.startsWith(other_filter->shortcutString() + ' ')) {
        search_text = current_text.mid(other_filter->shortcutString().length() + 1);
        break;
      }
    }
  }

  show(filter->shortcutString() + ' ' + search_text, filter->shortcutString().length() + 1, search_text.length());
}

auto LocatorManager::show(const QString &text, const int selection_start, const int selection_length) -> void
{
  locatorWidget()->showText(text, selection_start, selection_length);
}

auto LocatorManager::createLocatorInputWidget(QWidget *window) -> QWidget*
{
  const auto locator_widget = createStaticLocatorWidget(Locator::instance());

  // register locator widget for this window
  const auto agg = new Aggregation::Aggregate;
  agg->add(window);
  agg->add(locator_widget);

  return locator_widget;
}

auto LocatorManager::locatorHasFocus() -> bool
{
  auto w = qApp->focusWidget();

  while (w) {
    if (qobject_cast<LocatorWidget*>(w))
      return true;
    w = w->parentWidget();
  }

  return false;
}

} // namespace Orca::Plugin::Core
