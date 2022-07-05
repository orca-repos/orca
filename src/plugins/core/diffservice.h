// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <QObject>

namespace Core {

class CORE_EXPORT DiffService {
public:
  DiffService();
  virtual ~DiffService();

  static auto instance()->DiffService*;
  virtual auto diffFiles(const QString &left_file_name, const QString &right_file_name) -> void = 0;
  virtual auto diffModifiedFiles(const QStringList &file_names) -> void = 0;
};

} // namespace Core

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(Core::DiffService, "Core::DiffService")
QT_END_NAMESPACE
