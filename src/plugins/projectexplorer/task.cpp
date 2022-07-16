// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "task.hpp"

#include "fileinsessionfinder.hpp"
#include "projectexplorerconstants.hpp"

#include <app/app_version.hpp>
#include <texteditor/textmark.hpp>

#include <utils/algorithm.hpp>
#include <utils/utilsicons.hpp>
#include <utils/qtcassert.hpp>

#include <QFileInfo>
#include <QTextStream>

using namespace Utils;

namespace ProjectExplorer {

static auto taskTypeIcon(Task::TaskType t) -> QIcon
{
  static QIcon icons[3] = {QIcon(), Icons::CRITICAL.icon(), Icons::WARNING.icon()};

  if (t < 0 || t > 2)
    t = Task::Unknown;

  return icons[t];
}

unsigned int Task::s_nextId = 1;

/*!
    \class  ProjectExplorer::Task
    \brief The Task class represents a build issue (warning or error).
    \sa ProjectExplorer::TaskHub
*/

Task::Task(TaskType type_, const QString &description, const FilePath &file_, int line_, Id category_, const QIcon &icon, Options options) : taskId(s_nextId), type(type_), options(options), summary(description), line(line_), movedLine(line_), category(category_), m_icon(icon)
{
  ++s_nextId;
  setFile(file_);
  auto desc = description.split('\n');
  if (desc.length() > 1) {
    summary = desc.first();
    details = desc.mid(1);
  }
}

auto Task::compilerMissingTask() -> Task
{
  return BuildSystemTask(Error, tr("%1 needs a compiler set up to build. " "Configure a compiler in the kit options.").arg(Orca::Plugin::Core::IDE_DISPLAY_NAME));
}

auto Task::setMark(TextEditor::TextMark *mark) -> void
{
  QTC_ASSERT(mark, return);
  QTC_ASSERT(m_mark.isNull(), return);
  m_mark = QSharedPointer<TextEditor::TextMark>(mark);
}

auto Task::isNull() const -> bool
{
  return taskId == 0;
}

auto Task::clear() -> void
{
  taskId = 0;
  type = Unknown;
  summary.clear();
  details.clear();
  file = FilePath();
  line = -1;
  movedLine = -1;
  category = Id();
  m_icon = QIcon();
  formats.clear();
  m_mark.clear();
}

auto Task::setFile(const FilePath &file_) -> void
{
  file = file_;
  if (!file.isEmpty() && !file.toFileInfo().isAbsolute()) {
    auto possiblePaths = findFileInSession(file);
    if (possiblePaths.length() == 1)
      file = possiblePaths.first();
    else
      fileCandidates = possiblePaths;
  }
}

auto Task::description() const -> QString
{
  auto desc = summary;
  if (!details.isEmpty())
    desc.append('\n').append(details.join('\n'));
  return desc;
}

auto Task::icon() const -> QIcon
{
  if (m_icon.isNull())
    m_icon = taskTypeIcon(type);
  return m_icon;
}

//
// functions
//
auto operator==(const Task &t1, const Task &t2) -> bool
{
  return t1.taskId == t2.taskId;
}

auto operator<(const Task &a, const Task &b) -> bool
{
  if (a.type != b.type) {
    if (a.type == Task::Error)
      return true;
    if (b.type == Task::Error)
      return false;
    if (a.type == Task::Warning)
      return true;
    if (b.type == Task::Warning)
      return false;
    // Can't happen
    return true;
  } else {
    if (a.category < b.category)
      return true;
    if (b.category < a.category)
      return false;
    return a.taskId < b.taskId;
  }
}

auto qHash(const Task &task) -> QHashValueType
{
  return task.taskId;
}

auto toHtml(const Tasks &issues) -> QString
{
  QString result;
  QTextStream str(&result);

  for (const auto &t : issues) {
    str << "<b>";
    switch (t.type) {
    case Task::Error:
      str << QCoreApplication::translate("ProjectExplorer::Kit", "Error:") << " ";
      break;
    case Task::Warning:
      str << QCoreApplication::translate("ProjectExplorer::Kit", "Warning:") << " ";
      break;
    case Task::Unknown: default:
      break;
    }
    str << "</b>" << t.description() << "<br>";
  }
  return result;
}

auto containsType(const Tasks &issues, Task::TaskType type) -> bool
{
  return contains(issues, [type](const Task &t) { return t.type == type; });
}

// CompilerTask

CompileTask::CompileTask(TaskType type, const QString &desc, const FilePath &file, int line, int column_) : Task(type, desc, file, line, Constants::TASK_CATEGORY_COMPILE)
{
  column = column_;
}

// BuildSystemTask

BuildSystemTask::BuildSystemTask(TaskType type, const QString &desc, const FilePath &file, int line) : Task(type, desc, file, line, Constants::TASK_CATEGORY_BUILDSYSTEM) {}

// DeploymentTask

DeploymentTask::DeploymentTask(TaskType type, const QString &desc) : Task(type, desc, {}, -1, Constants::TASK_CATEGORY_DEPLOYMENT) {}

} // namespace ProjectExplorer
