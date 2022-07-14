// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"
#include "core-locator-filter-interface.hpp"
#include "ui_core-url-locator-filter.h"

#include <QMutex>

namespace Orca::Plugin::Core {

class CORE_EXPORT UrlLocatorFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  explicit UrlLocatorFilter(Utils::Id id);
  UrlLocatorFilter(const QString &display_name, Utils::Id id);
  ~UrlLocatorFilter() override;

  // ILocatorFilter
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;
  auto restoreState(const QByteArray &state) -> void override;
  auto openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool override;
  auto addDefaultUrl(const QString &url_template) -> void;
  auto remoteUrls() const -> QStringList;
  auto setIsCustomFilter(bool value) -> void;
  auto isCustomFilter() const -> bool;

protected:
  auto saveState(QJsonObject &object) const -> void override;
  auto restoreState(const QJsonObject &object) -> void override;

private:
  QString m_default_display_name;
  QStringList m_default_urls;
  QStringList m_remote_urls;
  bool m_is_custom_filter = false;
  mutable QMutex m_mutex;
};

class UrlFilterOptions final : public QDialog {
  Q_OBJECT
  friend class UrlLocatorFilter;

public:
  explicit UrlFilterOptions(UrlLocatorFilter *filter, QWidget *parent = nullptr);

private:
  auto addNewItem() const -> void;
  auto removeItem() const -> void;
  auto moveItemUp() const -> void;
  auto moveItemDown() const -> void;
  auto updateActionButtons() const -> void;

  UrlLocatorFilter *m_filter = nullptr;
  Ui::UrlFilterOptions m_ui{};
};

} // namespace Orca::Plugin::Core
