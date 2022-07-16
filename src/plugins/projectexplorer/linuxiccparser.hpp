// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"
#include "task.hpp"

#include <QRegularExpression>

namespace ProjectExplorer {

class LinuxIccParser : public OutputTaskParser {
  Q_OBJECT

public:
  LinuxIccParser();

  static auto id() -> Utils::Id;
  static auto iccParserSuite() -> QList<OutputLineParser*>;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override;

  QRegularExpression m_firstLine;
  QRegularExpression m_continuationLines;
  QRegularExpression m_caretLine;
  QRegularExpression m_pchInfoLine;
  bool m_expectFirstLine = true;
  Task m_temporary;
  int m_lines = 0;
};

} // namespace ProjectExplorer
