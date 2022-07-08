// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ldparser.hpp"
#include "projectexplorerconstants.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace ProjectExplorer;

namespace {
// opt. drive letter + filename: (2 brackets)
const char *const FILE_PATTERN = "(([A-Za-z]:)?[^:]+\\.[^:]+):";
// line no. or elf segment + offset (1 bracket)
const char *const POSITION_PATTERN = "(\\S+|\\(\\..+?[+-]0x[a-fA-F0-9]+\\)):";
const char *const COMMAND_PATTERN = "^(.*[\\\\/])?([a-z0-9]+-[a-z0-9]+-[a-z0-9]+-)?(ld|gold)(-[0-9\\.]+)?(\\.exe)?: ";
const char *const RANLIB_PATTERN = "ranlib(.exe)?: (file: (.*) has no symbols)$";
}

LdParser::LdParser()
{
  setObjectName(QLatin1String("LdParser"));
  m_ranlib.setPattern(QLatin1String(RANLIB_PATTERN));
  QTC_CHECK(m_ranlib.isValid());
  m_regExpLinker.setPattern(QLatin1Char('^') + QString::fromLatin1(FILE_PATTERN) + QLatin1Char('(') + QString::fromLatin1(FILE_PATTERN) + QLatin1String(")?(") + QLatin1String(POSITION_PATTERN) + QLatin1String(")?\\s(.+)$"));
  QTC_CHECK(m_regExpLinker.isValid());

  m_regExpGccNames.setPattern(QLatin1String(COMMAND_PATTERN));
  QTC_CHECK(m_regExpGccNames.isValid());
}

auto LdParser::handleLine(const QString &line, Utils::OutputFormat type) -> Result
{
  if (type != Utils::StdErrFormat)
    return Status::NotHandled;

  const auto lne = rightTrimmed(line);
  if (!lne.isEmpty() && !lne.at(0).isSpace() && !m_incompleteTask.isNull()) {
    flush();
    return Status::NotHandled;
  }

  if (lne.startsWith(QLatin1String("TeamBuilder ")) || lne.startsWith(QLatin1String("distcc[")) || lne.contains(QLatin1String("ar: creating "))) {
    return Status::NotHandled;
  }

  // ld on macOS
  if (lne.startsWith("Undefined symbols for architecture") && lne.endsWith(":")) {
    m_incompleteTask = CompileTask(Task::Error, lne);
    return Status::InProgress;
  }
  if (!m_incompleteTask.isNull() && lne.startsWith("  ")) {
    m_incompleteTask.details.append(lne);
    static const QRegularExpression locRegExp("    (?<symbol>\\S+) in (?<file>\\S+)");
    const auto match = locRegExp.match(lne);
    LinkSpecs linkSpecs;
    if (match.hasMatch()) {
      m_incompleteTask.setFile(absoluteFilePath(Utils::FilePath::fromString(match.captured("file"))));
      addLinkSpecForAbsoluteFilePath(linkSpecs, m_incompleteTask.file, 0, match, "file");
    }
    return {Status::InProgress, linkSpecs};
  }

  if (lne.startsWith("collect2:") || lne.startsWith("collect2.exe:")) {
    scheduleTask(CompileTask(Task::Error, lne /* description */), 1);
    return Status::Done;
  }

  auto match = m_ranlib.match(lne);
  if (match.hasMatch()) {
    const auto description = match.captured(2);
    scheduleTask(CompileTask(Task::Warning, description), 1);
    return Status::Done;
  }

  match = m_regExpGccNames.match(lne);
  if (match.hasMatch()) {
    auto description = lne.mid(match.capturedLength());
    auto type = Task::Error;
    if (description.startsWith(QLatin1String("warning: "))) {
      type = Task::Warning;
      description = description.mid(9);
    } else if (description.startsWith(QLatin1String("fatal: "))) {
      description = description.mid(7);
    }
    scheduleTask(CompileTask(type, description), 1);
    return Status::Done;
  }

  match = m_regExpLinker.match(lne);
  if (match.hasMatch() && lne.count(':') >= 2) {
    bool ok;
    auto lineno = match.captured(7).toInt(&ok);
    if (!ok)
      lineno = -1;
    auto filename = absoluteFilePath(Utils::FilePath::fromUserInput(match.captured(1)));
    auto capIndex = 1;
    const auto sourceFileName = match.captured(4);
    if (!sourceFileName.isEmpty() && !sourceFileName.startsWith(QLatin1String("(.text")) && !sourceFileName.startsWith(QLatin1String("(.data"))) {
      filename = absoluteFilePath(Utils::FilePath::fromUserInput(sourceFileName));
      capIndex = 4;
    }
    auto description = match.captured(8).trimmed();
    auto type = Task::Error;
    if (description.startsWith(QLatin1String("first defined here")) || description.startsWith(QLatin1String("note:"), Qt::CaseInsensitive)) {
      type = Task::Unknown;
    } else if (description.startsWith(QLatin1String("warning: "), Qt::CaseInsensitive)) {
      type = Task::Warning;
      description = description.mid(9);
    }
    static const QStringList keywords{
      "File format not recognized",
      "undefined reference",
      "first defined here",
      "feupdateenv is not implemented and will always fail",
      // yes, this is quite special ...
    };
    const auto descriptionContainsKeyword = [&description](const QString &keyword) {
      return description.contains(keyword);
    };
    if (Utils::anyOf(keywords, descriptionContainsKeyword)) {
      LinkSpecs linkSpecs;
      addLinkSpecForAbsoluteFilePath(linkSpecs, filename, lineno, match, capIndex);
      scheduleTask(CompileTask(type, description, filename, lineno), 1);
      return {Status::Done, linkSpecs};
    }
  }

  return Status::NotHandled;
}

auto LdParser::flush() -> void
{
  if (m_incompleteTask.isNull())
    return;
  const auto t = m_incompleteTask;
  m_incompleteTask.clear();
  scheduleTask(t, 1);
}
