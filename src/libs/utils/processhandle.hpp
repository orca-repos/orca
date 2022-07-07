// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QMetaType>

namespace Utils {

class ORCA_UTILS_EXPORT ProcessHandle {
public:
  ProcessHandle();
  explicit ProcessHandle(qint64 pid);

  auto isValid() const -> bool;
  auto setPid(qint64 pid) -> void;
  auto pid() const -> qint64;
  auto equals(const ProcessHandle &) const -> bool;
  auto activate() -> bool;

private:
  qint64 m_pid;
};

inline auto operator==(const ProcessHandle &p1, const ProcessHandle &p2) -> bool { return p1.equals(p2); }
inline auto operator!=(const ProcessHandle &p1, const ProcessHandle &p2) -> bool { return !p1.equals(p2); }

} // Utils

Q_DECLARE_METATYPE(Utils::ProcessHandle)
