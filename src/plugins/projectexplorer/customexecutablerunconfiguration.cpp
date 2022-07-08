// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customexecutablerunconfiguration.hpp"

#include "devicesupport/devicemanager.hpp"
#include "localenvironmentaspect.hpp"
#include "target.hpp"

using namespace Utils;

namespace ProjectExplorer {

constexpr char CUSTOM_EXECUTABLE_RUNCONFIG_ID[] = "ProjectExplorer.CustomExecutableRunConfiguration";

// CustomExecutableRunConfiguration

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target) : CustomExecutableRunConfiguration(target, CUSTOM_EXECUTABLE_RUNCONFIG_ID) {}

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target, Id id) : RunConfiguration(target, id)
{
  auto envAspect = addAspect<LocalEnvironmentAspect>(target);

  auto exeAspect = addAspect<ExecutableAspect>();
  exeAspect->setSettingsKey("ProjectExplorer.CustomExecutableRunConfiguration.Executable");
  exeAspect->setDisplayStyle(StringAspect::PathChooserDisplay);
  exeAspect->setHistoryCompleter("Qt.CustomExecutable.History");
  exeAspect->setExpectedKind(PathChooser::ExistingCommand);
  exeAspect->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(envAspect->environment()));

  addAspect<ArgumentsAspect>();
  addAspect<WorkingDirectoryAspect>();
  addAspect<TerminalAspect>();

  connect(envAspect, &EnvironmentAspect::environmentChanged, this, [exeAspect, envAspect] {
    exeAspect->setEnvironmentChange(EnvironmentChange::fromFixedEnvironment(envAspect->environment()));
  });

  setDefaultDisplayName(defaultDisplayName());
}

auto CustomExecutableRunConfiguration::executable() const -> FilePath
{
  return aspect<ExecutableAspect>()->executable();
}

auto CustomExecutableRunConfiguration::isEnabled() const -> bool
{
  return true;
}

auto CustomExecutableRunConfiguration::runnable() const -> Runnable
{
  const auto workingDirectory = aspect<WorkingDirectoryAspect>()->workingDirectory();

  Runnable r;
  r.command = commandLine();
  r.environment = aspect<EnvironmentAspect>()->environment();
  r.workingDirectory = workingDirectory;
  r.device = DeviceManager::defaultDesktopDevice();

  if (!r.command.isEmpty()) {
    const auto expanded = macroExpander()->expand(r.command.executable());
    r.command.setExecutable(r.environment.searchInPath(expanded.toString(), {workingDirectory}));
  }

  return r;
}

auto CustomExecutableRunConfiguration::defaultDisplayName() const -> QString
{
  if (executable().isEmpty())
    return tr("Custom Executable");
  return tr("Run %1").arg(executable().toUserOutput());
}

auto CustomExecutableRunConfiguration::checkForIssues() const -> Tasks
{
  Tasks tasks;
  if (executable().isEmpty()) {
    tasks << createConfigurationIssue(tr("You need to set an executable in the custom run " "configuration."));
  }
  return tasks;
}

// Factories

CustomExecutableRunConfigurationFactory::CustomExecutableRunConfigurationFactory() : FixedRunConfigurationFactory(CustomExecutableRunConfiguration::tr("Custom Executable"))
{
  registerRunConfiguration<CustomExecutableRunConfiguration>(CUSTOM_EXECUTABLE_RUNCONFIG_ID);
}

CustomExecutableRunWorkerFactory::CustomExecutableRunWorkerFactory()
{
  setProduct<SimpleTargetRunner>();
  addSupportedRunMode(Constants::NORMAL_RUN_MODE);
  addSupportedRunConfig(CUSTOM_EXECUTABLE_RUNCONFIG_ID);
}

} // namespace ProjectExplorer
