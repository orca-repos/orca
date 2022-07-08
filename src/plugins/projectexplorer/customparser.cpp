// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customparser.hpp"

#include "projectexplorerconstants.hpp"
#include "projectexplorer.hpp"
#include "task.hpp"

#include <core/icore.hpp>
#include <utils/qtcassert.hpp>

#include <QCheckBox>
#include <QLabel>
#include <QPair>
#include <QString>
#include <QVBoxLayout>

#ifdef WITH_TESTS
#   include <QTest>

#   include "projectexplorer.hpp"
#   include "outputparser_test.hpp"
#endif

using namespace Utils;

constexpr char idKey[] = "Id";
constexpr char nameKey[] = "Name";
constexpr char errorKey[] = "Error";
constexpr char warningKey[] = "Warning";
constexpr char patternKey[] = "Pattern";
constexpr char lineNumberCapKey[] = "LineNumberCap";
constexpr char fileNameCapKey[] = "FileNameCap";
constexpr char messageCapKey[] = "MessageCap";
constexpr char channelKey[] = "Channel";
constexpr char exampleKey[] = "Example";

namespace ProjectExplorer {

auto CustomParserExpression::operator ==(const CustomParserExpression &other) const -> bool
{
  return pattern() == other.pattern() && fileNameCap() == other.fileNameCap() && lineNumberCap() == other.lineNumberCap() && messageCap() == other.messageCap() && channel() == other.channel() && example() == other.example();
}

auto CustomParserExpression::pattern() const -> QString
{
  return m_regExp.pattern();
}

auto CustomParserExpression::setPattern(const QString &pattern) -> void
{
  m_regExp.setPattern(pattern);
  QTC_CHECK(m_regExp.isValid());
}

auto CustomParserExpression::channel() const -> CustomParserChannel
{
  return m_channel;
}

auto CustomParserExpression::setChannel(CustomParserChannel channel) -> void
{
  if (channel == ParseNoChannel || channel > ParseBothChannels)
    channel = ParseBothChannels;
  m_channel = channel;
}

auto CustomParserExpression::example() const -> QString
{
  return m_example;
}

auto CustomParserExpression::setExample(const QString &example) -> void
{
  m_example = example;
}

auto CustomParserExpression::messageCap() const -> int
{
  return m_messageCap;
}

auto CustomParserExpression::setMessageCap(int messageCap) -> void
{
  m_messageCap = messageCap;
}

auto CustomParserExpression::toMap() const -> QVariantMap
{
  QVariantMap map;
  map.insert(patternKey, pattern());
  map.insert(messageCapKey, messageCap());
  map.insert(fileNameCapKey, fileNameCap());
  map.insert(lineNumberCapKey, lineNumberCap());
  map.insert(exampleKey, example());
  map.insert(channelKey, channel());
  return map;
}

auto CustomParserExpression::fromMap(const QVariantMap &map) -> void
{
  setPattern(map.value(patternKey).toString());
  setMessageCap(map.value(messageCapKey).toInt());
  setFileNameCap(map.value(fileNameCapKey).toInt());
  setLineNumberCap(map.value(lineNumberCapKey).toInt());
  setExample(map.value(exampleKey).toString());
  setChannel(static_cast<CustomParserChannel>(map.value(channelKey).toInt()));
}

auto CustomParserExpression::lineNumberCap() const -> int
{
  return m_lineNumberCap;
}

auto CustomParserExpression::setLineNumberCap(int lineNumberCap) -> void
{
  m_lineNumberCap = lineNumberCap;
}

auto CustomParserExpression::fileNameCap() const -> int
{
  return m_fileNameCap;
}

auto CustomParserExpression::setFileNameCap(int fileNameCap) -> void
{
  m_fileNameCap = fileNameCap;
}

auto CustomParserSettings::operator ==(const CustomParserSettings &other) const -> bool
{
  return id == other.id && displayName == other.displayName && error == other.error && warning == other.warning;
}

auto CustomParserSettings::toMap() const -> QVariantMap
{
  QVariantMap map;
  map.insert(idKey, id.toSetting());
  map.insert(nameKey, displayName);
  map.insert(errorKey, error.toMap());
  map.insert(warningKey, warning.toMap());
  return map;
}

auto CustomParserSettings::fromMap(const QVariantMap &map) -> void
{
  id = Id::fromSetting(map.value(idKey));
  displayName = map.value(nameKey).toString();
  error.fromMap(map.value(errorKey).toMap());
  warning.fromMap(map.value(warningKey).toMap());
}

CustomParsersAspect::CustomParsersAspect(Target *target)
{
  Q_UNUSED(target)
  setId("CustomOutputParsers");
  setSettingsKey("CustomOutputParsers");
  setDisplayName(tr("Custom Output Parsers"));
  setConfigWidgetCreator([this] {
    const auto widget = new Internal::CustomParsersSelectionWidget;
    widget->setSelectedParsers(m_parsers);
    connect(widget, &Internal::CustomParsersSelectionWidget::selectionChanged, this, [this, widget] { m_parsers = widget->selectedParsers(); });
    return widget;
  });
}

auto CustomParsersAspect::fromMap(const QVariantMap &map) -> void
{
  m_parsers = transform(map.value(settingsKey()).toList(), &Id::fromSetting);
}

auto CustomParsersAspect::toMap(QVariantMap &map) const -> void
{
  map.insert(settingsKey(), transform(m_parsers, &Id::toSetting));
}

namespace Internal {

CustomParser::CustomParser(const CustomParserSettings &settings)
{
  setObjectName("CustomParser");

  setSettings(settings);
}

auto CustomParser::setSettings(const CustomParserSettings &settings) -> void
{
  m_error = settings.error;
  m_warning = settings.warning;
}

auto CustomParser::createFromId(Id id) -> CustomParser*
{
  const auto parser = findOrDefault(ProjectExplorerPlugin::customParsers(), [id](const CustomParserSettings &p) { return p.id == id; });
  if (parser.id.isValid())
    return new CustomParser(parser);
  return nullptr;
}

auto CustomParser::id() -> Id
{
  return Id("ProjectExplorer.OutputParser.Custom");
}

auto CustomParser::handleLine(const QString &line, OutputFormat type) -> Result
{
  const auto channel = type == StdErrFormat ? CustomParserExpression::ParseStdErrChannel : CustomParserExpression::ParseStdOutChannel;
  return parseLine(line, channel);
}

auto CustomParser::hasMatch(const QString &line, CustomParserExpression::CustomParserChannel channel, const CustomParserExpression &expression, Task::TaskType taskType) -> Result
{
  if (!(channel & expression.channel()))
    return Status::NotHandled;

  if (expression.pattern().isEmpty())
    return Status::NotHandled;

  const auto match = expression.match(line);
  if (!match.hasMatch())
    return Status::NotHandled;

  const auto fileName = absoluteFilePath(FilePath::fromString(match.captured(expression.fileNameCap())));
  const auto lineNumber = match.captured(expression.lineNumberCap()).toInt();
  const auto message = match.captured(expression.messageCap());
  LinkSpecs linkSpecs;
  addLinkSpecForAbsoluteFilePath(linkSpecs, fileName, lineNumber, match, expression.fileNameCap());
  scheduleTask(CompileTask(taskType, message, fileName, lineNumber), 1);
  return {Status::Done, linkSpecs};
}

auto CustomParser::parseLine(const QString &rawLine, CustomParserExpression::CustomParserChannel channel) -> Result
{
  const auto line = rightTrimmed(rawLine);
  const auto res = hasMatch(line, channel, m_error, Task::Error);
  if (res.status != Status::NotHandled)
    return res;
  return hasMatch(line, channel, m_warning, Task::Warning);
}

namespace {

class SelectionWidget : public QWidget {
  Q_OBJECT public:
  SelectionWidget(QWidget *parent = nullptr) : QWidget(parent)
  {
    const auto layout = new QVBoxLayout(this);
    const auto explanatoryLabel = new QLabel(tr("Custom output parsers scan command line output for user-provided error patterns<br>" "in order to create entries in the issues pane.<br>" "The parsers can be configured <a href=\"dummy\">here</a>."));
    layout->addWidget(explanatoryLabel);
    connect(explanatoryLabel, &QLabel::linkActivated, [] {
      Core::ICore::showOptionsDialog(Constants::CUSTOM_PARSERS_SETTINGS_PAGE_ID);
    });
    updateUi();
    connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::customParsersChanged, this, &SelectionWidget::updateUi);
  }

