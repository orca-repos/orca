// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "devicesupport/idevice.hpp"

#include <utils/outputformat.hpp>
#include <utils/processhandle.hpp>

#include <QProcess>

#include <memory>

namespace Utils {
class ProcessHandle;
}

namespace ProjectExplorer {

class Runnable;

namespace Internal {
class ApplicationLauncherPrivate;
}

class PROJECTEXPLORER_EXPORT ApplicationLauncher : public QObject {
  Q_OBJECT

public:
  explicit ApplicationLauncher(QObject *parent = nullptr);
  ~ApplicationLauncher() override;

  auto setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void;
  auto setUseTerminal(bool on) -> void;
  auto setRunAsRoot(bool on) -> void;
  auto start(const Runnable &runnable) -> void;
  auto start(const Runnable &runnable, const IDevice::ConstPtr &device) -> void;
  auto stop() -> void;
  auto isRunning() const -> bool;
  auto applicationPID() const -> Utils::ProcessHandle;
  auto isLocal() const -> bool;
  auto errorString() const -> QString;
  auto processError() const -> QProcess::ProcessError;
  static auto msgWinCannotRetrieveDebuggingOutput() -> QString;

signals:
  auto appendMessage(const QString &message, Utils::OutputFormat format, bool appendNewLine = true) -> void;
  auto processStarted() -> void;
  auto processExited(int exitCode, QProcess::ExitStatus exitStatus) -> void;
  auto error(QProcess::ProcessError error) -> void;

private:
  std::unique_ptr<Internal::ApplicationLauncherPrivate> d;
};

} // namespace ProjectExplorer
