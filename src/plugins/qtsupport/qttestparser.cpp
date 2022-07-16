// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qttestparser.hpp"

#include "qtoutputformatter.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#ifdef WITH_TESTS
#include "qtsupportplugin.hpp"
#include <projectexplorer/outputparser_test.hpp>
#include <QTest>
#endif // WITH_TESTS

using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {
namespace Internal {

auto QtTestParser::handleLine(const QString &line, OutputFormat type) -> Result
{
  if (type != StdOutFormat && type != DebugFormat)
    return Status::NotHandled;

  const auto theLine = rightTrimmed(line);
  static const QRegularExpression triggerPattern("^(?:XPASS|FAIL!)  : .+$");
  QTC_CHECK(triggerPattern.isValid());
  if (triggerPattern.match(theLine).hasMatch()) {
    emitCurrentTask();
    m_currentTask = Task(Task::Error, theLine, FilePath(), -1, Constants::TASK_CATEGORY_AUTOTEST);
    return {Status::InProgress, {}, {}, StdErrFormat};
  }
  if (m_currentTask.isNull())
    return Status::NotHandled;
  static const QRegularExpression locationPattern(HostOsInfo::isWindowsHost() ? QString(QT_TEST_FAIL_WIN_REGEXP) : QString(QT_TEST_FAIL_UNIX_REGEXP));
  QTC_CHECK(locationPattern.isValid());
  const auto match = locationPattern.match(theLine);
  if (match.hasMatch()) {
    LinkSpecs linkSpecs;
    m_currentTask.file = absoluteFilePath(FilePath::fromString(QDir::fromNativeSeparators(match.captured("file"))));
    m_currentTask.line = match.captured("line").toInt();
    addLinkSpecForAbsoluteFilePath(linkSpecs, m_currentTask.file, m_currentTask.line, match, "file");
    emitCurrentTask();
    return {Status::Done, linkSpecs};
  }
  if (line.startsWith("   Actual") || line.startsWith("   Expected")) {
    m_currentTask.details.append(theLine);
    return Status::InProgress;
  }
  return Status::NotHandled;
}

auto QtTestParser::emitCurrentTask() -> void
{
  if (!m_currentTask.isNull()) {
    scheduleTask(m_currentTask, 1);
    m_currentTask.clear();
  }
}

#ifdef WITH_TESTS
void QtSupportPlugin::testQtTestOutputParser()
{
    OutputParserTester testbench;
    testbench.addLineParser(new QtTestParser);
    const QString input = "random output\n"
            "PASS   : MyTest::someTest()\n"
            "XPASS  : MyTest::someTest()\n"
#ifdef Q_OS_WIN
            "C:\\dev\\tests\\tst_mytest.cpp(154) : failure location\n"
#else
            "   Loc: [/home/me/tests/tst_mytest.cpp(154)]\n"
#endif
            "FAIL!  : MyTest::someOtherTest(init) Compared values are not the same\n"
            "   Actual   (exceptionCaught): 0\n"
            "   Expected (true)           : 1\n"
#ifdef Q_OS_WIN
            "C:\\dev\\tests\\tst_mytest.cpp(220) : failure location\n"
#else
            "   Loc: [/home/me/tests/tst_mytest.cpp(220)]\n"
#endif
            "XPASS: irrelevant\n"
            "PASS   : MyTest::anotherTest()";
    const QString expectedChildOutput =
            "random output\n"
            "PASS   : MyTest::someTest()\n"
            "XPASS: irrelevant\n"
            "PASS   : MyTest::anotherTest()\n";
    const FilePath theFile = FilePath::fromString(HostOsInfo::isWindowsHost()
        ? QString("C:/dev/tests/tst_mytest.cpp") : QString("/home/me/tests/tst_mytest.cpp"));
    const Tasks expectedTasks{
        Task(Task::Error, "XPASS  : MyTest::someTest()", theFile, 154,
             Constants::TASK_CATEGORY_AUTOTEST),
        Task(Task::Error, "FAIL!  : MyTest::someOtherTest(init) "
                          "Compared values are not the same\n"
                          "   Actual   (exceptionCaught): 0\n"
                          "   Expected (true)           : 1",
             theFile, 220, Constants::TASK_CATEGORY_AUTOTEST)};
    testbench.testParsing(input, OutputParserTester::STDOUT, expectedTasks, expectedChildOutput,
                          QString(), QString());
}
#endif // WITH_TESTS

} // namespace Internal
} // namespace QtSupport