  auto setSelectedParsers(const QList<Id> &parsers) -> void
  {
    for (const auto &p : qAsConst(parserCheckBoxes))
      p.first->setChecked(parsers.contains(p.second));
    emit selectionChanged();
  }

  auto selectedParsers() const -> QList<Id>
  {
    QList<Id> parsers;
    for (const auto &p : qAsConst(parserCheckBoxes)) {
      if (p.first->isChecked())
        parsers << p.second;
    }
    return parsers;
  }

signals:
  auto selectionChanged() -> void;

private:
  auto updateUi() -> void
  {
    const auto layout = qobject_cast<QVBoxLayout*>(this->layout());
    QTC_ASSERT(layout, return);
    const auto parsers = selectedParsers();
    for (const auto &p : qAsConst(parserCheckBoxes))
      delete p.first;
    parserCheckBoxes.clear();
    for (const auto &s : ProjectExplorerPlugin::customParsers()) {
      const auto checkBox = new QCheckBox(s.displayName, this);
      connect(checkBox, &QCheckBox::stateChanged, this, &SelectionWidget::selectionChanged);
      parserCheckBoxes << qMakePair(checkBox, s.id);
      layout->addWidget(checkBox);
    }
    setSelectedParsers(parsers);
  }

  QList<QPair<QCheckBox*, Id>> parserCheckBoxes;
};

} // anonymous namespace

CustomParsersSelectionWidget::CustomParsersSelectionWidget(QWidget *parent) : DetailsWidget(parent)
{
  const auto widget = new SelectionWidget(this);
  connect(widget, &SelectionWidget::selectionChanged, [this] {
    updateSummary();
    emit selectionChanged();
  });
  setWidget(widget);
  updateSummary();
}

auto CustomParsersSelectionWidget::setSelectedParsers(const QList<Id> &parsers) -> void
{
  qobject_cast<SelectionWidget*>(widget())->setSelectedParsers(parsers);
}

auto CustomParsersSelectionWidget::selectedParsers() const -> QList<Id>
{
  return qobject_cast<SelectionWidget*>(widget())->selectedParsers();
}

auto CustomParsersSelectionWidget::updateSummary() -> void
{
  const auto parsers = qobject_cast<SelectionWidget*>(widget())->selectedParsers();
  if (parsers.isEmpty())
    setSummaryText(tr("There are no custom parsers active"));
  else
    setSummaryText(tr("There are %n custom parsers active", nullptr, parsers.count()));
}

} // namespace Internal

