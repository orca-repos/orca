// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "progressmanager.hpp"

#include <core/core-future-progress.hpp>
#include <core/core-progress-manager.hpp>
#include <languageserverprotocol/progresssupport.h>

using namespace LanguageServerProtocol;

namespace LanguageClient {

ProgressManager::ProgressManager() {}

ProgressManager::~ProgressManager()
{
  reset();
}

auto ProgressManager::handleProgress(const LanguageServerProtocol::ProgressParams &params) -> void
{
  const auto &token = params.token();
  const auto value = params.value();
  if (const auto begin = Utils::get_if<WorkDoneProgressBegin>(&value))
    beginProgress(token, *begin);
  else if (const auto report = Utils::get_if<WorkDoneProgressReport>(&value))
    reportProgress(token, *report);
  else if (const auto end = Utils::get_if<WorkDoneProgressEnd>(&value))
    endProgress(token, *end);
}

auto ProgressManager::setTitleForToken(const LanguageServerProtocol::ProgressToken &token, const QString &message) -> void
{
  m_titles.insert(token, message);
}

auto ProgressManager::reset() -> void
{
  const auto &tokens = m_progress.keys();
  for (const auto &token : tokens)
    endProgress(token);
}

auto ProgressManager::isProgressEndMessage(const LanguageServerProtocol::ProgressParams &params) -> bool
{
  return Utils::holds_alternative<WorkDoneProgressEnd>(params.value());
}

auto languageClientProgressId(const ProgressToken &token) -> Utils::Id
{
  constexpr char k_LanguageClientProgressId[] = "LanguageClient.ProgressId.";
  auto toString = [](const ProgressToken &token) {
    if (Utils::holds_alternative<int>(token))
      return QString::number(Utils::get<int>(token));
    return Utils::get<QString>(token);
  };
  return Utils::Id(k_LanguageClientProgressId).withSuffix(toString(token));
}

auto ProgressManager::beginProgress(const ProgressToken &token, const WorkDoneProgressBegin &begin) -> void
{
  const auto interface = new QFutureInterface<void>();
  interface->reportStarted();
  interface->setProgressRange(0, 100); // LSP always reports percentage of the task
  const auto title = m_titles.value(token, begin.title());
  const auto progress = Orca::Plugin::Core::ProgressManager::addTask(interface->future(), title, languageClientProgressId(token));
  m_progress[token] = {progress, interface};
  reportProgress(token, begin);
}

auto ProgressManager::reportProgress(const ProgressToken &token, const WorkDoneProgressReport &report) -> void
{
  const auto &progress = m_progress.value(token);
  if (progress.progressInterface) {
    const auto &message = report.message();
    if (message.has_value()) {
      progress.progressInterface->setSubtitle(*message);
      const auto showSubtitle = !message->isEmpty();
      progress.progressInterface->setSubtitleVisibleInStatusBar(showSubtitle);
    }
  }
  if (progress.futureInterface) {
    if (const auto &percentage = report.percentage(); percentage.has_value())
      progress.futureInterface->setProgressValue(*percentage);
  }
}

auto ProgressManager::endProgress(const ProgressToken &token, const WorkDoneProgressEnd &end) -> void
{
  const auto &progress = m_progress.value(token);
  const auto &message = end.message().value_or(QString());
  if (progress.progressInterface) {
    if (!message.isEmpty()) {
      progress.progressInterface->setKeepOnFinish(Orca::Plugin::Core::FutureProgress::KeepOnFinishTillUserInteraction);
    }
    progress.progressInterface->setSubtitle(message);
    progress.progressInterface->setSubtitleVisibleInStatusBar(!message.isEmpty());
  }
  endProgress(token);
}

auto ProgressManager::endProgress(const ProgressToken &token) -> void
{
  const auto &progress = m_progress.take(token);
  if (progress.futureInterface)
    progress.futureInterface->reportFinished();
  delete progress.futureInterface;
}

} // namespace LanguageClient
