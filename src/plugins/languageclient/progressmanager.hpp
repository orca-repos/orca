// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-future-progress.hpp>

#include <QFutureInterface>
#include <QPointer>

namespace LanguageServerProtocol {
class ProgressParams;
class ProgressToken;
class WorkDoneProgressBegin;
class WorkDoneProgressReport;
class WorkDoneProgressEnd;
} // namespace LanguageServerProtocol

namespace LanguageClient {

class ProgressManager {
public:
  ProgressManager();
  ~ProgressManager();

  auto handleProgress(const LanguageServerProtocol::ProgressParams &params) -> void;
  auto setTitleForToken(const LanguageServerProtocol::ProgressToken &token, const QString &message) -> void;
  auto reset() -> void;

  static auto isProgressEndMessage(const LanguageServerProtocol::ProgressParams &params) -> bool;

private:
  auto beginProgress(const LanguageServerProtocol::ProgressToken &token, const LanguageServerProtocol::WorkDoneProgressBegin &begin) -> void;
  auto reportProgress(const LanguageServerProtocol::ProgressToken &token, const LanguageServerProtocol::WorkDoneProgressReport &report) -> void;
  auto endProgress(const LanguageServerProtocol::ProgressToken &token, const LanguageServerProtocol::WorkDoneProgressEnd &end) -> void;
  auto endProgress(const LanguageServerProtocol::ProgressToken &token) -> void;

  struct LanguageClientProgress {
    QPointer<Orca::Plugin::Core::FutureProgress> progressInterface = nullptr;
    QFutureInterface<void> *futureInterface = nullptr;
  };

  QMap<LanguageServerProtocol::ProgressToken, LanguageClientProgress> m_progress;
  QMap<LanguageServerProtocol::ProgressToken, QString> m_titles;
};

} // namespace LanguageClient
