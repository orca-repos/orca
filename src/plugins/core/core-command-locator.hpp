// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-locator-filter-interface.hpp"

namespace Orca::Plugin::Core {

/* Command locators: Provides completion for a set of
 * Core::Command's by sub-string of their action's text. */
class Command;
struct CommandLocatorPrivate;

class CORE_EXPORT CommandLocator final : public ILocatorFilter {
  Q_OBJECT

public:
  CommandLocator(Utils::Id id, const QString &display_name, const QString &short_cut_string, QObject *parent = nullptr);
  ~CommandLocator() override;

  auto appendCommand(Command *cmd) const -> void;
  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &entry, QString *new_text, int *selection_start, int *selection_length) const -> void override;

private:
  CommandLocatorPrivate *d = nullptr;
};

} // namespace Orca::Plugin::Core
