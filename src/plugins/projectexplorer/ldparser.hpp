// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"
#include "task.hpp"

#include <QRegularExpression>

namespace ProjectExplorer {

class LdParser : public OutputTaskParser {
  Q_OBJECT

public:
  LdParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override;

  QRegularExpression m_ranlib;
  QRegularExpression m_regExpLinker;
  QRegularExpression m_regExpGccNames;
  Task m_incompleteTask;
};

} // namespace ProjectExplorer
