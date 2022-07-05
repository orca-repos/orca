// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QTemporaryFile>

namespace Utils {

class ORCA_UTILS_EXPORT TemporaryFile : public QTemporaryFile {
public:
  explicit TemporaryFile(const QString &pattern);
};

} // namespace Utils
