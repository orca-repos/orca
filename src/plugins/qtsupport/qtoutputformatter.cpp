// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtoutputformatter.hpp"

#include "qtkitinformation.hpp"
#include "qtsupportconstants.hpp"
#include "qttestparser.hpp"

#include <core/core-editor-manager.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/target.hpp>

#include <utils/algorithm.hpp>
#include <utils/ansiescapecodehandler.hpp>
#include <utils/fileinprojectfinder.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/outputformatter.hpp>
#include <utils/theme/theme.hpp>

#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QTextCursor>
#include <QUrl>

#include <tuple>

using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {
namespace Internal {

class QtOutputFormatterPrivate {
public:
  QtOutputFormatterPrivate() : qmlError("(" QT_QML_URL_REGEXP ":\\d+" "(?::\\d+)?)" "\\b"), qtError("Object::.*in (.*:\\d+)"), qtAssert(QT_ASSERT_REGEXP), qtAssertX(QT_ASSERT_X_REGEXP), qtTestFailUnix(QT_TEST_FAIL_UNIX_REGEXP), qtTestFailWin(QT_TEST_FAIL_WIN_REGEXP) { }

  const QRegularExpression qmlError;
  const QRegularExpression qtError;
  const QRegularExpression qtAssert;
  const QRegularExpression qtAssertX;
  const QRegularExpression qtTestFailUnix;
  const QRegularExpression qtTestFailWin;
  QPointer<Project> project;
  FileInProjectFinder projectFinder;
};

class QtOutputLineParser : public OutputLineParser {
public:
  explicit QtOutputLineParser(Target *target);
  ~QtOutputLineParser() override;

protected:
  virtual auto openEditor(const QString &fileName, int line, int column = -1) -> void;

private:
  auto handleLine(const QString &text, OutputFormat format) -> Result override;
  auto handleLink(const QString &href) -> bool override;
  auto updateProjectFileList() -> void;
  auto matchLine(const QString &line) const -> LinkSpec;

  QtOutputFormatterPrivate *d;
  friend class QtSupportPlugin; // for testing
};

QtOutputLineParser::QtOutputLineParser(Target *target) : d(new QtOutputFormatterPrivate)
{
  d->project = target ? target->project() : nullptr;
  if (d->project) {
    d->projectFinder.setProjectFiles(d->project->files(Project::SourceFiles));
    d->projectFinder.setProjectDirectory(d->project->projectDirectory());

    connect(d->project, &Project::fileListChanged, this, &QtOutputLineParser::updateProjectFileList, Qt::QueuedConnection);
  }
}

QtOutputLineParser::~QtOutputLineParser()
{
  delete d;
}

auto QtOutputLineParser::matchLine(const QString &line) const -> LinkSpec
{
  LinkSpec lr;

  auto hasMatch = [&lr, line](const QRegularExpression &regex) {
    const auto match = regex.match(line);
    if (!match.hasMatch())
      return false;

    lr.target = match.captured(1);
    lr.startPos = match.capturedStart(1);
    lr.length = lr.target.length();
    return true;
  };

  if (hasMatch(d->qmlError))
    return lr;
  if (hasMatch(d->qtError))
    return lr;
  if (hasMatch(d->qtAssert))
    return lr;
  if (hasMatch(d->qtAssertX))
    return lr;
  if (hasMatch(d->qtTestFailUnix))
    return lr;
  if (hasMatch(d->qtTestFailWin))
    return lr;

  return lr;
}

auto QtOutputLineParser::handleLine(const QString &txt, OutputFormat format) -> Result
{
  Q_UNUSED(format);
  const auto lr = matchLine(txt);
  if (!lr.target.isEmpty())
    return Result(Status::Done, {lr});
  return Status::NotHandled;
}

auto QtOutputLineParser::handleLink(const QString &href) -> bool
{
  QTC_ASSERT(!href.isEmpty(), return false);
  static const QRegularExpression qmlLineColumnLink("^(" QT_QML_URL_REGEXP ")" ":(\\d+)" ":(\\d+)$");
  const auto qmlLineColumnMatch = qmlLineColumnLink.match(href);

  const auto getFileToOpen = [this](const QUrl &fileUrl) {
    return chooseFileFromList(d->projectFinder.findFile(fileUrl)).toString();
  };
  if (qmlLineColumnMatch.hasMatch()) {
    const auto fileUrl = QUrl(qmlLineColumnMatch.captured(1));
    const auto line = qmlLineColumnMatch.captured(2).toInt();
    const auto column = qmlLineColumnMatch.captured(3).toInt();
    openEditor(getFileToOpen(fileUrl), line, column - 1);
    return true;
  }

  static const QRegularExpression qmlLineLink("^(" QT_QML_URL_REGEXP ")" ":(\\d+)$");
  const auto qmlLineMatch = qmlLineLink.match(href);

  if (qmlLineMatch.hasMatch()) {
    const char scheme[] = "file://";
    const auto filePath = qmlLineMatch.captured(1);
    auto fileUrl = QUrl(filePath);
    if (!fileUrl.isValid() && filePath.startsWith(scheme))
      fileUrl = QUrl::fromLocalFile(filePath.mid(int(strlen(scheme))));
    const auto line = qmlLineMatch.captured(2).toInt();
    openEditor(getFileToOpen(fileUrl), line);
    return true;
  }

  QString fileName;
  auto line = -1;

  static const QRegularExpression qtErrorLink("^(.*):(\\d+)$");
  const auto qtErrorMatch = qtErrorLink.match(href);
  if (qtErrorMatch.hasMatch()) {
    fileName = qtErrorMatch.captured(1);
    line = qtErrorMatch.captured(2).toInt();
  }

  static const QRegularExpression qtAssertLink("^(.+), line (\\d+)$");
  const auto qtAssertMatch = qtAssertLink.match(href);
  if (qtAssertMatch.hasMatch()) {
    fileName = qtAssertMatch.captured(1);
    line = qtAssertMatch.captured(2).toInt();
  }

  static const QRegularExpression qtTestFailLink("^(.*)\\((\\d+)\\)$");
  const auto qtTestFailMatch = qtTestFailLink.match(href);
  if (qtTestFailMatch.hasMatch()) {
    fileName = qtTestFailMatch.captured(1);
    line = qtTestFailMatch.captured(2).toInt();
  }

  if (!fileName.isEmpty()) {
    fileName = getFileToOpen(QUrl::fromLocalFile(fileName));
    openEditor(fileName, line);
    return true;
  }
  return false;
}

auto QtOutputLineParser::openEditor(const QString &fileName, int line, int column) -> void
{
  Orca::Plugin::Core::EditorManager::openEditorAt({FilePath::fromString(fileName), line, column});
}

auto QtOutputLineParser::updateProjectFileList() -> void
{
  if (d->project)
    d->projectFinder.setProjectFiles(d->project->files(Project::SourceFiles));
}

// QtOutputFormatterFactory

QtOutputFormatterFactory::QtOutputFormatterFactory()
{
  setFormatterCreator([](Target *t) -> QList<OutputLineParser*> {
    if (QtKitAspect::qtVersion(t ? t->kit() : nullptr))
      return {new QtTestParser, new QtOutputLineParser(t)};
    return {};
  });
}

} // namespace Internal
} // namespace QtSupport

