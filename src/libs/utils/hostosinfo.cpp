// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "hostosinfo.hpp"

#include <QCoreApplication>

#if !defined(QT_NO_OPENGL) && defined(QT_GUI_LIB)
#include <QOpenGLContext>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#ifdef Q_OS_MACOS
#include <sys/sysctl.hpp>
#endif

using namespace Utils;

Qt::CaseSensitivity HostOsInfo::m_overrideFileNameCaseSensitivity = Qt::CaseSensitive;
bool HostOsInfo::m_useOverrideFileNameCaseSensitivity = false;

#ifdef Q_OS_WIN
static auto hostProcessorArchitecture() -> WORD
{
  SYSTEM_INFO info;
  GetNativeSystemInfo(&info);
  return info.wProcessorArchitecture;
}
#endif

auto HostOsInfo::hostArchitecture() -> HostOsInfo::HostArchitecture
{
  #ifdef Q_OS_WIN
  static const WORD processorArchitecture = hostProcessorArchitecture();
  switch (processorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    return HostOsInfo::HostArchitectureAMD64;
  case PROCESSOR_ARCHITECTURE_INTEL:
    return HostOsInfo::HostArchitectureX86;
  case PROCESSOR_ARCHITECTURE_IA64:
    return HostOsInfo::HostArchitectureItanium;
  case PROCESSOR_ARCHITECTURE_ARM:
    return HostOsInfo::HostArchitectureArm;
  default:
    return HostOsInfo::HostArchitectureUnknown;
  }
  #else
    return HostOsInfo::HostArchitectureUnknown;
  #endif
}

auto HostOsInfo::isRunningUnderRosetta() -> bool
{
  #ifdef Q_OS_MACOS
    int translated = 0;
    auto size = sizeof(translated);
    if (sysctlbyname("sysctl.proc_translated", &translated, &size, nullptr, 0) == 0)
        return translated;
  #endif
  return false;
}

auto HostOsInfo::setOverrideFileNameCaseSensitivity(Qt::CaseSensitivity sensitivity) -> void
{
  m_useOverrideFileNameCaseSensitivity = true;
  m_overrideFileNameCaseSensitivity = sensitivity;
}

auto HostOsInfo::unsetOverrideFileNameCaseSensitivity() -> void
{
  m_useOverrideFileNameCaseSensitivity = false;
}

auto HostOsInfo::canCreateOpenGLContext(QString *errorMessage) -> bool
{
  #if defined(QT_NO_OPENGL) || !defined(QT_GUI_LIB)
    Q_UNUSED(errorMessage)
    return false;
  #else
  static const bool canCreate = QOpenGLContext().create();
  if (!canCreate)
    *errorMessage = QCoreApplication::translate("Utils::HostOsInfo", "Cannot create OpenGL context.");
  return canCreate;
  #endif
}
