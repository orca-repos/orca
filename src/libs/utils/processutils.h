// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QIODevice>
#include <QProcess>

namespace Utils {

enum class ProcessMode {
  Reader,
  // This opens in ReadOnly mode if no write data or in ReadWrite mode otherwise,
  // closes the write channel afterwards
  Writer // This opens in ReadWrite mode and doesn't close the write channel
};

class ProcessStartHandler {
public:
  ProcessStartHandler(QProcess *process) : m_process(process) {}

  auto setProcessMode(ProcessMode mode) -> void { m_processMode = mode; }
  auto setWriteData(const QByteArray &writeData) -> void { m_writeData = writeData; }
  auto openMode() const -> QIODevice::OpenMode;
  auto handleProcessStart() -> void;
  auto handleProcessStarted() -> void;
  auto setBelowNormalPriority() -> void;
  auto setNativeArguments(const QString &arguments) -> void;

private:
  ProcessMode m_processMode = ProcessMode::Reader;
  QByteArray m_writeData;
  QProcess *m_process;
};

class ProcessHelper : public QProcess {
public:
  using QProcess::setErrorString;

  ProcessHelper(QObject *parent) : QProcess(parent), m_processStartHandler(this)
  {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && defined(Q_OS_UNIX)
        setChildProcessModifier([this] { setupChildProcess_impl(); });
    #endif
  }

  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void setupChildProcess() override { setupChildProcess_impl(); }
  #endif

  auto processStartHandler() -> ProcessStartHandler* { return &m_processStartHandler; }
  auto setLowPriority() -> void { m_lowPriority = true; }
  auto setUnixTerminalDisabled() -> void { m_unixTerminalDisabled = true; }

private:
  auto setupChildProcess_impl() -> void;
  bool m_lowPriority = false;
  bool m_unixTerminalDisabled = false;
  ProcessStartHandler m_processStartHandler;
};

} // namespace Utils

Q_DECLARE_METATYPE(Utils::ProcessMode);
