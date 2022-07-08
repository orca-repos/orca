// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"

#include <projectexplorer/task.hpp>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT OsParser : public OutputTaskParser {
  Q_OBJECT

public:
  OsParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto hasFatalErrors() const -> bool override { return m_hasFatalError; }
  bool m_hasFatalError = false;
};

} // namespace ProjectExplorer
