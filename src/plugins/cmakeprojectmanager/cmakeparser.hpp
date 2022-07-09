// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <projectexplorer/ioutputparser.hpp>
#include <projectexplorer/task.hpp>

#include <utils/optional.hpp>

#include <QDir>
#include <QRegularExpression>

namespace CMakeProjectManager {

class CMAKE_EXPORT CMakeParser : public ProjectExplorer::OutputTaskParser {
  Q_OBJECT

public:
  explicit CMakeParser();
  auto setSourceDirectory(const QString &sourceDir) -> void;

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto flush() -> void override;

  enum TripleLineError {
    NONE,
    LINE_LOCATION,
    LINE_DESCRIPTION,
    LINE_DESCRIPTION2
  };

  TripleLineError m_expectTripleLineErrorData = NONE;

  Utils::optional<QDir> m_sourceDirectory;
  ProjectExplorer::Task m_lastTask;
  QRegularExpression m_commonError;
  QRegularExpression m_nextSubError;
  QRegularExpression m_commonWarning;
  QRegularExpression m_locationLine;
  bool m_skippedFirstEmptyLine = false;
  int m_lines = 0;
};

} // namespace CMakeProjectManager
