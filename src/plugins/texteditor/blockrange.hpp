// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace TextEditor {

class BlockRange {
public:
  BlockRange() = default;
  BlockRange(int firstPosition, int lastPosition) : firstPosition(firstPosition), lastPosition(lastPosition) {}

  auto isNull() const -> bool
  {
    return lastPosition < firstPosition;
  }

  auto first() const -> int
  {
    return firstPosition;
  }

  auto last() const -> int
  {
    return lastPosition;
  }

private:
  int firstPosition = 0;
  int lastPosition = -1;
};

} // namespace TextEditor
