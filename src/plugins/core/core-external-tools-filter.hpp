// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-locator-filter-interface.hpp"

namespace Orca::Plugin::Core {

class ExternalToolsFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  ExternalToolsFilter();

  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  auto prepareSearch(const QString &entry) -> void override;

private:
  QList<LocatorFilterEntry> m_results;
};

} // namespace Orca::Plugin::Core
