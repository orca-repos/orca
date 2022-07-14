// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-menu-bar-filter.hpp"

#include "core-action-container.hpp"
#include "core-action-manager.hpp"
#include "core-constants.hpp"
#include "core-interface.hpp"
#include "core-locator-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/porting.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QMenuBar>
#include <QPointer>
#include <QRegularExpression>

QT_BEGIN_NAMESPACE
auto qHash(const QPointer<QAction> &p, const Utils::QHashValueType seed) -> Utils::QHashValueType
{
  return qHash(p.data(), seed);
}
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

MenuBarFilter::MenuBarFilter()
{
  setId("Actions from the menu");
  setDisplayName(tr("Actions from the Menu"));
  setDescription(tr("Triggers an action from the menu. Matches any part of a menu hierarchy, separated by " "\">\". For example \"sess def\" matches \"File > Sessions > Default\"."));
  setDefaultShortcutString("t");
  connect(ICore::instance(), &ICore::contextAboutToChange, this, [this] {
    if (LocatorManager::locatorHasFocus())
      updateEnabledActionCache();
  });
}

static auto menuBarActions() -> QList<QAction*>
{
  const auto menu_bar = ActionManager::actionContainer(MENU_BAR)->menuBar();
  QTC_ASSERT(menu_bar, return {});
  return menu_bar->actions();
}

auto MenuBarFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  Q_UNUSED(future)
  Q_UNUSED(entry)
  return std::move(m_entries);
}

auto MenuBarFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)

  if (auto action = selection.internal_data.value<QPointer<QAction>>()) {
    QMetaObject::invokeMethod(action, [action] {
      if (action->isEnabled())
        action->trigger();
    }, Qt::QueuedConnection);
  }
}

auto MenuBarFilter::matchesForAction(QAction *action, const QStringList &entry_path, const QStringList &path, QVector<const QMenu*> &processed_menus) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> entries;
  if (!m_enabled_actions.contains(action))
    return entries;

  const auto whats_this = action->whatsThis();
  const QString text = Utils::stripAccelerator(action->text()) + (whats_this.isEmpty() ? QString() : QString(" (" + whats_this + ")"));

  if (auto menu = action->menu()) {
    if (processed_menus.contains(menu))
      return entries;
    processed_menus.append(menu);
    if (menu->isEnabled()) {
      const auto &actions = menu->actions();
      auto menu_path(path);
      menu_path << text;
      for (auto menu_action : actions)
        entries << matchesForAction(menu_action, entry_path, menu_path, processed_menus);
    }
  } else if (!text.isEmpty()) {
    auto entry_index = 0;
    auto entry_length = 0;
    auto highlight_type = LocatorFilterEntry::HighlightInfo::DisplayName;
    const auto path_text = path.join(" > ");
    auto action_path(path);

    if (!entry_path.isEmpty()) {
      auto path_index = 0;
      action_path << text;

      for (const auto &entry : entry_path) {
        const QRegularExpression re(".*" + entry + ".*", QRegularExpression::CaseInsensitiveOption);
        path_index = static_cast<int>(action_path.indexOf(re, path_index));
        if (path_index < 0)
          return entries;
      }

      const auto &last_entry(entry_path.last());
      entry_length = static_cast<int>(last_entry.length());
      entry_index = static_cast<int>(text.indexOf(last_entry, 0, Qt::CaseInsensitive));

      if (entry_index >= 0) {
        highlight_type = LocatorFilterEntry::HighlightInfo::DisplayName;
      } else {
        entry_index = static_cast<int>(path_text.indexOf(last_entry, 0, Qt::CaseInsensitive));
        QTC_ASSERT(entry_index >= 0, return entries);
        highlight_type = LocatorFilterEntry::HighlightInfo::ExtraInfo;
      }
    }

    LocatorFilterEntry filter_entry(this, text, QVariant(), action->icon());
    filter_entry.internal_data.setValue(QPointer<QAction>(action));
    filter_entry.extra_info = path_text;
    filter_entry.highlight_info = {entry_index, entry_length, highlight_type};
    entries << filter_entry;
  }
  return entries;
}

static auto requestMenuUpdate(const QAction *action) -> void
{
  if (const auto menu = action->menu()) {
    emit menu->aboutToShow();
    for (const auto &actions = menu->actions(); const auto &menu_actions : actions)
      requestMenuUpdate(menu_actions);
  }
}

auto MenuBarFilter::updateEnabledActionCache() -> void
{
  m_enabled_actions.clear();
  auto queue = menuBarActions();

  for (const auto action : qAsConst(queue))
    requestMenuUpdate(action);

  while (!queue.isEmpty()) {
    if (const auto action = queue.takeFirst(); action->isEnabled()) {
      m_enabled_actions.insert(action);
      if (const auto menu = action->menu()) {
        if (menu->isEnabled())
          queue.append(menu->actions());
      }
    }
  }
}

auto MenuBarFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  static const QString separators = ". >/";
  static const QRegularExpression seperator_reg_exp(QString("[%1]").arg(separators));
  auto normalized = entry;
  normalized.replace(seperator_reg_exp, separators.at(0));
  const auto entry_path = normalized.split(separators.at(0), Qt::SkipEmptyParts);
  m_entries.clear();
  QVector<const QMenu*> processed_menus;

  for (const auto action : menuBarActions())
    m_entries << matchesForAction(action, entry_path, QStringList(), processed_menus);
}

} // namespace Orca::Plugin::Core
