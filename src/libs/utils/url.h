// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QUrl>

namespace Utils {

ORCA_UTILS_EXPORT auto urlFromLocalHostAndFreePort() -> QUrl;
ORCA_UTILS_EXPORT auto urlFromLocalSocket() -> QUrl;
ORCA_UTILS_EXPORT auto urlSocketScheme() -> QString;
ORCA_UTILS_EXPORT auto urlTcpScheme() -> QString;

}
