// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "executefilter.hpp"

#include <core/icore.hpp>
#include <core/messagemanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcassert.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

using namespace Core;
using namespace Internal;

using namespace Utils;

ExecuteFilter::ExecuteFilter()
{
  setId("Execute custom commands");
  setDisplayName(tr("Execute Custom Commands"));
  setDescription(tr("Runs an arbitrary command with arguments. The command is searched for in the PATH " "environment variable if needed. Note that the command is run directly, not in a shell."));
  setDefaultShortcutString("!");
  setPriority(High);
  setDefaultIncludedByDefault(false);
}

ExecuteFilter::~ExecuteFilter()
{
  removeProcess();
}

auto ExecuteFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> value;

  if (!entry.isEmpty()) // avoid empty entry
    value.append(LocatorFilterEntry(this, entry, QVariant()));

  QList<LocatorFilterEntry> others;
  const auto entry_case_sensitivity = caseSensitivity(entry);

  for (const auto &cmd : qAsConst(m_command_history)) {
    if (future.isCanceled())
      break;
    if (cmd == entry) // avoid repeated entry
      continue;

    LocatorFilterEntry filter_entry(this, cmd, QVariant());

    if (const auto index = static_cast<int>(cmd.indexOf(entry, 0, entry_case_sensitivity)); index >= 0) {
      filter_entry.highlight_info = {index, static_cast<int>(entry.length())};
      value.append(filter_entry);
    } else {
      others.append(filter_entry);
    }
  }

  value.append(others);
  return value;
}

auto ExecuteFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(new_text)
  Q_UNUSED(selection_start)
  Q_UNUSED(selection_length)

  const auto p = const_cast<ExecuteFilter*>(this);
  const auto value = selection.display_name.trimmed();
  const auto index = static_cast<int>(m_command_history.indexOf(value));

  if (index != -1 && index != 0)
    p->m_command_history.removeAt(index);

  if (index != 0)
    p->m_command_history.prepend(value);

  static constexpr auto max_history = 100;

  while (p->m_command_history.size() > max_history)
    p->m_command_history.removeLast();

  bool found;
  auto working_directory = globalMacroExpander()->value("CurrentDocument:Path", &found);

  if (!found || working_directory.isEmpty())
    working_directory = globalMacroExpander()->value("CurrentDocument:Project:Path", &found);

  ExecuteData d;
  d.command = CommandLine::fromUserInput(value, globalMacroExpander());
  d.working_directory = FilePath::fromString(working_directory);

  if (m_process) {
    const auto info(tr("Previous command is still running (\"%1\").\nDo you want to kill it?").arg(p->headCommand()));
    const int r = QMessageBox::question(ICore::dialogParent(), tr("Kill Previous Process?"), info, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

    if (r == QMessageBox::Cancel)
      return;

    if (r == QMessageBox::No) {
      p->m_task_queue.enqueue(d);
      return;
    }
    p->removeProcess();
  }

  p->m_task_queue.enqueue(d);
  p->runHeadCommand();
}

auto ExecuteFilter::finished() -> void
{
  QTC_ASSERT(m_process, return);
  const auto command_name = headCommand();
  QString message;

  if (m_process->result() == QtcProcess::FinishedWithSuccess)
    message = tr("Command \"%1\" finished.").arg(command_name);
  else
    message = tr("Command \"%1\" failed.").arg(command_name);

  MessageManager::writeFlashing(message);

  removeProcess();
  runHeadCommand();
}

auto ExecuteFilter::readStandardOutput() -> void
{
  QTC_ASSERT(m_process, return);
  const auto data = m_process->readAllStandardOutput();
  MessageManager::writeSilently(QTextCodec::codecForLocale()->toUnicode(data.constData(), static_cast<int>(data.size()), &m_stdout_state));
}

auto ExecuteFilter::readStandardError() -> void
{
  QTC_ASSERT(m_process, return);
  const auto data = m_process->readAllStandardError();
  MessageManager::writeSilently(QTextCodec::codecForLocale()->toUnicode(data.constData(), static_cast<int>(data.size()), &m_stderr_state));
}

auto ExecuteFilter::runHeadCommand() -> void
{
  if (!m_task_queue.isEmpty()) {
    const auto &[command, workingDirectory] = m_task_queue.head();

    if (command.executable().isEmpty()) {
      MessageManager::writeDisrupting(tr("Could not find executable for \"%1\".").arg(command.executable().toUserOutput()));
      m_task_queue.dequeue();
      runHeadCommand();
      return;
    }

    MessageManager::writeDisrupting(tr("Starting command \"%1\".").arg(headCommand()));
    QTC_CHECK(!m_process);
    createProcess();
    m_process->setWorkingDirectory(workingDirectory);
    m_process->setCommand(command);
    m_process->start();

    if (!m_process->waitForStarted(1000)) {
      MessageManager::writeFlashing(tr("Could not start process: %1.").arg(m_process->errorString()));
      removeProcess();
      runHeadCommand();
    }
  }
}

auto ExecuteFilter::createProcess() -> void
{
  if (m_process)
    return;

  m_process = new QtcProcess();
  m_process->setEnvironment(Environment::systemEnvironment());

  connect(m_process, &QtcProcess::finished, this, &ExecuteFilter::finished);
  connect(m_process, &QtcProcess::readyReadStandardOutput, this, &ExecuteFilter::readStandardOutput);
  connect(m_process, &QtcProcess::readyReadStandardError, this, &ExecuteFilter::readStandardError);
}

auto ExecuteFilter::removeProcess() -> void
{
  if (!m_process)
    return;

  m_task_queue.dequeue();
  delete m_process;
  m_process = nullptr;
}

constexpr char history_key[] = "history";

auto ExecuteFilter::saveState(QJsonObject &object) const -> void
{
  if (!m_command_history.isEmpty())
    object.insert(history_key, QJsonArray::fromStringList(m_command_history));
}

auto ExecuteFilter::restoreState(const QJsonObject &object) -> void
{
  m_command_history = transform(object.value(history_key).toArray().toVariantList(), &QVariant::toString);
}

auto ExecuteFilter::headCommand() const -> QString
{
  if (m_task_queue.isEmpty())
    return {};

  const auto &[command, workingDirectory] = m_task_queue.head();
  return command.toUserOutput();
}
