// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-spotlight-locator-filter.hpp"

#include "core-editor-manager.hpp"
#include "core-message-manager.hpp"

#include <utils/algorithm.hpp>
#include <utils/commandline.hpp>
#include <utils/environment.hpp>
#include <utils/fancylineedit.hpp>
#include <utils/link.hpp>
#include <utils/macroexpander.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/stringutils.hpp>
#include <utils/variablechooser.hpp>

#include <QFormLayout>
#include <QJsonObject>
#include <QMutex>
#include <QWaitCondition>

using namespace Utils;

namespace Orca::Plugin::Core {

class SpotlightIterator final : public BaseFileFilter::Iterator {
public:
  explicit SpotlightIterator(const QStringList &command);
  ~SpotlightIterator() override;

  auto toFront() -> void override;
  auto hasNext() const -> bool override;
  auto next() -> FilePath override;
  auto filePath() const -> FilePath override;
  auto scheduleKillProcess() -> void;
  auto killProcess() -> void;

private:
  auto ensureNext() -> void;

  std::unique_ptr<QtcProcess> m_process;
  QMutex m_mutex;
  QWaitCondition m_wait_for_items;
  QList<FilePath> m_queue;
  QList<FilePath> m_file_paths;
  int m_index;
  bool m_finished;
};

SpotlightIterator::SpotlightIterator(const QStringList &command) : m_index(-1), m_finished(false)
{
  QTC_ASSERT(!command.isEmpty(), return);
  m_process.reset(new QtcProcess);
  m_process->setCommand({Environment::systemEnvironment().searchInPath(command.first()), command.mid(1)});
  m_process->setEnvironment(Environment::systemEnvironment());

  QObject::connect(m_process.get(), &QtcProcess::finished, [this] { scheduleKillProcess(); });
  QObject::connect(m_process.get(), &QtcProcess::errorOccurred, [this, command] {
    MessageManager::writeFlashing(SpotlightLocatorFilter::tr("Locator: Error occurred when running \"%1\".").arg(command.first()));
    scheduleKillProcess();
  });

  QObject::connect(m_process.get(), &QtcProcess::readyReadStandardOutput, [this] {
    auto output = QString::fromUtf8(m_process->readAllStandardOutput());
    output.replace("\r\n", "\n");
    const auto items = output.split('\n');
    QMutexLocker lock(&m_mutex);
    m_queue.append(transform(items, &FilePath::fromUserInput));
    if (m_file_paths.size() + m_queue.size() > 10000) // limit the amount of data
      scheduleKillProcess();
    m_wait_for_items.wakeAll();
  });

  m_process->start();
}

SpotlightIterator::~SpotlightIterator()
{
  killProcess();
}

auto SpotlightIterator::toFront() -> void
{
  m_index = -1;
}

auto SpotlightIterator::hasNext() const -> bool
{
  const auto that = const_cast<SpotlightIterator*>(this);
  that->ensureNext();
  return m_index + 1 < static_cast<int>(m_file_paths.size());
}

auto SpotlightIterator::next() -> FilePath
{
  ensureNext();
  ++m_index;
  QTC_ASSERT(m_index < m_file_paths.size(), return FilePath());
  return m_file_paths.at(m_index);
}

auto SpotlightIterator::filePath() const -> FilePath
{
  QTC_ASSERT(m_index < m_file_paths.size(), return FilePath());
  return m_file_paths.at(m_index);
}

auto SpotlightIterator::scheduleKillProcess() -> void
{
  QMetaObject::invokeMethod(m_process.get(), [this] { killProcess(); }, Qt::QueuedConnection);
}

auto SpotlightIterator::killProcess() -> void
{
  if (!m_process)
    return;

  m_process->disconnect();
  QMutexLocker lock(&m_mutex);
  m_finished = true;
  m_wait_for_items.wakeAll();
  m_process.reset();
}

auto SpotlightIterator::ensureNext() -> void
{
  if (m_index + 1 < static_cast<int>(m_file_paths.size())) // nothing to do
    return;

  // check if there are items in the queue, otherwise wait for some
  QMutexLocker lock(&m_mutex);
  if (m_queue.isEmpty() && !m_finished)
    m_wait_for_items.wait(&m_mutex);

  m_file_paths.append(m_queue);
  m_queue.clear();
}

// #pragma mark -- SpotlightLocatorFilter

static auto defaultCommand() -> QString
{
  if constexpr (HostOsInfo::isMacHost())
    return "mdfind";

  if constexpr (HostOsInfo::isWindowsHost())
    return "es.exe";

  return "locate";
}

/*
    For the tools es [1] and locate [2], interpret space as AND operator.

    Currently doesn't support fine picking a file with a space in the path by escaped space.

    [1]: https://www.voidtools.com/support/everything/command_line_interface/
    [2]: https://www.gnu.org/software/findutils/manual/html_node/find_html/Invoking-locate.html
 */

static auto defaultArguments(const Qt::CaseSensitivity sens = Qt::CaseInsensitive) -> QString
{
  if constexpr (HostOsInfo::isMacHost())
    return QString("\"kMDItemFSName = '*%{Query:EscapedWithWildcards}*'%1\"").arg(sens == Qt::CaseInsensitive ? QString("c") : "");
  else if constexpr (HostOsInfo::isWindowsHost())
    return QString("%1 -n 10000 %{Query:Escaped}").arg(sens == Qt::CaseInsensitive ? QString() : "-i ");
  else
    return QString("%1 -A -l 10000 %{Query:Escaped}").arg(sens == Qt::CaseInsensitive ? QString() : "-i ");
}

constexpr char k_command_key[] = "command";
constexpr char k_arguments_key[] = "arguments";
constexpr char k_case_sensitive_key[] = "caseSensitive";

static auto escaped(const QString &query) -> QString
{
  auto quoted = query;
  quoted.replace('\\', "\\\\").replace('\'', "\\\'").replace('\"', "\\\"");
  return quoted;
}

static auto createMacroExpander(const QString &query) -> MacroExpander*
{
  const auto expander = new MacroExpander;
  expander->registerVariable("Query", SpotlightLocatorFilter::tr("Locator query string."), [query] { return query; });
  expander->registerVariable("Query:Escaped", SpotlightLocatorFilter::tr("Locator query string with quotes escaped with backslash."), [query] { return escaped(query); });
  expander->registerVariable("Query:EscapedWithWildcards", SpotlightLocatorFilter::tr("Locator query string with quotes escaped with backslash and " "spaces replaced with \"*\" wildcards."), [query] {
    auto quoted = escaped(query);
    quoted.replace(' ', '*');
    return quoted;
  });
  expander->registerVariable("Query:Regex", SpotlightLocatorFilter::tr("Locator query string as regular expression."), [query] {
    auto regex = query;
    regex = regex.replace('*', ".*");
    regex = regex.replace(' ', ".*");
    return regex;
  });
  return expander;
}

SpotlightLocatorFilter::SpotlightLocatorFilter()
{
  setId("SpotlightFileNamesLocatorFilter");
  setDefaultShortcutString("md");
  setDefaultIncludedByDefault(false);
  setDisplayName(tr("File Name Index"));
  setDescription(tr("Matches files from a global file system index (Spotlight, Locate, Everything). Append " "\"+<number>\" or \":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
  setConfigurable(true);
  reset();
}

auto SpotlightLocatorFilter::prepareSearch(const QString &entry) -> void
{
  if (const auto link = Link::fromString(entry, true); link.targetFilePath.isEmpty()) {
    setFileIterator(new ListIterator(FilePaths()));
  } else {
    // only pass the file name part to allow searches like "somepath/*foo"
    const std::unique_ptr<MacroExpander> expander(createMacroExpander(link.targetFilePath.fileName()));
    const auto argument_string = expander->expand(caseSensitivity(link.targetFilePath.toString()) == Qt::CaseInsensitive ? m_arguments : m_case_sensitive_arguments);
    setFileIterator(new SpotlightIterator(QStringList(m_command) + ProcessArgs::splitArgs(argument_string)));
  }
  BaseFileFilter::prepareSearch(entry);
}

auto SpotlightLocatorFilter::openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool
{
  Q_UNUSED(needs_refresh)
  QWidget config_widget;

  const auto layout = new QFormLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  config_widget.setLayout(layout);

  const auto command_edit = new PathChooser;
  command_edit->setExpectedKind(PathChooser::ExistingCommand);
  command_edit->lineEdit()->setText(m_command);

  const auto arguments_edit = new FancyLineEdit;
  arguments_edit->setText(m_arguments);

  const auto case_sensitive_arguments_edit = new FancyLineEdit;
  case_sensitive_arguments_edit->setText(m_case_sensitive_arguments);
  layout->addRow(tr("Executable:"), command_edit);
  layout->addRow(tr("Arguments:"), arguments_edit);
  layout->addRow(tr("Case sensitive:"), case_sensitive_arguments_edit);
  std::unique_ptr<MacroExpander> expander(createMacroExpander(""));

  const auto chooser = new VariableChooser(&config_widget);
  chooser->addMacroExpanderProvider([expander = expander.get()] { return expander; });
  chooser->addSupportedWidget(arguments_edit);
  chooser->addSupportedWidget(case_sensitive_arguments_edit);

  const auto accepted = openConfigDialog(parent, &config_widget);
  if (accepted) {
    m_command = command_edit->rawFilePath().toString();
    m_arguments = arguments_edit->text();
    m_case_sensitive_arguments = case_sensitive_arguments_edit->text();
  }

  return accepted;
}

auto SpotlightLocatorFilter::saveState(QJsonObject &obj) const -> void
{
  if (m_command != defaultCommand())
    obj.insert(k_command_key, m_command);

  if (m_arguments != defaultArguments())
    obj.insert(k_arguments_key, m_arguments);

  if (m_case_sensitive_arguments != defaultArguments(Qt::CaseSensitive))
    obj.insert(k_case_sensitive_key, m_case_sensitive_arguments);
}

auto SpotlightLocatorFilter::restoreState(const QJsonObject &obj) -> void
{
  m_command = obj.value(k_command_key).toString(defaultCommand());
  m_arguments = obj.value(k_arguments_key).toString(defaultArguments());
  m_case_sensitive_arguments = obj.value(k_case_sensitive_key).toString(defaultArguments(Qt::CaseSensitive));
}

auto SpotlightLocatorFilter::reset() -> void
{
  m_command = defaultCommand();
  m_arguments = defaultArguments();
  m_case_sensitive_arguments = defaultArguments(Qt::CaseSensitive);
}

} // namespace Orca::Plugin::Core
