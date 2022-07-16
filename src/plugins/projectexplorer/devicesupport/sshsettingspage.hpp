// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-options-page-interface.hpp>

namespace ProjectExplorer {
namespace Internal {

class SshSettingsPage final : public Orca::Plugin::Core::IOptionsPage {
public:
  SshSettingsPage();
};

} // namespace Internal
} // namespace ProjectExplorer