// Unit tests:

#ifdef WITH_TESTS

using namespace Internal;

void ProjectExplorerPlugin::testCustomOutputParsers_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<QString>("workDir");
  QTest::addColumn<OutputParserTester::Channel>("inputChannel");
  QTest::addColumn<CustomParserExpression::CustomParserChannel>("filterErrorChannel");
  QTest::addColumn<CustomParserExpression::CustomParserChannel>("filterWarningChannel");
  QTest::addColumn<QString>("errorPattern");
  QTest::addColumn<int>("errorFileNameCap");
  QTest::addColumn<int>("errorLineNumberCap");
  QTest::addColumn<int>("errorMessageCap");
  QTest::addColumn<QString>("warningPattern");
  QTest::addColumn<int>("warningFileNameCap");
  QTest::addColumn<int>("warningLineNumberCap");
  QTest::addColumn<int>("warningMessageCap");
  QTest::addColumn<QString>("childStdOutLines");
  QTest::addColumn<QString>("childStdErrLines");
  QTest::addColumn<Tasks>("tasks");
  QTest::addColumn<QString>("outputLines");

  const QString simplePattern = "^([a-z]+\\.[a-z]+):(\\d+): error: ([^\\s].+)$";
  const FilePath fileName = FilePath::fromUserInput("main.c");

  QTest::newRow("empty patterns") << QString::fromLatin1("Sometext") << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << QString() << 1 << 2 << 3 << QString() << 1 << 2 << 3 << QString::fromLatin1("Sometext\n") << QString() << Tasks() << QString();

  QTest::newRow("pass-through stdout") << QString::fromLatin1("Sometext") << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << QString() << 1 << 2 << 3 << QString::fromLatin1("Sometext\n") << QString() << Tasks() << QString();

  QTest::newRow("pass-through stderr") << QString::fromLatin1("Sometext") << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << QString() << 1 << 2 << 3 << QString() << QString::fromLatin1("Sometext\n") << Tasks() << QString();

  const QString simpleError = "main.c:9: error: `sfasdf' undeclared (first use this function)";
  const QString simpleErrorPassThrough = simpleError + '\n';
  const QString message = "`sfasdf' undeclared (first use this function)";

  QTest::newRow("simple error") << simpleError << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << QString() << QString() << Tasks({CompileTask(Task::Error, message, fileName, 9)}) << QString();

  const QString pathPattern = "^([a-z\\./]+):(\\d+): error: ([^\\s].+)$";
  QString workingDir = "/home/src/project";
  FilePath expandedFileName = "/home/src/project/main.c";

  QTest::newRow("simple error with expanded path") << "main.c:9: error: `sfasdf' undeclared (first use this function)" << workingDir << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << pathPattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << QString() << QString() << Tasks({CompileTask(Task::Error, message, expandedFileName, 9)}) << QString();

  expandedFileName = "/home/src/project/subdir/main.c";
  QTest::newRow("simple error with subdir path") << "subdir/main.c:9: error: `sfasdf' undeclared (first use this function)" << workingDir << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << pathPattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << QString() << QString() << Tasks({CompileTask(Task::Error, message, expandedFileName, 9)}) << QString();

  workingDir = "/home/src/build-project";
  QTest::newRow("simple error with buildir path") << "../project/subdir/main.c:9: error: `sfasdf' undeclared (first use this function)" << workingDir << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << pathPattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << QString() << QString() << Tasks({CompileTask(Task::Error, message, expandedFileName, 9)}) << QString();

  QTest::newRow("simple error on wrong channel") << simpleError << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseStdErrChannel << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << simpleErrorPassThrough << QString() << Tasks() << QString();

  QTest::newRow("simple error on other wrong channel") << simpleError << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseStdOutChannel << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << QString() << 0 << 0 << 0 << QString() << simpleErrorPassThrough << Tasks() << QString();

  const QString simpleError2 = "Error: Line 19 in main.c: `sfasdf' undeclared (first use this function)";
  const QString simplePattern2 = "^Error: Line (\\d+) in ([a-z]+\\.[a-z]+): ([^\\s].+)$";
  const int lineNumber2 = 19;

  QTest::newRow("another simple error on stderr") << simpleError2 << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern2 << 2 << 1 << 3 << QString() << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, message, fileName, lineNumber2)}) << QString();

  QTest::newRow("another simple error on stdout") << simpleError2 << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern2 << 2 << 1 << 3 << QString() << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, message, fileName, lineNumber2)}) << QString();

  const QString simpleWarningPattern = "^([a-z]+\\.[a-z]+):(\\d+): warning: ([^\\s].+)$";
  const QString simpleWarning = "main.c:1234: warning: `helloWorld' declared but not used";
  const QString warningMessage = "`helloWorld' declared but not used";

  QTest::newRow("simple warning") << simpleWarning << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << QString() << 1 << 2 << 3 << simpleWarningPattern << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Warning, warningMessage, fileName, 1234)}) << QString();

  const QString simpleWarning2 = "Warning: `helloWorld' declared but not used (main.c:19)";
  const QString simpleWarningPassThrough2 = simpleWarning2 + '\n';
  const QString simpleWarningPattern2 = "^Warning: (.*) \\(([a-z]+\\.[a-z]+):(\\d+)\\)$";

  QTest::newRow("another simple warning on stdout") << simpleWarning2 << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseStdOutChannel << simplePattern2 << 1 << 2 << 3 << simpleWarningPattern2 << 2 << 3 << 1 << QString() << QString() << Tasks({CompileTask(Task::Warning, warningMessage, fileName, lineNumber2)}) << QString();

  QTest::newRow("warning on wrong channel") << simpleWarning2 << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseStdErrChannel << QString() << 1 << 2 << 3 << simpleWarningPattern2 << 2 << 3 << 1 << simpleWarningPassThrough2 << QString() << Tasks() << QString();

  QTest::newRow("warning on other wrong channel") << simpleWarning2 << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseStdOutChannel << QString() << 1 << 2 << 3 << simpleWarningPattern2 << 2 << 3 << 1 << QString() << simpleWarningPassThrough2 << Tasks() << QString();

  QTest::newRow("error and *warning*") << simpleWarning << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << simpleWarningPattern << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Warning, warningMessage, fileName, 1234)}) << QString();

  QTest::newRow("*error* when equal pattern") << simpleError << QString() << OutputParserTester::STDERR << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << simplePattern << 1 << 2 << 3 << simplePattern << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, message, fileName, 9)}) << QString();

  const QString unitTestError = "../LedDriver/LedDriverTest.c:63: FAIL: Expected 0x0080 Was 0xffff";
  const FilePath unitTestFileName = FilePath::fromUserInput("../LedDriver/LedDriverTest.c");
  const QString unitTestMessage = "Expected 0x0080 Was 0xffff";
  const QString unitTestPattern = "^([^:]+):(\\d+): FAIL: ([^\\s].+)$";
  const int unitTestLineNumber = 63;

  QTest::newRow("unit test error") << unitTestError << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << unitTestPattern << 1 << 2 << 3 << QString() << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, unitTestMessage, unitTestFileName, unitTestLineNumber)}) << QString();

  const QString leadingSpacesPattern = "^    MESSAGE:(.+)";
  const QString leadingSpacesMessage = "    MESSAGE:Error";
  const QString noLeadingSpacesMessage = "MESSAGE:Error";
  QTest::newRow("leading spaces: match") << leadingSpacesMessage << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << leadingSpacesPattern << 2 << 3 << 1 << QString() << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, "Error", {}, -1)}) << QString();
  QTest::newRow("leading spaces: no match") << noLeadingSpacesMessage << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << leadingSpacesPattern << 2 << 3 << 1 << QString() << 1 << 2 << 3 << (noLeadingSpacesMessage + '\n') << QString() << Tasks() << QString();
  const QString noLeadingSpacesPattern = "^MESSAGE:(.+)";
  QTest::newRow("no leading spaces: match") << noLeadingSpacesMessage << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << noLeadingSpacesPattern << 2 << 3 << 1 << QString() << 1 << 2 << 3 << QString() << QString() << Tasks({CompileTask(Task::Error, "Error", {}, -1)}) << QString();
  QTest::newRow("no leading spaces: no match") << leadingSpacesMessage << QString() << OutputParserTester::STDOUT << CustomParserExpression::ParseBothChannels << CustomParserExpression::ParseBothChannels << noLeadingSpacesPattern << 3 << 2 << 1 << QString() << 1 << 2 << 3 << (leadingSpacesMessage + '\n') << QString() << Tasks() << QString();
}

