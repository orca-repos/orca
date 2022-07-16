// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentaspect.hpp"

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT LocalEnvironmentAspect : public EnvironmentAspect {
  Q_OBJECT

public:
  explicit LocalEnvironmentAspect(Target *parent, bool includeBuildEnvironment = true);
};

} // namespace ProjectExplorer
