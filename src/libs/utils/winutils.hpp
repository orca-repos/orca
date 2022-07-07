// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

namespace Utils {

class FilePath;

// Helper to format a Windows error message, taking the
// code as returned by the GetLastError()-API.
ORCA_UTILS_EXPORT auto winErrorMessage(unsigned long error) -> QString;

// Determine a DLL version
enum WinDLLVersionType {
  WinDLLFileVersion,
  WinDLLProductVersion
};

ORCA_UTILS_EXPORT auto winGetDLLVersion(WinDLLVersionType t, const QString &name, QString *errorMessage) -> QString;
ORCA_UTILS_EXPORT auto is64BitWindowsSystem() -> bool;

// Check for a 64bit binary.
ORCA_UTILS_EXPORT auto is64BitWindowsBinary(const FilePath &binary) -> bool;

// Get the path to the executable for a given PID.
ORCA_UTILS_EXPORT auto imageName(quint32 processId) -> QString;

//
// RAII class to temporarily prevent windows crash messages from popping up using the
// application-global (!) error mode.
//
// Useful primarily for QProcess launching, since the setting will be inherited.
//
class ORCA_UTILS_EXPORT WindowsCrashDialogBlocker {
public:
  WindowsCrashDialogBlocker();
  ~WindowsCrashDialogBlocker();
  #ifdef Q_OS_WIN
private:
  const unsigned int silenceErrorMode;
  const unsigned int originalErrorMode;
  #endif // Q_OS_WIN
};

} // namespace Utils