void ProjectExplorerPlugin::testCustomOutputParsers()
{
  QFETCH(QString, input);
  QFETCH(QString, workDir);
  QFETCH(OutputParserTester::Channel, inputChannel);
  QFETCH(CustomParserExpression::CustomParserChannel, filterErrorChannel);
  QFETCH(CustomParserExpression::CustomParserChannel, filterWarningChannel);
  QFETCH(QString, errorPattern);
  QFETCH(int, errorFileNameCap);
  QFETCH(int, errorLineNumberCap);
  QFETCH(int, errorMessageCap);
  QFETCH(QString, warningPattern);
  QFETCH(int, warningFileNameCap);
  QFETCH(int, warningLineNumberCap);
  QFETCH(int, warningMessageCap);
  QFETCH(QString, childStdOutLines);
  QFETCH(QString, childStdErrLines);
  QFETCH(Tasks, tasks);
  QFETCH(QString, outputLines);

  CustomParserSettings settings;
  settings.error.setPattern(errorPattern);
  settings.error.setFileNameCap(errorFileNameCap);
  settings.error.setLineNumberCap(errorLineNumberCap);
  settings.error.setMessageCap(errorMessageCap);
  settings.error.setChannel(filterErrorChannel);
  settings.warning.setPattern(warningPattern);
  settings.warning.setFileNameCap(warningFileNameCap);
  settings.warning.setLineNumberCap(warningLineNumberCap);
  settings.warning.setMessageCap(warningMessageCap);
  settings.warning.setChannel(filterWarningChannel);

  CustomParser *parser = new CustomParser;
  parser->setSettings(settings);
  parser->addSearchDir(FilePath::fromString(workDir));
  parser->skipFileExistsCheck();

  OutputParserTester testbench;
  testbench.addLineParser(parser);
  testbench.testParsing(input, inputChannel, tasks, childStdOutLines, childStdErrLines, outputLines);
}

#endif

} // namespace ProjectExplorer

#include <customparser.moc>
