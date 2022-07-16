// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "builddirparameters.hpp"

#include <utils/outputformatter.hpp>
#include <utils/qtcprocess.hpp>

#include <QElapsedTimer>
#include <QFutureInterface>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace CMakeProjectManager {
namespace Internal {

class CMakeProcess : public QObject {
  Q_OBJECT

public:
  CMakeProcess();
  CMakeProcess(const CMakeProcess &) = delete;
  ~CMakeProcess();

  auto run(const BuildDirParameters &parameters, const QStringList &arguments) -> void;
  auto terminate() -> void;
  auto state() const -> QProcess::ProcessState;

  // Update progress information:
  auto reportCanceled() -> void;
  auto reportFinished() -> void; // None of the progress related functions will work after this!
  auto setProgressValue(int p) -> void;
  auto lastExitCode() const -> int { return m_lastExitCode; }

signals:
  auto started() -> void;
  auto finished() -> void;

private:
  auto handleProcessFinished() -> void;
  auto checkForCancelled() -> void;

  std::unique_ptr<Utils::QtcProcess> m_process;
  Utils::OutputFormatter m_parser;
  std::unique_ptr<QFutureInterface<void>> m_future;
  bool m_processWasCanceled = false;
  QTimer m_cancelTimer;
  QElapsedTimer m_elapsed;
  int m_lastExitCode = 0;
};

} // namespace Internal
} // namespace CMakeProjectManager
