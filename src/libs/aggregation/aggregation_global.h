// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qglobal.h>

#if defined(AGGREGATION_LIBRARY)
#  define AGGREGATION_EXPORT Q_DECL_EXPORT
#else
#  define AGGREGATION_EXPORT Q_DECL_IMPORT
#endif
