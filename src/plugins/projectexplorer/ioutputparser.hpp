// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "buildstep.hpp"

#include <utils/outputformatter.hpp>

#include <functional>

namespace ProjectExplorer {
class Task;

class PROJECTEXPLORER_EXPORT OutputTaskParser : public Utils::OutputLineParser {
  Q_OBJECT

public:
  OutputTaskParser();
  ~OutputTaskParser() override;

  class TaskInfo {
  public:
    TaskInfo(const Task &t, int l, int s) : task(t), linkedLines(l), skippedLines(s) {}
    Task task;
    int linkedLines = 0;
    int skippedLines = 0;
  };

  auto taskInfo() const -> const QList<TaskInfo>;

protected:
  auto scheduleTask(const Task &task, int outputLines, int skippedLines = 0) -> void;
  auto setDetailsFormat(Task &task, const LinkSpecs &linkSpecs = {}) -> void;

private:
  auto runPostPrintActions(QPlainTextEdit *edit) -> void override;

  class Private;
  Private *const d;
};

} // namespace ProjectExplorer
