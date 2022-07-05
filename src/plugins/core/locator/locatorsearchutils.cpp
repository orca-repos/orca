// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "locatorsearchutils.h"

#include <QSet>
#include <QString>
#include <QVariant>

auto Core::Internal::runSearch(QFutureInterface<LocatorFilterEntry> &future, const QList<ILocatorFilter*> &filters, const QString &search_text) -> void
{
  QSet<QString> already_added;
  const auto check_duplicates = (filters.size() > 1);
  for (const auto filter : filters) {
    if (future.isCanceled())
      break;

    const auto filter_results = filter->matchesFor(future, search_text);
    QVector<LocatorFilterEntry> unique_filter_results;
    unique_filter_results.reserve(filter_results.size());

    for (const auto &entry : filter_results) {
      if (check_duplicates) {
        if (const auto string_data = entry.internal_data.toString(); !string_data.isEmpty()) {
          if (already_added.contains(string_data))
            continue;
          already_added.insert(string_data);
        }
      }
      unique_filter_results.append(entry);
    }

    if (!unique_filter_results.isEmpty())
      future.reportResults(unique_filter_results);
  }
}
