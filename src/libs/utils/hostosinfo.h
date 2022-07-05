// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "osspecificaspects.h"

#include <QString>

#ifdef Q_OS_WIN
#define QTC_HOST_EXE_SUFFIX QTC_WIN_EXE_SUFFIX
#else
#define QTC_HOST_EXE_SUFFIX ""
#endif // Q_OS_WIN

namespace Utils {

class ORCA_UTILS_EXPORT HostOsInfo {
public:
  static constexpr auto hostOs() -> OsType
  {
    #if defined(Q_OS_WIN)
    return OsTypeWindows;
    #elif defined(Q_OS_LINUX)
        return OsTypeLinux;
    #elif defined(Q_OS_MAC)
        return OsTypeMac;
    #elif defined(Q_OS_UNIX)
        return OsTypeOtherUnix;
    #else
        return OsTypeOther;
    #endif
  }

  enum HostArchitecture {
    HostArchitectureX86,
    HostArchitectureAMD64,
    HostArchitectureItanium,
    HostArchitectureArm,
    HostArchitectureUnknown
  };

  static auto hostArchitecture() -> HostArchitecture;

  static constexpr auto isWindowsHost() -> bool { return hostOs() == OsTypeWindows; }
  static constexpr auto isLinuxHost() -> bool { return hostOs() == OsTypeLinux; }
  static constexpr auto isMacHost() -> bool { return hostOs() == OsTypeMac; }

  static constexpr auto isAnyUnixHost() -> bool
  {
    #ifdef Q_OS_UNIX
        return true;
    #else
    return false;
    #endif
  }

  static auto isRunningUnderRosetta() -> bool;

  static auto withExecutableSuffix(const QString &executable) -> QString
  {
    return OsSpecificAspects::withExecutableSuffix(hostOs(), executable);
  }

  static auto setOverrideFileNameCaseSensitivity(Qt::CaseSensitivity sensitivity) -> void;
  static auto unsetOverrideFileNameCaseSensitivity() -> void;

  static auto fileNameCaseSensitivity() -> Qt::CaseSensitivity
  {
    return m_useOverrideFileNameCaseSensitivity ? m_overrideFileNameCaseSensitivity : OsSpecificAspects::fileNameCaseSensitivity(hostOs());
  }

  static auto pathListSeparator() -> QChar
  {
    return OsSpecificAspects::pathListSeparator(hostOs());
  }

  static auto controlModifier() -> Qt::KeyboardModifier
  {
    return OsSpecificAspects::controlModifier(hostOs());
  }

  static auto canCreateOpenGLContext(QString *errorMessage) -> bool;

private:
  static Qt::CaseSensitivity m_overrideFileNameCaseSensitivity;
  static bool m_useOverrideFileNameCaseSensitivity;
};

} // namespace Utils
