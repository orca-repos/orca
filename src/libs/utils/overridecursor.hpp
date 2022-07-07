// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QCursor>

namespace Utils {

class ORCA_UTILS_EXPORT OverrideCursor {
public:
  OverrideCursor(const QCursor &cursor);
  ~OverrideCursor();
  auto set() -> void;
  auto reset() -> void;

private:
  bool m_set = true;
  QCursor m_cursor;
};

}
