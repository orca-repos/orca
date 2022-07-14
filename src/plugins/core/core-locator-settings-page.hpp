// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-options-page-interface.hpp"

namespace Orca::Plugin::Core {

class LocatorSettingsPage final : public IOptionsPage {
  Q_OBJECT

public:
  LocatorSettingsPage();
};

} // namespace Orca::Plugin::Core
