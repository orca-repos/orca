// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-context-interface.hpp"

namespace Orca::Plugin::Core {

class CORE_EXPORT StatusBarManager {
public:
  enum StatusBarPosition {
    First=0,
    Second=1,
    Third=2,
    LastLeftAligned=Third,
    RightCorner
  };

  static auto addStatusBarWidget(QWidget *widget, StatusBarPosition position, const Context &ctx = Context()) -> void;
  static auto destroyStatusBarWidget(QWidget *widget) -> void;
  static auto restoreSettings() -> void;
};

} // namespace Orca::Plugin::Core
