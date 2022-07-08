// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"

#include <QRegularExpression>
#include <QStringList>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT GnuMakeParser : public OutputTaskParser {
  Q_OBJECT

public:
  explicit GnuMakeParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto hasFatalErrors() const -> bool override;
  auto emitTask(const Task &task) -> void;

  QRegularExpression m_makeDir;
  QRegularExpression m_makeLine;
  QRegularExpression m_threeStarError;
  QRegularExpression m_errorInMakefile;
  bool m_suppressIssues = false;
  int m_fatalErrorCount = 0;

  #if defined WITH_TESTS
    friend class ProjectExplorerPlugin;
  #endif
};

#if defined WITH_TESTS
class GnuMakeParserTester : public QObject {
  Q_OBJECT

public:
  explicit GnuMakeParserTester(GnuMakeParser *parser, QObject *parent = nullptr);
  void parserIsAboutToBeDeleted();

  Utils::FilePaths directories;
  GnuMakeParser *parser;
};
#endif

} // namespace ProjectExplorer
