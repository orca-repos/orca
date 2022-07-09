// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeproject.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakebuildstep.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectimporter.hpp"
#include "cmakeprojectnodes.hpp"
#include "cmaketool.hpp"

#include <core/icontext.hpp>
#include <projectexplorer/buildconfiguration.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {

using namespace Internal;

/*!
  \class CMakeProject
*/
CMakeProject::CMakeProject(const FilePath &fileName) : Project(Constants::CMAKE_MIMETYPE, fileName)
{
  setId(CMakeProjectManager::Constants::CMAKE_PROJECT_ID);
  setProjectLanguages(Core::Context(ProjectExplorer::Constants::CXX_LANGUAGE_ID));
  setDisplayName(projectDirectory().fileName());
  setCanBuildProducts();
  setHasMakeInstallEquivalent(true);
}

CMakeProject::~CMakeProject()
{
  delete m_projectImporter;
}

auto CMakeProject::projectIssues(const Kit *k) const -> Tasks
{
  auto result = Project::projectIssues(k);

  if (!CMakeKitAspect::cmakeTool(k))
    result.append(createProjectTask(Task::TaskType::Error, tr("No cmake tool set.")));
  if (ToolChainKitAspect::toolChains(k).isEmpty())
    result.append(createProjectTask(Task::TaskType::Warning, tr("No compilers set in kit.")));

  result.append(m_issues);

  return result;
}

auto CMakeProject::projectImporter() const -> ProjectImporter*
{
  if (!m_projectImporter)
    m_projectImporter = new CMakeProjectImporter(projectFilePath());
  return m_projectImporter;
}

auto CMakeProject::addIssue(IssueType type, const QString &text) -> void
{
  m_issues.append(createProjectTask(type, text));
}

auto CMakeProject::clearIssues() -> void
{
  m_issues.clear();
}

auto CMakeProject::setupTarget(Target *t) -> bool
{
  t->updateDefaultBuildConfigurations();
  if (t->buildConfigurations().isEmpty())
    return false;
  t->updateDefaultDeployConfigurations();
  return true;
}

auto CMakeProject::deploymentKnowledge() const -> ProjectExplorer::DeploymentKnowledge
{
  return !files([](const ProjectExplorer::Node *n) {
    return n->filePath().fileName() == "QtCreatorDeployment.txt";
  }).isEmpty()
           ? DeploymentKnowledge::Approximative
           : DeploymentKnowledge::Bad;
}

auto CMakeProject::makeInstallCommand(const Target *target, const QString &installRoot) -> MakeInstallCommand
{
  MakeInstallCommand cmd;
  if (const BuildConfiguration *const bc = target->activeBuildConfiguration()) {
    if (const auto cmakeStep = bc->buildSteps()->firstOfType<CMakeBuildStep>()) {
      if (auto tool = CMakeKitAspect::cmakeTool(target->kit()))
        cmd.command = tool->cmakeExecutable();
    }
  }

  QString installTarget = "install";
  QStringList config;

  auto bs = qobject_cast<CMakeBuildSystem*>(target->buildSystem());
  auto bc = qobject_cast<CMakeBuildConfiguration*>(target->activeBuildConfiguration());
  if (bs && bc) {
    if (bs->usesAllCapsTargets())
      installTarget = "INSTALL";
    if (bs->isMultiConfig())
      config << "--config" << bc->cmakeBuildType();
  }

  FilePath buildDirectory = ".";
  if (bc)
    buildDirectory = bc->buildDirectory();

  cmd.arguments << "--build" << buildDirectory.onDevice(cmd.command).path() << "--target" << installTarget << config;

  cmd.environment.set("DESTDIR", QDir::toNativeSeparators(installRoot));
  return cmd;
}

} // namespace CMakeProjectManager
