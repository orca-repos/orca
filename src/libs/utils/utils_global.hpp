// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qglobal.h>

#if defined(UTILS_LIBRARY)
#  define ORCA_UTILS_EXPORT Q_DECL_EXPORT
#elif  defined(ORCA_UTILS_STATIC_LIB) // Abuse single files for manual tests
#  define ORCA_UTILS_EXPORT
#else
#  define ORCA_UTILS_EXPORT Q_DECL_IMPORT
#endif
