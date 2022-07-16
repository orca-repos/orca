// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "itaskhandler.hpp"

namespace ProjectExplorer {
namespace Internal {

class ShowInEditorTaskHandler : public ITaskHandler {
  Q_OBJECT

public:
  auto isDefaultHandler() const -> bool override { return true; }
  auto canHandle(const Task &) const -> bool override;
  auto handle(const Task &task) -> void override;
  auto createAction(QObject *parent) const -> QAction* override;
};

} // namespace Internal
} // namespace ProjectExplorer
