// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"
#include "task.hpp"

#include <QRegularExpression>
#include <QString>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT MsvcParser : public OutputTaskParser {
  Q_OBJECT

public:
  MsvcParser();

  static auto id() -> Utils::Id;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override;
  auto processCompileLine(const QString &line) -> Result;

  QRegularExpression m_compileRegExp;
  QRegularExpression m_additionalInfoRegExp;
  Task m_lastTask;
  LinkSpecs m_linkSpecs;
  int m_lines = 0;
};

class PROJECTEXPLORER_EXPORT ClangClParser : public OutputTaskParser {
  Q_OBJECT

public:
  ClangClParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override;

  const QRegularExpression m_compileRegExp;
  Task m_lastTask;
  int m_linkedLines = 0;
};

} // namespace ProjectExplorer
