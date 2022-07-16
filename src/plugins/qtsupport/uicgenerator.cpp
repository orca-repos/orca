// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "uicgenerator.hpp"
#include "baseqtversion.hpp"
#include "qtkitinformation.hpp"

#include <projectexplorer/kitmanager.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/buildconfiguration.hpp>

#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QFileInfo>
#include <QDir>
#include <QLoggingCategory>
#include <QDateTime>

using namespace ProjectExplorer;

namespace QtSupport {

UicGenerator::UicGenerator(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets, QObject *parent) : ProcessExtraCompiler(project, source, targets, parent)
{
  QTC_ASSERT(targets.count() == 1, return);
}

auto UicGenerator::command() const -> Utils::FilePath
{
  const QtVersion *version = nullptr;
  Target *target;
  if ((target = project()->activeTarget()))
    version = QtKitAspect::qtVersion(target->kit());
  else
    version = QtKitAspect::qtVersion(KitManager::defaultKit());

  if (!version)
    return Utils::FilePath();

  return version->uicFilePath();
}

auto UicGenerator::arguments() const -> QStringList
{
  return {"-p"};
}

auto UicGenerator::handleProcessFinished(Utils::QtcProcess *process) -> FileNameToContentsHash
{
  FileNameToContentsHash result;
  if (process->exitStatus() != QProcess::NormalExit && process->exitCode() != 0)
    return result;

  const auto targetList = targets();
  if (targetList.size() != 1)
    return result;
  // As far as I can discover in the UIC sources, it writes out local 8-bit encoding. The
  // conversion below is to normalize both the encoding, and the line terminators.
  auto content = QString::fromLocal8Bit(process->readAllStandardOutput()).toUtf8();
  content.prepend("#pragma once\n");
  result[targetList.first()] = content;
  return result;
}

auto UicGeneratorFactory::sourceType() const -> FileType
{
  return FileType::Form;
}

auto UicGeneratorFactory::sourceTag() const -> QString
{
  return QLatin1String("ui");
}

auto UicGeneratorFactory::create(const Project *project, const Utils::FilePath &source, const Utils::FilePaths &targets) -> ExtraCompiler*
{
  return new UicGenerator(project, source, targets, this);
}

} // QtSupport
