// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-locator-filter-interface.hpp"

#include <QIcon>

namespace Orca::Plugin::Core {

class Locator;

/*!
  This filter provides the user with the list of available Locator filters.
  The list is only shown when nothing has been typed yet.
 */
class LocatorFiltersFilter : public ILocatorFilter {
  Q_OBJECT

public:
  LocatorFiltersFilter();

  // ILocatorFilter
  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;

private:
  QStringList m_filter_shortcut_strings;
  QStringList m_filter_display_names;
  QStringList m_filter_descriptions;
  QIcon m_icon;
};

} // namespace Orca::Plugin::Core
