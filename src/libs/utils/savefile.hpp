// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QTemporaryFile>

#include <memory>

namespace Utils {

class ORCA_UTILS_EXPORT SaveFile : public QFile {
  Q_OBJECT

public:
  explicit SaveFile(const QString &filename);
  ~SaveFile() override;

  auto open(OpenMode flags = QIODevice::WriteOnly) -> bool override;
  auto rollback() -> void;
  auto commit() -> bool;
  static auto initializeUmask() -> void;

private:
  const QString m_finalFileName;
  std::unique_ptr<QTemporaryFile> m_tempFile;
  bool m_finalized = true;
};

} // namespace Utils
