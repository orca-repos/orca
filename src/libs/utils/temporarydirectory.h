// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QTemporaryDir>

namespace Utils {

class FilePath;

class ORCA_UTILS_EXPORT TemporaryDirectory : public QTemporaryDir {
public:
  explicit TemporaryDirectory(const QString &pattern);

  static auto masterTemporaryDirectory() -> QTemporaryDir*;
  static auto setMasterTemporaryDirectory(const QString &pattern) -> void;
  static auto masterDirectoryPath() -> QString;
  static auto masterDirectoryFilePath() -> FilePath;
  auto path() const -> FilePath;
  auto filePath(const QString &fileName) const -> FilePath;
};

} // namespace Utils
