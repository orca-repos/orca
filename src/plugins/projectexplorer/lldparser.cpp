// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lldparser.hpp"

#include "projectexplorerconstants.hpp"
#include "task.hpp"

#include <utils/fileutils.hpp>

#include <QStringList>

namespace ProjectExplorer {
namespace Internal {

auto LldParser::handleLine(const QString &line, Utils::OutputFormat type) -> Result
{
  if (type != Utils::StdErrFormat)
    return Status::NotHandled;

  const auto trimmedLine = rightTrimmed(line);
  if (trimmedLine.contains("error:") && trimmedLine.contains("lld")) {
    scheduleTask(CompileTask(Task::Error, trimmedLine), 1);
    return Status::Done;
  }
  static const QStringList prefixes{">>> referenced by ", ">>> defined at ", ">>> "};
  for (const auto &prefix : prefixes) {
    if (!trimmedLine.startsWith(prefix))
      continue;
    auto lineNo = -1;
    const int locOffset = trimmedLine.lastIndexOf(':');
    if (locOffset != -1) {
      const int endLocOffset = trimmedLine.indexOf(')', locOffset);
      const auto numberWidth = endLocOffset == -1 ? -1 : endLocOffset - locOffset - 1;
      auto isNumber = true;
      lineNo = trimmedLine.mid(locOffset + 1, numberWidth).toInt(&isNumber);
      if (!isNumber)
        lineNo = -1;
    }
    int filePathOffset = trimmedLine.lastIndexOf('(', locOffset);
    if (filePathOffset != -1)
      ++filePathOffset;
    else
      filePathOffset = prefix.length();
    const auto filePathLen = locOffset == -1 ? -1 : locOffset - filePathOffset;
    const auto file = absoluteFilePath(Utils::FilePath::fromUserInput(trimmedLine.mid(filePathOffset, filePathLen).trimmed()));
    LinkSpecs linkSpecs;
    addLinkSpecForAbsoluteFilePath(linkSpecs, file, lineNo, filePathOffset, filePathLen);
    scheduleTask(CompileTask(Task::Unknown, trimmedLine.mid(4).trimmed(), file, lineNo), 1);
    return {Status::Done, linkSpecs};
  }
  return Status::NotHandled;
}

} // namespace Internal
} // namespace ProjectExplorer
