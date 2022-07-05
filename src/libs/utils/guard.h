// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QtGlobal>

namespace Utils {

class ORCA_UTILS_EXPORT Guard {
  Q_DISABLE_COPY(Guard)

public:
  Guard();
  ~Guard();
  auto isLocked() const -> bool;

private:
  int m_lockCount = 0;
  friend class GuardLocker;
};

class ORCA_UTILS_EXPORT GuardLocker {
  Q_DISABLE_COPY(GuardLocker)

public:
  GuardLocker(Guard &guard);
  ~GuardLocker();

private:
  Guard &m_guard;
};

} // namespace Utils
