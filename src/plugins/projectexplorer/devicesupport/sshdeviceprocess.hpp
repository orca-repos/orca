// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "deviceprocess.hpp"

#include <memory>

namespace ProjectExplorer {

class Runnable;

class PROJECTEXPLORER_EXPORT SshDeviceProcess : public DeviceProcess {
  Q_OBJECT

public:
  explicit SshDeviceProcess(const QSharedPointer<const IDevice> &device, QObject *parent = nullptr);
  ~SshDeviceProcess() override;

  auto start(const Runnable &runnable) -> void override;
  auto interrupt() -> void override;
  auto terminate() -> void override;
  auto kill() -> void override;
  auto state() const -> QProcess::ProcessState override;
  auto exitStatus() const -> QProcess::ExitStatus override;
  auto exitCode() const -> int override;
  auto errorString() const -> QString override;
  auto readAllStandardOutput() -> QByteArray override;
  auto readAllStandardError() -> QByteArray override;
  auto write(const QByteArray &data) -> qint64 override;

private:
  auto handleConnected() -> void;
  auto handleConnectionError() -> void;
  auto handleDisconnected() -> void;
  auto handleProcessStarted() -> void;
  auto handleProcessFinished(const QString &error) -> void;
  auto handleKillOperationFinished(const QString &errorMessage) -> void;
  auto handleKillOperationTimeout() -> void;
  virtual auto fullCommandLine(const Runnable &runnable) const -> QString;
  virtual auto processId() const -> qint64;

  class SshDeviceProcessPrivate;
  friend class SshDeviceProcessPrivate;
  const std::unique_ptr<SshDeviceProcessPrivate> d;
};

} // namespace ProjectExplorer
