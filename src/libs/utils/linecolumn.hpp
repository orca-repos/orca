// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "optional.hpp"

#include <QMetaType>

namespace Utils {

class ORCA_UTILS_EXPORT LineColumn {
public:
  constexpr LineColumn() = default;
  constexpr LineColumn(int line, int column) : line(line), column(column) {}

  auto isValid() const -> bool
  {
    return line >= 0 && column >= 0;
  }

  friend auto operator==(LineColumn first, LineColumn second) -> bool
  {
    return first.isValid() && first.line == second.line && first.column == second.column;
  }

  friend auto operator!=(LineColumn first, LineColumn second) -> bool
  {
    return !(first == second);
  }

  static auto extractFromFileName(const QString &fileName, int &postfixPos) -> LineColumn;

  int line = -1;
  int column = -1;
};

using OptionalLineColumn = optional<LineColumn>;

} // namespace Utils

Q_DECLARE_METATYPE(Utils::LineColumn)
