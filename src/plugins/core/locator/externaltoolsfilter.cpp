// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "externaltoolsfilter.h"

#include <core/externaltool.h>
#include <core/externaltoolmanager.h>
#include <core/messagemanager.h>

#include <utils/qtcassert.h>

using namespace Core;
using namespace Internal;

ExternalToolsFilter::ExternalToolsFilter()
{
  setId("Run external tool");
  setDisplayName(tr("Run External Tool"));
  setDescription(tr("Runs an external tool that you have set up in the options (Environment > " "External Tools)."));
  setDefaultShortcutString("x");
  setPriority(Medium);
}

auto ExternalToolsFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &, const QString &) -> QList<LocatorFilterEntry>
{
  return m_results;
}

auto ExternalToolsFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)

  const auto tool = selection.internal_data.value<ExternalTool*>();
  QTC_ASSERT(tool, return);

  if (const auto runner = new ExternalToolRunner(tool); runner->hasError())
    MessageManager::writeFlashing(runner->errorString());
}

auto ExternalToolsFilter::prepareSearch(const QString &entry) -> void
{
  QList<LocatorFilterEntry> best_entries;
  QList<LocatorFilterEntry> better_entries;
  QList<LocatorFilterEntry> good_entries;

  const auto entry_case_sensitivity = caseSensitivity(entry);

  for (const auto external_tools_by_id = ExternalToolManager::toolsById(); auto tool : external_tools_by_id) {
    auto index = static_cast<int>(tool->displayName().indexOf(entry, 0, entry_case_sensitivity));
    auto h_data_type = LocatorFilterEntry::HighlightInfo::DisplayName;

    if (index < 0) {
      index = static_cast<int>(tool->description().indexOf(entry, 0, entry_case_sensitivity));
      h_data_type = LocatorFilterEntry::HighlightInfo::ExtraInfo;
    }

    if (index >= 0) {
      LocatorFilterEntry filter_entry(this, tool->displayName(), QVariant::fromValue(tool));
      filter_entry.extra_info = tool->description();
      filter_entry.highlight_info = LocatorFilterEntry::HighlightInfo(index, static_cast<int>(entry.length()), h_data_type);

      if (filter_entry.display_name.startsWith(entry, entry_case_sensitivity))
        best_entries.append(filter_entry);
      else if (filter_entry.display_name.contains(entry, entry_case_sensitivity))
        better_entries.append(filter_entry);
      else
        good_entries.append(filter_entry);
    }
  }
  m_results = best_entries + better_entries + good_entries;
}
