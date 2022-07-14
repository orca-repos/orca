// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-locator-filter-interface.hpp"

namespace Orca::Plugin::Core {

CORE_EXPORT auto runSearch(QFutureInterface<LocatorFilterEntry> &future, const QList<ILocatorFilter*> &filters, const QString &search_text) -> void;

} // namespace Orca::Plugin::Core
