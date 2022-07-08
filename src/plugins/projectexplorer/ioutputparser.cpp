// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ioutputparser.hpp"

#include "task.hpp"
#include "taskhub.hpp"

#include <core/outputwindow.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <utils/algorithm.hpp>
#include <utils/ansiescapecodehandler.hpp>

#include <QPlainTextEdit>


/*!
    \class ProjectExplorer::OutputTaskParser

    \brief The OutputTaskParser class provides an interface for an output parser
    that emits issues (tasks).

    \sa ProjectExplorer::Task
*/

/*!
   \fn ProjectExplorer::OutputTaskParser::Status ProjectExplorer::OutputTaskParser::handleLine(const QString &line, Utils::OutputFormat type)

   Called once for each line of standard output or standard error to parse.
*/

/*!
   \fn bool ProjectExplorer::OutputTaskParser::hasFatalErrors() const

   This is mainly a Symbian specific quirk.
*/

/*!
   \fn void ProjectExplorer::OutputTaskParser::addTask(const ProjectExplorer::Task &task)

   Should be emitted for each task seen in the output.
*/

/*!
   \fn void ProjectExplorer::OutputTaskParser::flush()

   Instructs a parser to flush its state.
   Parsers may have state (for example, because they need to aggregate several
   lines into one task). This
   function is called when this state needs to be flushed out to be visible.
*/

namespace ProjectExplorer {

class OutputTaskParser::Private {
public:
  QList<TaskInfo> scheduledTasks;
};

OutputTaskParser::OutputTaskParser() : d(new Private) { }
OutputTaskParser::~OutputTaskParser() { delete d; }

auto OutputTaskParser::taskInfo() const -> const QList<TaskInfo>
{
  return d->scheduledTasks;
}

auto OutputTaskParser::scheduleTask(const Task &task, int outputLines, int skippedLines) -> void
{
  TaskInfo ts(task, outputLines, skippedLines);
  if (ts.task.type == Task::Error && demoteErrorsToWarnings())
    ts.task.type = Task::Warning;
  d->scheduledTasks << ts;
  QTC_CHECK(d->scheduledTasks.size() <= 2);
}

auto OutputTaskParser::setDetailsFormat(Task &task, const LinkSpecs &linkSpecs) -> void
{
  if (task.details.isEmpty())
    return;

  Utils::FormattedText monospacedText(task.details.join('\n'));
  monospacedText.format.setFont(TextEditor::TextEditorSettings::fontSettings().font());
  monospacedText.format.setFontStyleHint(QFont::Monospace);
  const auto linkifiedText = Utils::OutputFormatter::linkifiedText({monospacedText}, linkSpecs);
  task.formats.clear();
  int offset = task.summary.length() + 1;
  for (const auto &ft : linkifiedText) {
    task.formats << QTextLayout::FormatRange{offset, int(ft.text.length()), ft.format};
    offset += ft.text.length();
  }
}

auto OutputTaskParser::runPostPrintActions(QPlainTextEdit *edit) -> void
{
  auto offset = 0;
  if (const auto ow = qobject_cast<Core::OutputWindow*>(edit)) {
    Utils::reverseForeach(taskInfo(), [ow, &offset](const TaskInfo &ti) {
      ow->registerPositionOf(ti.task.taskId, ti.linkedLines, ti.skippedLines, offset);
      offset += ti.linkedLines;
    });
  }

  for (const auto &t : qAsConst(d->scheduledTasks))
    TaskHub::addTask(t.task);
  d->scheduledTasks.clear();
}

} // namespace ProjectExplorer
