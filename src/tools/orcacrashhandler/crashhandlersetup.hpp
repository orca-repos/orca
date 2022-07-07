// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>

class CrashHandlerSetup
{
public:
    enum RestartCapability { EnableRestart, DisableRestart };

    CrashHandlerSetup(const QString &appName,
                      RestartCapability restartCap = EnableRestart,
                      const QString &executableDirPath = QString());
    ~CrashHandlerSetup();
};
