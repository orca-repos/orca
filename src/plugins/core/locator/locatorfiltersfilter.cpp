// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "locatorfiltersfilter.hpp"
#include "locator.hpp"
#include "locatorwidget.hpp"

#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

using namespace Core;
using namespace Core::Internal;

Q_DECLARE_METATYPE(ILocatorFilter*)

LocatorFiltersFilter::LocatorFiltersFilter(): m_icon(Utils::Icons::NEXT.icon())
{
  setId("FiltersFilter");
  setDisplayName(tr("Available filters"));
  setDefaultIncludedByDefault(true);
  setHidden(true);
  setPriority(Highest);
  setConfigurable(false);
}

auto LocatorFiltersFilter::prepareSearch(const QString &entry) -> void
{
  m_filter_shortcut_strings.clear();
  m_filter_display_names.clear();
  m_filter_descriptions.clear();

  if (!entry.isEmpty())
    return;

  QMap<QString, ILocatorFilter*> unique_filters;

  for (const auto all_filters = Locator::filters(); auto filter : all_filters) {
    const QString filter_id = filter->shortcutString() + ',' + filter->displayName();
    unique_filters.insert(filter_id, filter);
  }

  for (const auto filter : qAsConst(unique_filters)) {
    if (!filter->shortcutString().isEmpty() && !filter->isHidden() && filter->isEnabled()) {
      m_filter_shortcut_strings.append(filter->shortcutString());
      m_filter_display_names.append(filter->displayName());
      m_filter_descriptions.append(filter->description());
    }
  }
}

auto LocatorFiltersFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  Q_UNUSED(entry) // search is already done in the GUI thread in prepareSearch
  QList<LocatorFilterEntry> entries;

  for (auto i = 0; i < m_filter_shortcut_strings.size(); ++i) {
    if (future.isCanceled())
      break;

    LocatorFilterEntry filter_entry(this, m_filter_shortcut_strings.at(i), i, m_icon);
    filter_entry.extra_info = m_filter_display_names.at(i);
    filter_entry.tool_tip = m_filter_descriptions.at(i);
    entries.append(filter_entry);
  }
  return entries;
}

auto LocatorFiltersFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(selection_length)
  bool ok;
  const auto index = selection.internal_data.toInt(&ok);
  QTC_ASSERT(ok && index >= 0 && index < m_filter_shortcut_strings.size(), return);

  if (const auto shortcut_string = m_filter_shortcut_strings.at(index); !shortcut_string.isEmpty()) {
    *new_text = shortcut_string + ' ';
    *selection_start = shortcut_string.length() + 1;
  }
}
