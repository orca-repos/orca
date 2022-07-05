// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "shellcommand.h"
#include "icore.h"

#include <core/progressmanager/progressmanager.h>

#include <QFutureInterface>
#include <QFutureWatcher>

using namespace Utils;

namespace Core {

ShellCommand::ShellCommand(const FilePath &working_directory, const Environment &environment) : Utils::ShellCommand(working_directory, environment)
{
  connect(ICore::instance(), &ICore::coreAboutToClose, this, &ShellCommand::coreAboutToClose);
}

auto ShellCommand::futureProgress() const -> FutureProgress*
{
  return m_progress.data();
}

auto ShellCommand::addTask(QFuture<void> &future) -> void
{
  const auto name = displayName();
  const auto id = Id::fromString(name + QLatin1String(".action"));

  if (hasProgressParser()) {
    m_progress = ProgressManager::addTask(future, name, id);
  } else {
    // add a timed tasked based on timeout
    // we cannot access the future interface directly, so we need to create a new one
    // with the same lifetime
    auto fi = new QFutureInterface<void>();
    auto watcher = new QFutureWatcher<void>();
    connect(watcher, &QFutureWatcherBase::finished, [fi, watcher] {
      fi->reportFinished();
      delete fi;
      watcher->deleteLater();
    });
    watcher->setFuture(future);
    m_progress = ProgressManager::addTimedTask(*fi, name, id, qMax(2, timeoutS() / 5)/*itsmagic*/);
  }
}

auto ShellCommand::coreAboutToClose() -> void
{
  abort();
}

} // namespace Core
