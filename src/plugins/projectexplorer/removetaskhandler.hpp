// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "itaskhandler.hpp"

namespace ProjectExplorer {
namespace Internal {

class RemoveTaskHandler : public ITaskHandler {
  Q_OBJECT

public:
  RemoveTaskHandler() : ITaskHandler(true) {}

  auto handle(const Tasks &tasks) -> void override;
  auto createAction(QObject *parent) const -> QAction* override;
};

} // namespace Internal
} // namespace ProjectExplorer
