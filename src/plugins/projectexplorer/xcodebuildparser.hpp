// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "ioutputparser.hpp"
#include "devicesupport/idevice.hpp"

#include <QRegularExpression>
#include <QStringList>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT XcodebuildParser : public OutputTaskParser {
  Q_OBJECT

public:
  enum XcodebuildStatus {
    InXcodebuild,
    OutsideXcodebuild,
    UnknownXcodebuildState
  };

  XcodebuildParser();

private:
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
  auto hasDetectedRedirection() const -> bool override;
  auto hasFatalErrors() const -> bool override { return m_fatalErrorCount > 0; }

  int m_fatalErrorCount = 0;
  const QRegularExpression m_failureRe;
  const QRegularExpression m_successRe;
  const QRegularExpression m_buildRe;
  XcodebuildStatus m_xcodeBuildParserState = OutsideXcodebuild;

  #if defined WITH_TESTS
    friend class XcodebuildParserTester;
    friend class ProjectExplorerPlugin;
  #endif
};

#if defined WITH_TESTS
class XcodebuildParserTester : public QObject
{
    Q_OBJECT
public:
    explicit XcodebuildParserTester(XcodebuildParser *parser, QObject *parent = nullptr);

    XcodebuildParser *parser;
    XcodebuildParser::XcodebuildStatus expectedFinalState = XcodebuildParser::OutsideXcodebuild;

public slots:
    void onAboutToDeleteParser();
};
#endif

} // namespace ProjectExplorer
