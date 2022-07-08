// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "copytaskhandler.hpp"

#include <core/coreconstants.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>

using namespace ProjectExplorer;
using namespace Internal;

auto CopyTaskHandler::handle(const Tasks &tasks) -> void
{
  QStringList lines;
  for (const auto &task : tasks) {
    QString type;
    switch (task.type) {
    case Task::Error:
      //: Task is of type: error
      type = tr("error:") + QLatin1Char(' ');
      break;
    case Task::Warning:
      //: Task is of type: warning
      type = tr("warning:") + QLatin1Char(' ');
      break;
    default:
      break;
    }
    lines << task.file.toUserOutput() + ':' + QString::number(task.line) + ": " + type + task.description();
  }
  QApplication::clipboard()->setText(lines.join('\n'));
}

auto CopyTaskHandler::actionManagerId() const -> Utils::Id
{
  return Utils::Id(Core::Constants::COPY);
}

auto CopyTaskHandler::createAction(QObject *parent) const -> QAction*
{
  return new QAction(parent);
}
