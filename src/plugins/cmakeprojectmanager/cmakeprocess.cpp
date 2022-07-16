// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprocess.hpp"

#include "cmakeparser.hpp"

#include <core/core-progress-manager.hpp>
#include <projectexplorer/buildsystem.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/taskhub.hpp>

#include <utils/stringutils.hpp>

using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

using namespace ProjectExplorer;

static auto stripTrailingNewline(QString str) -> QString
{
  if (str.endsWith('\n'))
    str.chop(1);
  return str;
}

CMakeProcess::CMakeProcess()
{
  connect(&m_cancelTimer, &QTimer::timeout, this, &CMakeProcess::checkForCancelled);
  m_cancelTimer.setInterval(500);
}

CMakeProcess::~CMakeProcess()
{
  m_process.reset();
  m_parser.flush();

  if (m_future) {
    reportCanceled();
    reportFinished();
  }
}

auto CMakeProcess::run(const BuildDirParameters &parameters, const QStringList &arguments) -> void
{
  QTC_ASSERT(!m_process && !m_future, return);

  auto cmake = parameters.cmakeTool();
  QTC_ASSERT(parameters.isValid() && cmake, return);

  const auto cmakeExecutable = cmake->cmakeExecutable();

  const auto sourceDirectory = parameters.sourceDirectory.onDevice(cmakeExecutable);
  const auto buildDirectory = parameters.buildDirectory.onDevice(cmakeExecutable);

  if (!buildDirectory.exists()) {
    auto msg = tr("The build directory \"%1\" does not exist").arg(buildDirectory.toUserOutput());
    BuildSystem::appendBuildSystemOutput(msg + '\n');
    emit finished();
    return;
  }

  if (buildDirectory.needsDevice()) {
    if (cmake->cmakeExecutable().host() != buildDirectory.host()) {
      auto msg = tr("CMake executable \"%1\" and build directory " "\"%2\" must be on the same device.").arg(cmake->cmakeExecutable().toUserOutput(), buildDirectory.toUserOutput());
      BuildSystem::appendBuildSystemOutput(msg + '\n');
      emit finished();
      return;
    }
  }

  const auto parser = new CMakeParser;
  parser->setSourceDirectory(parameters.sourceDirectory.path());
  m_parser.addLineParser(parser);

  // Always use the sourceDir: If we are triggered because the build directory is getting deleted
  // then we are racing against CMakeCache.txt also getting deleted.

  auto process = std::make_unique<QtcProcess>();
  m_processWasCanceled = false;

  m_cancelTimer.start();

  process->setWorkingDirectory(buildDirectory);
  process->setEnvironment(parameters.environment);

  process->setStdOutLineCallback([](const QString &s) {
    BuildSystem::appendBuildSystemOutput(stripTrailingNewline(s));
  });

  process->setStdErrLineCallback([this](const QString &s) {
    m_parser.appendMessage(s, StdErrFormat);
    BuildSystem::appendBuildSystemOutput(stripTrailingNewline(s));
  });

  connect(process.get(), &QtcProcess::finished, this, &CMakeProcess::handleProcessFinished);

  CommandLine commandLine(cmakeExecutable);
  commandLine.addArgs({"-S", sourceDirectory.path(), "-B", buildDirectory.path()});
  commandLine.addArgs(arguments);

  TaskHub::clearTasks(ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM);

  BuildSystem::startNewBuildSystemOutput(tr("Running %1 in %2.").arg(commandLine.toUserOutput()).arg(buildDirectory.toUserOutput()));

  auto future = std::make_unique<QFutureInterface<void>>();
  future->setProgressRange(0, 1);
  Orca::Plugin::Core::ProgressManager::addTimedTask(*future.get(), tr("Configuring \"%1\"").arg(parameters.projectName), "CMake.Configure", 10);

  process->setCommand(commandLine);
  emit started();
  m_elapsed.start();
  process->start();

  m_process = std::move(process);
  m_future = std::move(future);
}

auto CMakeProcess::terminate() -> void
{
  if (m_process) {
    m_processWasCanceled = true;
    m_process->terminate();
  }
}

auto CMakeProcess::state() const -> QProcess::ProcessState
{
  if (m_process)
    return m_process->state();
  return QProcess::NotRunning;
}

auto CMakeProcess::reportCanceled() -> void
{
  QTC_ASSERT(m_future, return);
  m_future->reportCanceled();
}

auto CMakeProcess::reportFinished() -> void
{
  QTC_ASSERT(m_future, return);
  m_future->reportFinished();
  m_future.reset();
}

auto CMakeProcess::setProgressValue(int p) -> void
{
  QTC_ASSERT(m_future, return);
  m_future->setProgressValue(p);
}

auto CMakeProcess::handleProcessFinished() -> void
{
  QTC_ASSERT(m_process && m_future, return);

  m_cancelTimer.stop();

  const auto code = m_process->exitCode();

  QString msg;
  if (m_process->exitStatus() != QProcess::NormalExit) {
    if (m_processWasCanceled) {
      msg = tr("CMake process was canceled by the user.");
    } else {
      msg = tr("CMake process crashed.");
    }
  } else if (code != 0) {
    msg = tr("CMake process exited with exit code %1.").arg(code);
  }
  m_lastExitCode = code;

  if (!msg.isEmpty()) {
    BuildSystem::appendBuildSystemOutput(msg + '\n');
    TaskHub::addTask(BuildSystemTask(Task::Error, msg));
    m_future->reportCanceled();
  } else {
    m_future->setProgressValue(1);
  }

  m_future->reportFinished();

  emit finished();

  const auto elapsedTime = Utils::formatElapsedTime(m_elapsed.elapsed());
  BuildSystem::appendBuildSystemOutput(elapsedTime + '\n');
}

auto CMakeProcess::checkForCancelled() -> void
{
  if (!m_process || !m_future)
    return;

  if (m_future->isCanceled()) {
    m_cancelTimer.stop();
    m_processWasCanceled = true;
    m_process->close();
  }
}

} // namespace Internal
} // namespace CMakeProjectManager
