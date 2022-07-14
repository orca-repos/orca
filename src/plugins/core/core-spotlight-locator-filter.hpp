// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-base-file-filter.hpp"

namespace Orca::Plugin::Core {

class SpotlightLocatorFilter final : public BaseFileFilter {
  Q_OBJECT

public:
  SpotlightLocatorFilter();

  using ILocatorFilter::openConfigDialog;

  auto prepareSearch(const QString &entry) -> void override;
  auto openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool override;

protected:
  auto saveState(QJsonObject &obj) const -> void override;
  auto restoreState(const QJsonObject &obj) -> void override;

private:
  auto reset() -> void;

  QString m_command;
  QString m_arguments;
  QString m_case_sensitive_arguments;
};

} // namespace Orca::Plugin::Core
