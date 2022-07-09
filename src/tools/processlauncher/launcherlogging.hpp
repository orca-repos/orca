// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtCore/qloggingcategory.h>
#include <QtCore/qstring.h>

namespace Utils {
namespace Internal {

Q_DECLARE_LOGGING_CATEGORY(launcherLog)

template <typename T>
auto logDebug(const T &msg) -> void
{
  qCDebug(launcherLog) << msg;
}

template <typename T>
auto logWarn(const T &msg) -> void
{
  qCWarning(launcherLog) << msg;
}

template <typename T>
auto logError(const T &msg) -> void
{
  qCCritical(launcherLog) << msg;
}

}
}
