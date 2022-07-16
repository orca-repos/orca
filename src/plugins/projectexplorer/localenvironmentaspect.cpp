// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "localenvironmentaspect.hpp"

#include "buildconfiguration.hpp"
#include "environmentaspectwidget.hpp"
#include "kit.hpp"
#include "target.hpp"

using namespace Utils;

namespace ProjectExplorer {

LocalEnvironmentAspect::LocalEnvironmentAspect(Target *target, bool includeBuildEnvironment)
{
  setIsLocal(true);
  addSupportedBaseEnvironment(tr("Clean Environment"), {});

  addSupportedBaseEnvironment(tr("System Environment"), [] {
    return Environment::systemEnvironment();
  });

  if (includeBuildEnvironment) {
    addPreferredBaseEnvironment(tr("Build Environment"), [target] {
      Environment env;
      if (const auto bc = target->activeBuildConfiguration()) {
        env = bc->environment();
      } else {
        // Fallback for targets without buildconfigurations:
        env = target->kit()->buildEnvironment();
      }
      return env;
    });

    connect(target, &Target::activeBuildConfigurationChanged, this, &EnvironmentAspect::environmentChanged);
    connect(target, &Target::buildEnvironmentChanged, this, &EnvironmentAspect::environmentChanged);
  }
}

} // namespace ProjectExplorer
