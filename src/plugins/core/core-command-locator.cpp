// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-command-locator.hpp"

#include "core-command.hpp"

#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QAction>

using namespace Utils;

namespace Orca::Plugin::Core {

struct CommandLocatorPrivate {
  QList<Command*> commands;
  QList<QPair<int, QString>> commands_data;
};

/*!
    \class Core::CommandLocator
    \inmodule Orca
    \internal
*/

CommandLocator::CommandLocator(const Id id, const QString &display_name, const QString &short_cut_string, QObject *parent) : ILocatorFilter(parent), d(new CommandLocatorPrivate)
{
  setId(id);
  setDisplayName(display_name);
  setDefaultShortcutString(short_cut_string);
}

CommandLocator::~CommandLocator()
{
  delete d;
}

auto CommandLocator::appendCommand(Command *cmd) const -> void
{
  d->commands.push_back(cmd);
}

auto CommandLocator::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  d->commands_data = {};
  const int count = d->commands.size();

  // Get active, enabled actions matching text, store in list.
  // Reference via index in extraInfo.
  for (auto i = 0; i < count; ++i) {
    const auto command = d->commands.at(i);

    if (!command->isActive())
      continue;

    if (const auto action = command->action(); action && action->isEnabled())
      d->commands_data.append(qMakePair(i, action->text()));
  }
}

auto CommandLocator::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> good_entries;
  QList<LocatorFilterEntry> better_entries;
  const auto entry_case_sensitivity = caseSensitivity(entry);

  for (const auto &[fst, snd] : qAsConst(d->commands_data)) {
    if (future.isCanceled())
      break;

    const auto text = stripAccelerator(snd);

    if (const auto index = static_cast<int>(text.indexOf(entry, 0, entry_case_sensitivity)); index >= 0) {
      LocatorFilterEntry filter_entry(this, text, QVariant(fst));
      filter_entry.highlight_info = {index, static_cast<int>(entry.length())};

      if (index == 0)
        better_entries.append(filter_entry);
      else
        good_entries.append(filter_entry);
    }
  }
  better_entries.append(good_entries);
  return better_entries;
}

auto CommandLocator::accept(const LocatorFilterEntry &entry, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)

  // Retrieve action via index.
  const auto index = entry.internal_data.toInt();
  QTC_ASSERT(index >= 0 && index < d->commands.size(), return);
  auto action = d->commands.at(index)->action();

  // avoid nested stack trace and blocking locator by delayed triggering
  QMetaObject::invokeMethod(action, [action] {
    if (action->isEnabled())
      action->trigger();
  }, Qt::QueuedConnection);
}

}  // namespace Orca::Plugin::Core
