// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "desktoprunconfiguration.hpp"

#include "buildsystem.hpp"
#include "localenvironmentaspect.hpp"
#include "project.hpp"
#include "runconfigurationaspects.hpp"
#include "target.hpp"


#include <constants/android/androidconstants.hpp>
#include <constants/docker/dockerconstants.hpp>
#include <constants/qbsprojectmanager/qbsprojectmanagerconstants.hpp>

#include <qmakeprojectmanager/qmakeprojectmanagerconstants.hpp>
#include <cmakeprojectmanager/cmakeprojectconstants.hpp>

#include <utils/fileutils.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>


using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class DesktopRunConfiguration : public RunConfiguration {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::DesktopRunConfiguration)

protected:
  enum Kind {
    Qmake,
    Qbs,
    CMake
  }; // FIXME: Remove

  DesktopRunConfiguration(Target *target, Id id, Kind kind);

private:
  auto updateTargetInformation() -> void;
  auto executableToRun(const BuildTargetInfo &targetInfo) const -> FilePath;

  const Kind m_kind;
};

DesktopRunConfiguration::DesktopRunConfiguration(Target *target, Id id, Kind kind) : RunConfiguration(target, id), m_kind(kind)
{
  const auto envAspect = addAspect<LocalEnvironmentAspect>(target);

  addAspect<ExecutableAspect>();
  addAspect<ArgumentsAspect>();
  addAspect<WorkingDirectoryAspect>();
  addAspect<TerminalAspect>();

  auto libAspect = addAspect<UseLibraryPathsAspect>();
  connect(libAspect, &UseLibraryPathsAspect::changed, envAspect, &EnvironmentAspect::environmentChanged);

  if (HostOsInfo::isMacHost()) {
    auto dyldAspect = addAspect<UseDyldSuffixAspect>();
    connect(dyldAspect, &UseLibraryPathsAspect::changed, envAspect, &EnvironmentAspect::environmentChanged);
    envAspect->addModifier([dyldAspect](Environment &env) {
      if (dyldAspect->value())
        env.set(QLatin1String("DYLD_IMAGE_SUFFIX"), QLatin1String("_debug"));
    });
  }

  if (HostOsInfo::isAnyUnixHost())
    addAspect<RunAsRootAspect>();

  envAspect->addModifier([this, libAspect](Environment &env) {
    const auto bti = buildTargetInfo();
    if (bti.runEnvModifier)
      bti.runEnvModifier(env, libAspect->value());
  });

  setUpdater([this] { updateTargetInformation(); });

  connect(target, &Target::buildSystemUpdated, this, &RunConfiguration::update);
}

auto DesktopRunConfiguration::updateTargetInformation() -> void
{
  if (!activeBuildSystem())
    return;

  auto bti = buildTargetInfo();

  auto terminalAspect = aspect<TerminalAspect>();
  terminalAspect->setUseTerminalHint(bti.usesTerminal);

  if (m_kind == Qmake) {

    auto profile = FilePath::fromString(buildKey());
    if (profile.isEmpty())
      setDefaultDisplayName(tr("Qt Run Configuration"));
    else
      setDefaultDisplayName(profile.completeBaseName());

    emit aspect<EnvironmentAspect>()->environmentChanged();

    auto wda = aspect<WorkingDirectoryAspect>();
    wda->setDefaultWorkingDirectory(bti.workingDirectory);

    aspect<ExecutableAspect>()->setExecutable(bti.targetFilePath);

  } else if (m_kind == Qbs) {

    setDefaultDisplayName(bti.displayName);
    const auto executable = executableToRun(bti);

    aspect<ExecutableAspect>()->setExecutable(executable);

    if (!executable.isEmpty()) {
      const auto defaultWorkingDir = executable.absolutePath();
      if (!defaultWorkingDir.isEmpty())
        aspect<WorkingDirectoryAspect>()->setDefaultWorkingDirectory(defaultWorkingDir);
    }

  } else if (m_kind == CMake) {

    aspect<ExecutableAspect>()->setExecutable(bti.targetFilePath);
    aspect<WorkingDirectoryAspect>()->setDefaultWorkingDirectory(bti.workingDirectory);
    emit aspect<LocalEnvironmentAspect>()->environmentChanged();

  }
}

auto DesktopRunConfiguration::executableToRun(const BuildTargetInfo &targetInfo) const -> FilePath
{
  const auto appInBuildDir = targetInfo.targetFilePath;
  const auto deploymentData = target()->deploymentData();
  if (deploymentData.localInstallRoot().isEmpty())
    return appInBuildDir;

  const auto deployedAppFilePath = deploymentData.deployableForLocalFile(appInBuildDir).remoteFilePath();
  if (deployedAppFilePath.isEmpty())
    return appInBuildDir;

  const auto appInLocalInstallDir = deploymentData.localInstallRoot() + deployedAppFilePath;
  return appInLocalInstallDir.exists() ? appInLocalInstallDir : appInBuildDir;
}

// Factory

class DesktopQmakeRunConfiguration final : public DesktopRunConfiguration {
public:
  DesktopQmakeRunConfiguration(Target *target, Id id) : DesktopRunConfiguration(target, id, Qmake) {}
};

class QbsRunConfiguration final : public DesktopRunConfiguration {
public:
  QbsRunConfiguration(Target *target, Id id) : DesktopRunConfiguration(target, id, Qbs) {}
};

class CMakeRunConfiguration final : public DesktopRunConfiguration {
public:
  CMakeRunConfiguration(Target *target, Id id) : DesktopRunConfiguration(target, id, CMake) {}
};

const char QMAKE_RUNCONFIG_ID[] = "Qt4ProjectManager.Qt4RunConfiguration:";
const char QBS_RUNCONFIG_ID[] = "Qbs.RunConfiguration:";
const char CMAKE_RUNCONFIG_ID[] = "CMakeProjectManager.CMakeRunConfiguration.";

CMakeRunConfigurationFactory::CMakeRunConfigurationFactory()
{
  registerRunConfiguration<CMakeRunConfiguration>(CMAKE_RUNCONFIG_ID);
  addSupportedProjectType(CMakeProjectManager::Constants::CMAKE_PROJECT_ID);
  addSupportedTargetDeviceType(Constants::DESKTOP_DEVICE_TYPE);
  addSupportedTargetDeviceType(Docker::Constants::DOCKER_DEVICE_TYPE);
}

QbsRunConfigurationFactory::QbsRunConfigurationFactory()
{
  registerRunConfiguration<QbsRunConfiguration>(QBS_RUNCONFIG_ID);
  addSupportedProjectType(QbsProjectManager::Constants::PROJECT_ID);
  addSupportedTargetDeviceType(Constants::DESKTOP_DEVICE_TYPE);
  addSupportedTargetDeviceType(Docker::Constants::DOCKER_DEVICE_TYPE);
}

DesktopQmakeRunConfigurationFactory::DesktopQmakeRunConfigurationFactory()
{
  registerRunConfiguration<DesktopQmakeRunConfiguration>(QMAKE_RUNCONFIG_ID);
  addSupportedProjectType(QmakeProjectManager::Constants::QMAKEPROJECT_ID);
  addSupportedTargetDeviceType(Constants::DESKTOP_DEVICE_TYPE);
  addSupportedTargetDeviceType(Docker::Constants::DOCKER_DEVICE_TYPE);
}

} // namespace Internal
} // namespace ProjectExplorer
