// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "gccparser.hpp"
#include "task.hpp"

#include <QRegularExpression>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT ClangParser : public GccParser {
  Q_OBJECT

public:
  ClangParser();

  static auto clangParserSuite() -> QList<OutputLineParser*>;
  static auto id() -> Utils::Id;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;

  QRegularExpression m_commandRegExp;
  QRegularExpression m_inLineRegExp;
  QRegularExpression m_messageRegExp;
  QRegularExpression m_summaryRegExp;
  QRegularExpression m_codesignRegExp;
  bool m_expectSnippet;
};

} // namespace ProjectExplorer
