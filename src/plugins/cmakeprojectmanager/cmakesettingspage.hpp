// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-options-page-interface.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeSettingsPage final : public Orca::Plugin::Core::IOptionsPage {
public:
  CMakeSettingsPage();
};

} // namespace Internal
} // namespace CMakeProjectManager
