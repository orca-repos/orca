// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/rawprojectpart.hpp>

namespace CppEditor {

// FIXME: Remove
class CppProjectUpdaterInterface {
public:
  virtual ~CppProjectUpdaterInterface() = default;

  virtual auto update(const ProjectExplorer::ProjectUpdateInfo &projectUpdateInfo) -> void = 0;
  virtual auto cancel() -> void = 0;
};

} // namespace CppEditor
