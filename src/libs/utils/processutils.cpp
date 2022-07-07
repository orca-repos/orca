// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processutils.hpp"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <errno.hpp>
#include <stdio.hpp>
#include <unistd.hpp>
#endif

namespace Utils {

auto ProcessStartHandler::openMode() const -> QIODevice::OpenMode
{
  if (m_processMode == ProcessMode::Writer)
    return QIODevice::ReadWrite; // some writers also read data
  if (m_writeData.isEmpty())
    return QIODevice::ReadOnly; // only reading
  return QIODevice::ReadWrite;  // initial write and then reading (close the write channel)
}

auto ProcessStartHandler::handleProcessStart() -> void
{
  if (m_processMode == ProcessMode::Writer)
    return;
  if (m_writeData.isEmpty())
    m_process->closeWriteChannel();
}

auto ProcessStartHandler::handleProcessStarted() -> void
{
  if (!m_writeData.isEmpty()) {
    m_process->write(m_writeData);
    m_writeData = {};
    if (m_processMode == ProcessMode::Reader)
      m_process->closeWriteChannel();
  }
}

auto ProcessStartHandler::setBelowNormalPriority() -> void
{
  #ifdef Q_OS_WIN
  m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
    args->flags |= BELOW_NORMAL_PRIORITY_CLASS;
  });
  #endif // Q_OS_WIN
}

auto ProcessStartHandler::setNativeArguments(const QString &arguments) -> void
{
  #ifdef Q_OS_WIN
  if (!arguments.isEmpty())
    m_process->setNativeArguments(arguments);
  #else
    Q_UNUSED(arguments)
  #endif // Q_OS_WIN
}

auto ProcessHelper::setupChildProcess_impl() -> void
{
  #if defined Q_OS_UNIX
    // nice value range is -20 to +19 where -20 is highest, 0 default and +19 is lowest
    if (m_lowPriority) {
        errno = 0;
        if (::nice(5) == -1 && errno != 0)
            perror("Failed to set nice value");
    }

    // Disable terminal by becoming a session leader.
    if (m_unixTerminalDisabled)
        setsid();
  #endif
}

} // namespace Utils