// Unit tests:

#ifdef WITH_TESTS

#   include <QTest>

#   include "qtsupportplugin.hpp"

Q_DECLARE_METATYPE(QTextCharFormat)

namespace QtSupport {

using namespace QtSupport::Internal;

class TestQtOutputLineParser : public QtOutputLineParser
{
public:
    TestQtOutputLineParser() :
        QtOutputLineParser(nullptr)
    {
    }

    void openEditor(const QString &fileName, int line, int column = -1)
    {
        this->fileName = fileName;
        this->line = line;
        this->column = column;
    }

public:
    QString fileName;
    int line = -1;
    int column = -1;
};

class TestQtOutputFormatter : public OutputFormatter
{
public:
    TestQtOutputFormatter() { setLineParsers({new TestQtOutputLineParser}); }
};

void QtSupportPlugin::testQtOutputFormatter_data()
{
    QTest::addColumn<QString>("input");

    // matchLine results
    QTest::addColumn<int>("linkStart");
    QTest::addColumn<int>("linkEnd");
    QTest::addColumn<QString>("href");

    // handleLink results
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("column");

    QTest::newRow("pass through")
            << "Pass through plain text."
            << -1 << -2 << QString()
            << QString() << -1 << -1;

    QTest::newRow("qrc:/main.qml:20")
            << "qrc:/main.qml:20 Unexpected token `identifier'"
            << 0 << 16 << "qrc:/main.qml:20"
            << "/main.qml" << 20 << -1;

    QTest::newRow("qrc:///main.qml:20")
            << "qrc:///main.qml:20 Unexpected token `identifier'"
            << 0 << 18 << "qrc:///main.qml:20"
            << "/main.qml" << 20 << -1;

    QTest::newRow("onClicked (qrc:/main.qml:20)")
            << "onClicked (qrc:/main.qml:20)"
            << 11 << 27 << "qrc:/main.qml:20"
            << "/main.qml" << 20 << -1;

    QTest::newRow("file:///main.qml:20")
            << "file:///main.qml:20 Unexpected token `identifier'"
            << 0 << 19 << "file:///main.qml:20"
            << "/main.qml" << 20 << -1;

    QTest::newRow("File link without further text")
            << "file:///home/user/main.cpp:157"
            << 0 << 30 << "file:///home/user/main.cpp:157"
            << "/home/user/main.cpp" << 157 << -1;

    QTest::newRow("File link with text before")
            << "Text before: file:///home/user/main.cpp:157"
            << 13 << 43 << "file:///home/user/main.cpp:157"
            << "/home/user/main.cpp" << 157 << -1;

    QTest::newRow("File link with text afterwards")
            << "file:///home/user/main.cpp:157: Text afterwards"
            << 0 << 30 << "file:///home/user/main.cpp:157"
            << "/home/user/main.cpp" << 157 << -1;

    QTest::newRow("File link with text before and afterwards")
            << "Text before file:///home/user/main.cpp:157 and text afterwards"
            << 12 << 42 << "file:///home/user/main.cpp:157"
            << "/home/user/main.cpp" << 157 << -1;

    QTest::newRow("Unix file link with timestamp")
            << "file:///home/user/main.cpp:157 2018-03-21 10:54:45.706"
            << 0 << 30 << "file:///home/user/main.cpp:157"
            << "/home/user/main.cpp" << 157 << -1;

    QTest::newRow("Windows file link with timestamp")
            << "file:///e:/path/main.cpp:157 2018-03-21 10:54:45.706"
            << 0 << 28 << "file:///e:/path/main.cpp:157"
            << (Utils::HostOsInfo::isWindowsHost()
                ? "e:/path/main.cpp"
                : "/e:/path/main.cpp")
            << 157 << -1;

    QTest::newRow("Unix failed QTest link")
            << "   Loc: [../TestProject/test.cpp(123)]"
            << 9 << 37 << "../TestProject/test.cpp(123)"
            << "../TestProject/test.cpp" << 123 << -1;

    QTest::newRow("Unix failed QTest link (alternate)")
            << "   Loc: [/Projects/TestProject/test.cpp:123]"
            << 9 << 43 << "/Projects/TestProject/test.cpp:123"
            << "/Projects/TestProject/test.cpp" << 123 << -1;

    QTest::newRow("Unix relative file link")
            << "file://../main.cpp:157"
            << 0 << 22 << "file://../main.cpp:157"
            << "../main.cpp" << 157 << -1;

    if (HostOsInfo::isWindowsHost()) {
        QTest::newRow("Windows failed QTest link")
                << "..\\TestProject\\test.cpp(123) : failure location"
                << 0 << 28 << "..\\TestProject\\test.cpp(123)"
                << "../TestProject/test.cpp" << 123 << -1;

        QTest::newRow("Windows failed QTest link (alternate)")
                << "   Loc: [c:\\Projects\\TestProject\\test.cpp:123]"
                << 9 << 45 << "c:\\Projects\\TestProject\\test.cpp:123"
                << "c:/Projects/TestProject/test.cpp" << 123 << -1;

        QTest::newRow("Windows failed QTest link with carriage return")
                << "..\\TestProject\\test.cpp(123) : failure location\r"
                << 0 << 28 << "..\\TestProject\\test.cpp(123)"
                << "../TestProject/test.cpp" << 123 << -1;

        QTest::newRow("Windows relative file link with native separator")
                << "file://..\\main.cpp:157"
                << 0 << 22 << "file://..\\main.cpp:157"
                << "../main.cpp" << 157 << -1;
    }
}

void QtSupportPlugin::testQtOutputFormatter()
{
    QFETCH(QString, input);

    QFETCH(int, linkStart);
    QFETCH(int, linkEnd);
    QFETCH(QString, href);

    QFETCH(QString, file);
    QFETCH(int, line);
    QFETCH(int, column);

    TestQtOutputLineParser formatter;

    QtOutputLineParser::LinkSpec result = formatter.matchLine(input);
    formatter.handleLink(result.target);

    QCOMPARE(result.startPos, linkStart);
    QCOMPARE(result.startPos + result.length, linkEnd);
    QCOMPARE(result.target, href);

    QCOMPARE(formatter.fileName, file);
    QCOMPARE(formatter.line, line);
    QCOMPARE(formatter.column, column);
}

static QTextCharFormat blueFormat()
{
    QTextCharFormat result;
    result.setForeground(QColor(0, 0, 127));
    return result;
}

static QTextCharFormat greenFormat()
{
    QTextCharFormat result;
    result.setForeground(QColor(0, 127, 0));
    return result;
}

void QtSupportPlugin::testQtOutputFormatter_appendMessage_data()
{
    QTest::addColumn<QString>("inputText");
    QTest::addColumn<QString>("outputText");
    QTest::addColumn<QTextCharFormat>("inputFormat");
    QTest::addColumn<QTextCharFormat>("outputFormat");

    QTest::newRow("pass through")
            << "test\n123"
            << "test\n123"
            << QTextCharFormat()
            << QTextCharFormat();
    QTest::newRow("Qt error")
            << "Object::Test in test.cpp:123"
            << "Object::Test in test.cpp:123"
            << QTextCharFormat()
            << OutputFormatter::linkFormat(QTextCharFormat(), "test.cpp:123");
    QTest::newRow("colored")
            << "blue da ba dee"
            << "blue da ba dee"
            << blueFormat()
            << blueFormat();
    QTest::newRow("ANSI color change")
            << "\x1b[38;2;0;0;127mHello"
            << "Hello"
            << QTextCharFormat()
            << blueFormat();
}

void QtSupportPlugin::testQtOutputFormatter_appendMessage()
{
    QPlainTextEdit edit;
    TestQtOutputFormatter formatter;
    formatter.setPlainTextEdit(&edit);

    QFETCH(QString, inputText);
    QFETCH(QString, outputText);
    QFETCH(QTextCharFormat, inputFormat);
    QFETCH(QTextCharFormat, outputFormat);
    if (outputFormat == QTextCharFormat())
        outputFormat = formatter.charFormat(StdOutFormat);
    if (inputFormat != QTextCharFormat())
        formatter.overrideTextCharFormat(inputFormat);

    formatter.appendMessage(inputText, StdOutFormat);
    formatter.flush();

    QCOMPARE(edit.toPlainText(), outputText);
    QCOMPARE(edit.currentCharFormat(), outputFormat);
}

void QtSupportPlugin::testQtOutputFormatter_appendMixedAssertAndAnsi()
{
    QPlainTextEdit edit;

    TestQtOutputFormatter formatter;
    formatter.setPlainTextEdit(&edit);

    const QString inputText =
                "\x1b[38;2;0;127;0mGreen "
                "file://test.cpp:123 "
                "\x1b[38;2;0;0;127mBlue\n";
    const QString outputText =
                "Green "
                "file://test.cpp:123 "
                "Blue\n";

    formatter.appendMessage(inputText, StdOutFormat);
    formatter.flush();

    QCOMPARE(edit.toPlainText(), outputText);

    edit.moveCursor(QTextCursor::Start);
    QCOMPARE(edit.currentCharFormat(), greenFormat());

    edit.moveCursor(QTextCursor::WordRight);
    edit.moveCursor(QTextCursor::Right);
    QCOMPARE(edit.currentCharFormat(),
             OutputFormatter::linkFormat(QTextCharFormat(), "file://test.cpp:123"));

    edit.moveCursor(QTextCursor::End);
    QCOMPARE(edit.currentCharFormat(), blueFormat());
}

} // namespace QtSupport

#endif // WITH_TESTS
