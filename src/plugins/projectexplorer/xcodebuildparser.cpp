// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "xcodebuildparser.hpp"

#include "projectexplorerconstants.hpp"
#include "task.hpp"

#include <utils/qtcassert.hpp>

#include <QCoreApplication>

using namespace Utils;

namespace ProjectExplorer {

static constexpr char failureRe[] = "\\*\\* BUILD FAILED \\*\\*$";
static constexpr char successRe[] = "\\*\\* BUILD SUCCEEDED \\*\\*$";
static constexpr char buildRe[] = "=== BUILD (AGGREGATE )?TARGET (.*) OF PROJECT (.*) WITH .* ===$";
static constexpr char signatureChangeEndsWithPattern[] = ": replacing existing signature";

XcodebuildParser::XcodebuildParser() : m_failureRe(QLatin1String(failureRe)), m_successRe(QLatin1String(successRe)), m_buildRe(QLatin1String(buildRe))
{
  setObjectName(QLatin1String("XcodeParser"));
  QTC_CHECK(m_failureRe.isValid());
  QTC_CHECK(m_successRe.isValid());
  QTC_CHECK(m_buildRe.isValid());
}

auto XcodebuildParser::handleLine(const QString &line, OutputFormat type) -> Result
{
  static const QStringList notesPatterns({"note: Build preparation complete", "note: Building targets in parallel", "note: Planning build"});
  const auto lne = rightTrimmed(line);
  if (type == StdOutFormat) {
    auto match = m_buildRe.match(line);
    if (match.hasMatch() || notesPatterns.contains(lne)) {
      m_xcodeBuildParserState = InXcodebuild;
      return Status::Done;
    }
    if (m_xcodeBuildParserState == InXcodebuild || m_xcodeBuildParserState == UnknownXcodebuildState) {
      match = m_successRe.match(lne);
      if (match.hasMatch()) {
        m_xcodeBuildParserState = OutsideXcodebuild;
        return Status::Done;
      }
      if (lne.endsWith(QLatin1String(signatureChangeEndsWithPattern))) {
        const int filePathEndPos = lne.size() - QLatin1String(signatureChangeEndsWithPattern).size();
        const CompileTask task(Task::Warning, tr("Replacing signature"), absoluteFilePath(FilePath::fromString(lne.left(filePathEndPos))));
        LinkSpecs linkSpecs;
        addLinkSpecForAbsoluteFilePath(linkSpecs, task.file, task.line, 0, filePathEndPos);
        scheduleTask(task, 1);
        return {Status::Done, linkSpecs};
      }
    }
    return Status::NotHandled;
  }
  const auto match = m_failureRe.match(lne);
  if (match.hasMatch()) {
    ++m_fatalErrorCount;
    m_xcodeBuildParserState = UnknownXcodebuildState;
    scheduleTask(CompileTask(Task::Error, tr("Xcodebuild failed.")), 1);
  }
  if (m_xcodeBuildParserState == OutsideXcodebuild)
    return Status::NotHandled;
  return Status::Done;
}

auto XcodebuildParser::hasDetectedRedirection() const -> bool
{
  return m_xcodeBuildParserState != OutsideXcodebuild;
}

} // namespace ProjectExplorer

// Unit tests:

#ifdef WITH_TESTS
#include <QTest>

#include "outputparser_test.hpp"
#include "projectexplorer.hpp"

using namespace ProjectExplorer;

Q_DECLARE_METATYPE(ProjectExplorer::XcodebuildParser::XcodebuildStatus)

XcodebuildParserTester::XcodebuildParserTester(XcodebuildParser *p, QObject *parent) : QObject(parent), parser(p) { }

void XcodebuildParserTester::onAboutToDeleteParser()
{
  QCOMPARE(parser->m_xcodeBuildParserState, expectedFinalState);
}

