// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/ioutputparser.hpp>
#include <projectexplorer/task.hpp>

namespace QtSupport {
namespace Internal {

class QtTestParser : public ProjectExplorer::OutputTaskParser {
  Q_OBJECT
  
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override { emitCurrentTask(); }
  auto emitCurrentTask() -> void;

  ProjectExplorer::Task m_currentTask;
};

} // namespace Internal
} // namespace QtSupport
