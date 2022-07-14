// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-command.hpp"
#include "core-locator-filter-interface.hpp"

#include <extensionsystem/iplugin.hpp>

#include <QFuture>
#include <QObject>
#include <QTimer>

#include <functional>

namespace Orca::Plugin::Core {

class LocatorData;

class Locator final : public QObject {
  Q_OBJECT

public:
  Locator();
  ~Locator() override;

  static auto instance() -> Locator*;
  auto aboutToShutdown(const std::function<void()> &emit_asynchronous_shutdown_finished) -> ExtensionSystem::IPlugin::ShutdownFlag;
  auto initialize() -> void;
  auto extensionsInitialized() -> void;
  auto delayedInitialize() -> bool;
  static auto filters() -> QList<ILocatorFilter*>;
  auto customFilters() -> QList<ILocatorFilter*>;
  auto setFilters(QList<ILocatorFilter*> f) -> void;
  auto setCustomFilters(QList<ILocatorFilter*> filters) -> void;
  auto refreshInterval() const -> int;
  auto setRefreshInterval(int interval) -> void;

signals:
  auto filtersChanged() -> void;

public slots:
  auto refresh(QList<ILocatorFilter*> filters) -> void;
  auto saveSettings() const -> void;

private:
  auto loadSettings() -> void;
  auto updateFilterActions() -> void;
  auto updateEditorManagerPlaceholderText() -> void;

  LocatorData *m_locator_data = nullptr;
  bool m_shutting_down = false;
  bool m_settings_initialized = false;
  QList<ILocatorFilter*> m_filters;
  QList<ILocatorFilter*> m_custom_filters;
  QMap<Utils::Id, QAction*> m_filter_action_map;
  QTimer m_refresh_timer;
  QFuture<void> m_refresh_task;
  QList<ILocatorFilter*> m_refreshing_filters;
};

} // namespace Orca::Plugin::Core
