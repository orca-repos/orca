// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qscxmlcgenerator.hpp"

#include <qtsupport/baseqtversion.hpp>
#include <qtsupport/qtkitinformation.hpp>
#include <projectexplorer/target.hpp>
#include <utils/qtcassert.hpp>

#include <QDateTime>
#include <QLoggingCategory>
#include <QUuid>

using namespace ProjectExplorer;

namespace QtSupport {

static QLoggingCategory log("qtc.qscxmlcgenerator", QtWarningMsg);
static constexpr char TaskCategory[] = "Task.Category.ExtraCompiler.QScxmlc";

QScxmlcGenerator::QScxmlcGenerator(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent) : ProcessExtraCompiler(project, source, targets, parent), m_tmpdir("qscxmlgenerator")
{
  QTC_ASSERT(targets.count() == 2, return);
  m_header = m_tmpdir.filePath(targets[0].fileName()).toString();
  m_impl = m_tmpdir.filePath(targets[1].fileName()).toString();
}

auto QScxmlcGenerator::parseIssues(const QByteArray &processStderr) -> Tasks
{
  Tasks issues;
  foreach(const QByteArray &line, processStderr.split('\n')) {
    auto tokens = line.split(':');

    if (tokens.length() > 4) {
      auto file = Utils::FilePath::fromUtf8(tokens[0]);
      const auto line = tokens[1].toInt();
      // int column = tokens[2].toInt(); <- nice, but not needed for now.
      const auto type = tokens[3].trimmed() == "error" ? Task::Error : Task::Warning;
      auto message = QString::fromUtf8(tokens.mid(4).join(':').trimmed());
      issues.append(Task(type, message, file, line, TaskCategory));
    }
  }
  return issues;
}

auto QScxmlcGenerator::command() const -> Utils::FilePath
{
  const QtVersion *version = nullptr;
  Target *target;
  if ((target = project()->activeTarget()))
    version = QtKitAspect::qtVersion(target->kit());
  else
    version = QtKitAspect::qtVersion(KitManager::defaultKit());

  if (!version)
    return Utils::FilePath();

  return version->qscxmlcFilePath();
}

auto QScxmlcGenerator::arguments() const -> QStringList
{
  QTC_ASSERT(!m_header.isEmpty(), return QStringList());

  return QStringList({QLatin1String("--header"), m_header, QLatin1String("--impl"), m_impl, tmpFile().fileName()});
}

auto QScxmlcGenerator::workingDirectory() const -> Utils::FilePath
{
  return m_tmpdir.path();
}

auto QScxmlcGenerator::prepareToRun(const QByteArray &sourceContents) -> bool
{
  const auto fn = tmpFile();
  QFile input(fn.toString());
  if (!input.open(QIODevice::WriteOnly))
    return false;
  input.write(sourceContents);
  input.close();

  return true;
}

auto QScxmlcGenerator::handleProcessFinished(Utils::QtcProcess *process) -> FileNameToContentsHash
{
  Q_UNUSED(process)
  const auto wd = workingDirectory();
  FileNameToContentsHash result;
  forEachTarget([&](const Utils::FilePath &target) {
    const auto file = wd.pathAppended(target.fileName());
    QFile generated(file.toString());
    if (!generated.open(QIODevice::ReadOnly))
      return;
    result[target] = generated.readAll();
  });
  return result;
}

auto QScxmlcGenerator::tmpFile() const -> Utils::FilePath
{
  return workingDirectory().pathAppended(source().fileName());
}

auto QScxmlcGeneratorFactory::sourceType() const -> FileType
{
  return FileType::StateChart;
}

auto QScxmlcGeneratorFactory::sourceTag() const -> QString
{
  return QStringLiteral("scxml");
}

auto QScxmlcGeneratorFactory::create(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets) -> ExtraCompiler*
{
  return new QScxmlcGenerator(project, source, targets, this);
}

} // namespace QtSupport