void ProjectExplorerPlugin::testXcodebuildParserParsing_data()
{
  QTest::addColumn<ProjectExplorer::XcodebuildParser::XcodebuildStatus>("initialStatus");
  QTest::addColumn<QString>("input");
  QTest::addColumn<OutputParserTester::Channel>("inputChannel");
  QTest::addColumn<QString>("childStdOutLines");
  QTest::addColumn<QString>("childStdErrLines");
  QTest::addColumn<Tasks>("tasks");
  QTest::addColumn<QString>("outputLines");
  QTest::addColumn<ProjectExplorer::XcodebuildParser::XcodebuildStatus>("finalStatus");

  QTest::newRow("outside pass-through stdout") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("Sometext") << OutputParserTester::STDOUT << QString::fromLatin1("Sometext\n") << QString() << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;
  QTest::newRow("outside pass-through stderr") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("Sometext") << OutputParserTester::STDERR << QString() << QString::fromLatin1("Sometext\n") << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;
  QTest::newRow("inside pass stdout to stderr") << XcodebuildParser::InXcodebuild << QString::fromLatin1("Sometext") << OutputParserTester::STDOUT << QString() << QString::fromLatin1("Sometext\n") << Tasks() << QString() << XcodebuildParser::InXcodebuild;
  QTest::newRow("inside ignore stderr") << XcodebuildParser::InXcodebuild << QString::fromLatin1("Sometext") << OutputParserTester::STDERR << QString() << QString() << Tasks() << QString() << XcodebuildParser::InXcodebuild;
  QTest::newRow("unknown pass stdout to stderr") << XcodebuildParser::UnknownXcodebuildState << QString::fromLatin1("Sometext") << OutputParserTester::STDOUT << QString() << QString::fromLatin1("Sometext\n") << Tasks() << QString() << XcodebuildParser::UnknownXcodebuildState;
  QTest::newRow("unknown ignore stderr (change?)") << XcodebuildParser::UnknownXcodebuildState << QString::fromLatin1("Sometext") << OutputParserTester::STDERR << QString() << QString() << Tasks() << QString() << XcodebuildParser::UnknownXcodebuildState;
  QTest::newRow("switch outside->in->outside") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("outside\n" "=== BUILD AGGREGATE TARGET Qt Preprocess OF PROJECT testQQ WITH THE DEFAULT CONFIGURATION (Debug) ===\n" "in xcodebuild\n" "=== BUILD TARGET testQQ OF PROJECT testQQ WITH THE DEFAULT CONFIGURATION (Debug) ===\n" "in xcodebuild2\n" "** BUILD SUCCEEDED **\n" "outside2") << OutputParserTester::STDOUT << QString::fromLatin1("outside\noutside2\n") << QString::fromLatin1("in xcodebuild\nin xcodebuild2\n") << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;
  QTest::newRow("switch outside->in->outside (new)") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("outside\n" "note: Build preparation complete\n" "in xcodebuild\n" "in xcodebuild2\n" "** BUILD SUCCEEDED **\n" "outside2") << OutputParserTester::STDOUT << QString::fromLatin1("outside\noutside2\n") << QString::fromLatin1("in xcodebuild\nin xcodebuild2\n") << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;
  QTest::newRow("switch Unknown->in->outside") << XcodebuildParser::UnknownXcodebuildState << QString::fromLatin1("unknown\n" "=== BUILD TARGET testQQ OF PROJECT testQQ WITH THE DEFAULT CONFIGURATION (Debug) ===\n" "in xcodebuild\n" "** BUILD SUCCEEDED **\n" "outside") << OutputParserTester::STDOUT << QString::fromLatin1("outside\n") << QString::fromLatin1("unknown\nin xcodebuild\n") << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;

  QTest::newRow("switch in->unknown") << XcodebuildParser::InXcodebuild << QString::fromLatin1("insideErr\n" "** BUILD FAILED **\n" "unknownErr") << OutputParserTester::STDERR << QString() << QString() << (Tasks() << CompileTask(Task::Error, XcodebuildParser::tr("Xcodebuild failed."))) << QString() << XcodebuildParser::UnknownXcodebuildState;

  QTest::newRow("switch out->unknown") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("outErr\n" "** BUILD FAILED **\n" "unknownErr") << OutputParserTester::STDERR << QString() << QString::fromLatin1("outErr\n") << (Tasks() << CompileTask(Task::Error, XcodebuildParser::tr("Xcodebuild failed."))) << QString() << XcodebuildParser::UnknownXcodebuildState;

  QTest::newRow("inside catch codesign replace signature") << XcodebuildParser::InXcodebuild << QString::fromLatin1("/somepath/somefile.app: replacing existing signature") << OutputParserTester::STDOUT << QString() << QString() << (Tasks() << CompileTask(Task::Warning, XcodebuildParser::tr("Replacing signature"), "/somepath/somefile.app")) << QString() << XcodebuildParser::InXcodebuild;

  QTest::newRow("outside forward codesign replace signature") << XcodebuildParser::OutsideXcodebuild << QString::fromLatin1("/somepath/somefile.app: replacing existing signature") << OutputParserTester::STDOUT << QString::fromLatin1("/somepath/somefile.app: replacing existing signature\n") << QString() << Tasks() << QString() << XcodebuildParser::OutsideXcodebuild;
}

void ProjectExplorerPlugin::testXcodebuildParserParsing()
{
  OutputParserTester testbench;
  auto *childParser = new XcodebuildParser;
  auto *tester = new XcodebuildParserTester(childParser);

  connect(&testbench, &OutputParserTester::aboutToDeleteParser, tester, &XcodebuildParserTester::onAboutToDeleteParser);

  testbench.addLineParser(childParser);
  QFETCH(ProjectExplorer::XcodebuildParser::XcodebuildStatus, initialStatus);
  QFETCH(QString, input);
  QFETCH(OutputParserTester::Channel, inputChannel);
  QFETCH(QString, childStdOutLines);
  QFETCH(QString, childStdErrLines);
  QFETCH(Tasks, tasks);
  QFETCH(QString, outputLines);
  QFETCH(ProjectExplorer::XcodebuildParser::XcodebuildStatus, finalStatus);

  tester->expectedFinalState = finalStatus;
  childParser->m_xcodeBuildParserState = initialStatus;
  testbench.testParsing(input, inputChannel, tasks, childStdOutLines, childStdErrLines, outputLines);

  delete tester;
}

#endif

