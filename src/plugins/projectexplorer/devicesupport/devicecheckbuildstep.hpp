// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../buildstep.hpp"
#include "../projectexplorer_export.hpp"

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT DeviceCheckBuildStep : public BuildStep {
  Q_OBJECT

public:
  DeviceCheckBuildStep(BuildStepList *bsl, Utils::Id id);

  auto init() -> bool override;
  auto doRun() -> void override;
  static auto stepId() -> Utils::Id;
  static auto displayName() -> QString;
};

} // namespace ProjectExplorer
