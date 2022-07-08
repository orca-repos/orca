// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "osparser.hpp"
#include "projectexplorerconstants.hpp"
#include "task.hpp"

#include <utils/hostosinfo.hpp>

using namespace ProjectExplorer;

OsParser::OsParser()
{
  setObjectName(QLatin1String("OsParser"));
}

auto OsParser::handleLine(const QString &line, Utils::OutputFormat type) -> Result
{
  if (type == Utils::StdOutFormat) {
    if (Utils::HostOsInfo::isWindowsHost()) {
      const auto trimmed = line.trimmed();
      if (trimmed == QLatin1String("The process cannot access the file because it is " "being used by another process.")) {
        scheduleTask(CompileTask(Task::Error, tr("The process cannot access the file because it is being used " "by another process.\n" "Please close all running instances of your application before " "starting a build.")), 1);
        m_hasFatalError = true;
        return Status::Done;
      }
    }
    return Status::NotHandled;
  }
  if (Utils::HostOsInfo::isLinuxHost()) {
    const auto trimmed = line.trimmed();
    if (trimmed.contains(QLatin1String(": error while loading shared libraries:"))) {
      scheduleTask(CompileTask(Task::Error, trimmed), 1);
      return Status::Done;
    }
  }
  return Status::NotHandled;
}
