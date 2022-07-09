// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/smallstringview.hpp>

#include <QString>

#include <vector>
#include <functional>

namespace CppEditor {

class Usage {
public:
  Usage() = default;
  Usage(Utils::SmallStringView path, int line, int column) : path(QString::fromUtf8(path.data(), int(path.size()))), line(line), column(column) {}

  friend auto operator==(const Usage &first, const Usage &second) -> bool
  {
    return first.line == second.line && first.column == second.column && first.path == second.path;
  }

  friend auto operator<(const Usage &first, const Usage &second) -> bool
  {
    return std::tie(first.path, first.line, first.column) < std::tie(second.path, second.line, second.column);
  }

  QString path;
  int line = 0;
  int column = 0;
};

using Usages = std::vector<Usage>;
using UsagesCallback = std::function<void(const Usages &usages)>;

} // namespace CppEditor
