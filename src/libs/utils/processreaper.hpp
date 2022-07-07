// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "singleton.hpp"

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT ProcessReaper final : public SingletonWithOptionalDependencies<ProcessReaper> {
public:
  static auto reap(QProcess *process, int timeoutMs = 500) -> void;

private:
  ProcessReaper() = default;
  ~ProcessReaper();
  friend class SingletonWithOptionalDependencies<ProcessReaper>;
};

} // namespace Utils
