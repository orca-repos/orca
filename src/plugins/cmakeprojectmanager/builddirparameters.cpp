// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "builddirparameters.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectplugin.hpp"
#include "cmakespecificsettings.hpp"
#include "cmaketoolmanager.hpp"

#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/toolchain.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace ProjectExplorer;

namespace CMakeProjectManager {
namespace Internal {

BuildDirParameters::BuildDirParameters() = default;

BuildDirParameters::BuildDirParameters(CMakeBuildConfiguration *bc)
{
  QTC_ASSERT(bc, return);

  const Utils::MacroExpander *expander = bc->macroExpander();

  const auto expandedArguments = Utils::transform(bc->initialCMakeArguments(), [expander](const QString &s) {
    return expander->expand(s);
  });
  initialCMakeArguments = Utils::filtered(expandedArguments, [](const QString &s) { return !s.isEmpty(); });
  configurationChangesArguments = Utils::transform(bc->configurationChangesArguments(), [expander](const QString &s) {
    return expander->expand(s);
  });
  additionalCMakeArguments = Utils::transform(bc->additionalCMakeArguments(), [expander](const QString &s) {
    return expander->expand(s);
  });
  const Target *t = bc->target();
  const Kit *k = t->kit();
  const Project *p = t->project();

  projectName = p->displayName();

  sourceDirectory = bc->sourceDirectory();
  if (sourceDirectory.isEmpty())
    sourceDirectory = p->projectDirectory();
  buildDirectory = bc->buildDirectory();

  cmakeBuildType = bc->cmakeBuildType();

  environment = bc->environment();
  // Disable distributed building for configuration runs. CMake does not do those in parallel,
  // so there is no win in sending data over the network.
  // Unfortunately distcc does not have a simple environment flag to turn it off:-/
  if (Utils::HostOsInfo::isAnyUnixHost())
    environment.set("ICECC", "no");

  auto settings = CMakeProjectPlugin::projectTypeSpecificSettings();
  if (!settings->ninjaPath.filePath().isEmpty()) {
    const auto ninja = settings->ninjaPath.filePath();
    environment.appendOrSetPath(ninja.isFile() ? ninja.parentDir() : ninja);
  }

  cmakeToolId = CMakeKitAspect::cmakeToolId(k);
}

auto BuildDirParameters::isValid() const -> bool
{
  return cmakeTool();
}

auto BuildDirParameters::cmakeTool() const -> CMakeTool*
{
  return CMakeToolManager::findById(cmakeToolId);
}

BuildDirParameters::BuildDirParameters(const BuildDirParameters &) = default;
auto BuildDirParameters::operator=(const BuildDirParameters &) -> BuildDirParameters& = default;

} // namespace Internal
} // namespace CMakeProjectManager
