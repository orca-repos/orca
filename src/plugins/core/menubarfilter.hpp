// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/ilocatorfilter.hpp>

#include <QAction>
#include <QPointer>
#include <QSet>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

class MenuBarFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  MenuBarFilter();

  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  auto prepareSearch(const QString &entry) -> void override;

private:
  auto matchesForAction(QAction *action, const QStringList &entry_path, const QStringList &path, QVector<const QMenu*> &processed_menus) -> QList<LocatorFilterEntry>;
  auto updateEnabledActionCache() -> void;

  QList<LocatorFilterEntry> m_entries;
  QSet<QPointer<QAction>> m_enabled_actions;
};

} // namespace Internal
} // namespace Core
