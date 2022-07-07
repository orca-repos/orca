// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processhandle.hpp"

namespace Utils {

/*!
    \class Utils::ProcessHandle
    \brief The ProcessHandle class is a helper class to describe a process.

    Encapsulates parameters of a running process, local (PID) or remote (to be
    done, address, port, and so on).
*/

// That's the same as in QProcess, i.e. Qt doesn't care for process #0.
const qint64 InvalidPid = 0;

ProcessHandle::ProcessHandle() : m_pid(InvalidPid) {}
ProcessHandle::ProcessHandle(qint64 pid) : m_pid(pid) {}

auto ProcessHandle::isValid() const -> bool
{
  return m_pid != InvalidPid;
}

auto ProcessHandle::setPid(qint64 pid) -> void
{
  m_pid = pid;
}

auto ProcessHandle::pid() const -> qint64
{
  return m_pid;
}

auto ProcessHandle::equals(const ProcessHandle &rhs) const -> bool
{
  return m_pid == rhs.m_pid;
}

#ifndef Q_OS_OSX
auto ProcessHandle::activate() -> bool
{
  return false;
}
#endif

} // Utils
