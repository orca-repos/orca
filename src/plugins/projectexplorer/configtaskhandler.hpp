// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "itaskhandler.hpp"

#include "task.hpp"

namespace ProjectExplorer {
namespace Internal {

class ConfigTaskHandler : public ITaskHandler {
  Q_OBJECT

public:
  ConfigTaskHandler(const Task &pattern, Utils::Id page);

  auto canHandle(const Task &task) const -> bool override;
  auto handle(const Task &task) -> void override;
  auto createAction(QObject *parent) const -> QAction* override;

private:
  const Task m_pattern;
  const Utils::Id m_targetPage;
};

} // namespace Internal
} // namespace ProjectExplorer
