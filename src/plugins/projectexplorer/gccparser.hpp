// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"

#include <projectexplorer/task.hpp>

#include <QRegularExpression>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT GccParser : public OutputTaskParser {
  Q_OBJECT

public:
  GccParser();

  static auto id() -> Utils::Id;
  static auto gccParserSuite() -> QList<OutputLineParser*>;

protected:
  auto createOrAmendTask(Task::TaskType type, const QString &description, const QString &originalLine, bool forceAmend = false, const Utils::FilePath &file = {}, int line = -1, int column = 0, const LinkSpecs &linkSpecs = {}) -> void;
  auto flush() -> void override;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto isContinuation(const QString &newLine) const -> bool;

  QRegularExpression m_regExp;
  QRegularExpression m_regExpScope;
  QRegularExpression m_regExpIncluded;
  QRegularExpression m_regExpInlined;
  QRegularExpression m_regExpGccNames;
  QRegularExpression m_regExpCc1plus;
  Task m_currentTask;
  LinkSpecs m_linkSpecs;
  int m_lines = 0;
  bool m_requiredFromHereFound = false;
};

} // namespace ProjectExplorer
