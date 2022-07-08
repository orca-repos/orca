// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "desktopprocesssignaloperation.hpp"

#include "localprocesslist.hpp"

#include <app/app_version.hpp>

#include <utils/winutils.hpp>
#include <utils/fileutils.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

#ifdef Q_OS_WIN
#include <Windows.h>
#ifndef PROCESS_SUSPEND_RESUME
#define PROCESS_SUSPEND_RESUME 0x0800
#endif // PROCESS_SUSPEND_RESUME
#else // Q_OS_WIN
#include <errno.hpp>
#include <signal.hpp>
#endif // else Q_OS_WIN

namespace ProjectExplorer {

auto DesktopProcessSignalOperation::killProcess(qint64 pid) -> void
{
  killProcessSilently(pid);
  emit finished(m_errorMessage);
}

auto DesktopProcessSignalOperation::killProcess(const QString &filePath) -> void
{
  m_errorMessage.clear();
  foreach(const DeviceProcessItem &process, Internal::LocalProcessList::getLocalProcesses()) {
    if (process.cmdLine == filePath)
      killProcessSilently(process.pid);
  }
  emit finished(m_errorMessage);
}

auto DesktopProcessSignalOperation::interruptProcess(qint64 pid) -> void
{
  m_errorMessage.clear();
  interruptProcessSilently(pid);
  emit finished(m_errorMessage);
}

auto DesktopProcessSignalOperation::interruptProcess(const QString &filePath) -> void
{
  m_errorMessage.clear();
  foreach(const DeviceProcessItem &process, Internal::LocalProcessList::getLocalProcesses()) {
    if (process.cmdLine == filePath)
      interruptProcessSilently(process.pid);
  }
  emit finished(m_errorMessage);
}

auto DesktopProcessSignalOperation::appendMsgCannotKill(qint64 pid, const QString &why) -> void
{
  if (!m_errorMessage.isEmpty())
    m_errorMessage += QChar::fromLatin1('\n');
  m_errorMessage += tr("Cannot kill process with pid %1: %2").arg(pid).arg(why);
  m_errorMessage += QLatin1Char(' ');
}

auto DesktopProcessSignalOperation::appendMsgCannotInterrupt(qint64 pid, const QString &why) -> void
{
  if (!m_errorMessage.isEmpty())
    m_errorMessage += QChar::fromLatin1('\n');
  m_errorMessage += tr("Cannot interrupt process with pid %1: %2").arg(pid).arg(why);
  m_errorMessage += QLatin1Char(' ');
}

auto DesktopProcessSignalOperation::killProcessSilently(qint64 pid) -> void
{
  #ifdef Q_OS_WIN
  const DWORD rights = PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_DUP_HANDLE | PROCESS_TERMINATE | PROCESS_CREATE_THREAD | PROCESS_SUSPEND_RESUME;
  if (const auto handle = OpenProcess(rights, FALSE, DWORD(pid))) {
    if (!TerminateProcess(handle, UINT(-1)))
      appendMsgCannotKill(pid, Utils::winErrorMessage(GetLastError()));
    CloseHandle(handle);
  } else {
    appendMsgCannotKill(pid, tr("Cannot open process."));
  }
  #else
    if (pid <= 0)
        appendMsgCannotKill(pid, tr("Invalid process id."));
    else if (kill(pid, SIGKILL))
        appendMsgCannotKill(pid, QString::fromLocal8Bit(strerror(errno)));
  #endif // Q_OS_WIN
}

auto DesktopProcessSignalOperation::interruptProcessSilently(qint64 pid) -> void
{
  #ifdef Q_OS_WIN
  enum SpecialInterrupt {
    NoSpecialInterrupt,
    Win32Interrupt,
    Win64Interrupt
  };

  const auto is64BitSystem = Utils::is64BitWindowsSystem();
  auto si = NoSpecialInterrupt;
  if (is64BitSystem)
    si = is64BitWindowsBinary(m_debuggerCommand) ? Win64Interrupt : Win32Interrupt;
  /*
  Windows 64 bit has a 32 bit subsystem (WOW64) which makes it possible to run a
  32 bit application inside a 64 bit environment.
  When GDB is used DebugBreakProcess must be called from the same system (32/64 bit) running
  the inferior. If CDB is used we could in theory break wow64 processes,
  but the break is actually a wow64 breakpoint. CDB is configured to ignore these
  breakpoints, because they also appear on module loading.
  Therefore we need helper executables (win(32/64)interrupt.exe) on Windows 64 bit calling
  DebugBreakProcess from the correct system.

  DebugBreak matrix for windows

  Api = UseDebugBreakApi
  Win64 = UseWin64InterruptHelper
  Win32 = UseWin32InterruptHelper
  N/A = This configuration is not possible

        | Windows 32bit   | Windows 64bit
        | QtCreator 32bit | QtCreator 32bit                   | QtCreator 64bit
        | Inferior 32bit  | Inferior 32bit  | Inferior 64bit  | Inferior 32bit  | Inferior 64bit
----------|-----------------|-----------------|-----------------|-----------------|----------------
CDB 32bit | Api             | Api             | N/A             | Win32           | N/A
  64bit | N/A             | Win64           | Win64           | Api             | Api
----------|-----------------|-----------------|-----------------|-----------------|----------------
GDB 32bit | Api             | Api             | N/A             | Win32           | N/A
  64bit | N/A             | N/A             | Win64           | N/A             | Api
----------|-----------------|-----------------|-----------------|-----------------|----------------

  */
  HANDLE inferior = NULL;
  do {
    const DWORD rights = PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_DUP_HANDLE | PROCESS_TERMINATE | PROCESS_CREATE_THREAD | PROCESS_SUSPEND_RESUME;
    inferior = OpenProcess(rights, FALSE, pid);
    if (inferior == NULL) {
      appendMsgCannotInterrupt(pid, tr("Cannot open process: %1") + Utils::winErrorMessage(GetLastError()));
      break;
    }
    const auto creatorIs64Bit = is64BitWindowsBinary(Utils::FilePath::fromUserInput(QCoreApplication::applicationFilePath()));
    if (!is64BitSystem || si == NoSpecialInterrupt || (si == Win64Interrupt && creatorIs64Bit) || (si == Win32Interrupt && !creatorIs64Bit)) {
      if (!DebugBreakProcess(inferior)) {
        appendMsgCannotInterrupt(pid, tr("DebugBreakProcess failed:") + QLatin1Char(' ') + Utils::winErrorMessage(GetLastError()));
      }
    } else if (si == Win32Interrupt || si == Win64Interrupt) {
      auto executable = QCoreApplication::applicationDirPath();
      executable += si == Win32Interrupt ? QLatin1String("/win32interrupt.exe") : QLatin1String("/win64interrupt.exe");
      if (!QFile::exists(executable)) {
        appendMsgCannotInterrupt(pid, tr("%1 does not exist. If you built %2 " "yourself, check out https://code.qt.io/cgit/" "qt-creator/binary-artifacts.git/.").arg(QDir::toNativeSeparators(executable), QString(Core::Constants::IDE_DISPLAY_NAME)));
      }
      switch (QProcess::execute(executable, QStringList(QString::number(pid)))) {
      case -2:
        appendMsgCannotInterrupt(pid, tr("Cannot start %1. Check src\\tools\\win64interrupt\\win64interrupt.c " "for more information.").arg(QDir::toNativeSeparators(executable)));
        break;
      case 0:
        break;
      default:
        appendMsgCannotInterrupt(pid, QDir::toNativeSeparators(executable) + QLatin1Char(' ') + tr("could not break the process."));
        break;
      }
    }
  } while (false);
  if (inferior != NULL)
    CloseHandle(inferior);
  #else
    if (pid <= 0)
        appendMsgCannotInterrupt(pid, tr("Invalid process id."));
    else if (kill(pid, SIGINT))
        appendMsgCannotInterrupt(pid, QString::fromLocal8Bit(strerror(errno)));
  #endif // Q_OS_WIN
}

} // namespace ProjectExplorer
