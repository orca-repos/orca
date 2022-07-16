// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildstep.hpp"

#include <QProcess>

namespace Utils {
class CommandLine;
}

namespace ProjectExplorer {

class ProcessParameters;

class PROJECTEXPLORER_EXPORT AbstractProcessStep : public BuildStep {
  Q_OBJECT

public:
  auto processParameters() -> ProcessParameters*;
  auto setupProcessParameters(ProcessParameters *params) const -> void;
  auto ignoreReturnValue() const -> bool;
  auto setIgnoreReturnValue(bool b) -> void;
  auto setCommandLineProvider(const std::function<Utils::CommandLine()> &provider) -> void;
  auto setWorkingDirectoryProvider(const std::function<Utils::FilePath()> &provider) -> void;
  auto setEnvironmentModifier(const std::function<void(Utils::Environment &)> &modifier) -> void;
  auto setUseEnglishOutput() -> void;
  auto emitFaultyConfigurationMessage() -> void;

protected:
  AbstractProcessStep(BuildStepList *bsl, Utils::Id id);
  ~AbstractProcessStep() override;

  auto init() -> bool override;
  auto setupOutputFormatter(Utils::OutputFormatter *formatter) -> void override;
  auto doRun() -> void override;
  auto doCancel() -> void override;
  auto setLowPriority() -> void;

  virtual auto finish(bool success) -> void;
  virtual auto processStarted() -> void;
  virtual auto processFinished(int exitCode, QProcess::ExitStatus status) -> void;
  virtual auto processStartupFailed() -> void;
  virtual auto processSucceeded(int exitCode, QProcess::ExitStatus status) -> bool;
  virtual auto stdOutput(const QString &output) -> void;
  virtual auto stdError(const QString &output) -> void;

private:
  auto processReadyReadStdOutput() -> void;
  auto processReadyReadStdError() -> void;
  auto slotProcessFinished() -> void;

  class Private;
  Private *d;
};

} // namespace ProjectExplorer
