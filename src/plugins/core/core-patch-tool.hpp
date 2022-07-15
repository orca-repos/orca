// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/filepath.hpp>

namespace Orca::Plugin::Core {

class CORE_EXPORT PatchTool {
public:
  static auto patchCommand() -> Utils::FilePath;
  static auto setPatchCommand(const Utils::FilePath &new_command) -> void;
  // Utility to run the 'patch' command
  static auto runPatch(const QByteArray &input, const Utils::FilePath &working_directory = {}, int strip = 0, bool reverse = false) -> bool;
};

} // namespace Orca::Plugin::Core