// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprojectmanager.hpp"

#include "cmakebuildsystem.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeproject.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectnodes.hpp"
#include "fileapiparser.hpp"

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/icore.hpp>
#include <core/messagemanager.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>

#include <utils/parameteraction.hpp>

#include <QAction>
#include <QFileDialog>
#include <QMessageBox>

using namespace ProjectExplorer;
using namespace CMakeProjectManager::Internal;

CMakeManager::CMakeManager() : m_runCMakeAction(new QAction(QIcon(), tr("Run CMake"), this)), m_clearCMakeCacheAction(new QAction(QIcon(), tr("Clear CMake Configuration"), this)), m_runCMakeActionContextMenu(new QAction(QIcon(), tr("Run CMake"), this)), m_rescanProjectAction(new QAction(QIcon(), tr("Rescan Project"), this))
{
  auto mbuild = Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_BUILDPROJECT);
  auto mproject = Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_PROJECTCONTEXT);
  auto msubproject = Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_SUBPROJECTCONTEXT);
  auto mfile = Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_FILECONTEXT);

  const Core::Context projectContext(CMakeProjectManager::Constants::CMAKE_PROJECT_ID);
  const Core::Context globalContext(Core::Constants::C_GLOBAL);

  auto command = Core::ActionManager::registerAction(m_runCMakeAction, Constants::RUN_CMAKE, globalContext);
  command->setAttribute(Core::Command::CA_Hide);
  mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);
  connect(m_runCMakeAction, &QAction::triggered, [this]() {
    runCMake(SessionManager::startupBuildSystem());
  });

  command = Core::ActionManager::registerAction(m_clearCMakeCacheAction, Constants::CLEAR_CMAKE_CACHE, globalContext);
  command->setAttribute(Core::Command::CA_Hide);
  mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);
  connect(m_clearCMakeCacheAction, &QAction::triggered, [this]() {
    clearCMakeCache(SessionManager::startupBuildSystem());
  });

  command = Core::ActionManager::registerAction(m_runCMakeActionContextMenu, Constants::RUN_CMAKE_CONTEXT_MENU, projectContext);
  command->setAttribute(Core::Command::CA_Hide);
  mproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
  msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
  connect(m_runCMakeActionContextMenu, &QAction::triggered, [this]() {
    runCMake(ProjectTree::currentBuildSystem());
  });

  m_buildFileContextMenu = new QAction(tr("Build"), this);
  command = Core::ActionManager::registerAction(m_buildFileContextMenu, Constants::BUILD_FILE_CONTEXT_MENU, projectContext);
  command->setAttribute(Core::Command::CA_Hide);
  mfile->addAction(command, ProjectExplorer::Constants::G_FILE_OTHER);
  connect(m_buildFileContextMenu, &QAction::triggered, this, &CMakeManager::buildFileContextMenu);

  command = Core::ActionManager::registerAction(m_rescanProjectAction, Constants::RESCAN_PROJECT, globalContext);
  command->setAttribute(Core::Command::CA_Hide);
  mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);
  connect(m_rescanProjectAction, &QAction::triggered, [this]() {
    rescanProject(ProjectTree::currentBuildSystem());
  });

  m_buildFileAction = new Utils::ParameterAction(tr("Build File"), tr("Build File \"%1\""), Utils::ParameterAction::AlwaysEnabled, this);
  command = Core::ActionManager::registerAction(m_buildFileAction, Constants::BUILD_FILE);
  command->setAttribute(Core::Command::CA_Hide);
  command->setAttribute(Core::Command::CA_UpdateText);
  command->setDescription(m_buildFileAction->text());
  command->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+B")));
  mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);
  connect(m_buildFileAction, &QAction::triggered, this, [this] { buildFile(); });

  connect(SessionManager::instance(), &SessionManager::startupProjectChanged, this, [this] {
    updateCmakeActions(ProjectTree::currentNode());
  });
  connect(BuildManager::instance(), &BuildManager::buildStateChanged, this, [this] {
    updateCmakeActions(ProjectTree::currentNode());
  });
  connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, this, &CMakeManager::updateBuildFileAction);
  connect(ProjectTree::instance(), &ProjectTree::currentNodeChanged, this, &CMakeManager::updateCmakeActions);

  updateCmakeActions(ProjectTree::currentNode());
}

auto CMakeManager::updateCmakeActions(Node *node) -> void
{
  auto project = qobject_cast<CMakeProject*>(SessionManager::startupProject());
  const auto visible = project && !BuildManager::isBuilding(project);
  m_runCMakeAction->setVisible(visible);
  m_runCMakeActionContextMenu->setEnabled(visible);
  m_clearCMakeCacheAction->setVisible(visible);
  m_rescanProjectAction->setVisible(visible);
  enableBuildFileMenus(node);
}

auto CMakeManager::clearCMakeCache(BuildSystem *buildSystem) -> void
{
  auto cmakeBuildSystem = dynamic_cast<CMakeBuildSystem*>(buildSystem);
  QTC_ASSERT(cmakeBuildSystem, return);

  cmakeBuildSystem->clearCMakeCache();
}

auto CMakeManager::runCMake(BuildSystem *buildSystem) -> void
{
  auto cmakeBuildSystem = dynamic_cast<CMakeBuildSystem*>(buildSystem);
  QTC_ASSERT(cmakeBuildSystem, return);

  if (ProjectExplorerPlugin::saveModifiedFiles())
    cmakeBuildSystem->runCMake();
}

auto CMakeManager::rescanProject(BuildSystem *buildSystem) -> void
{
  auto cmakeBuildSystem = dynamic_cast<CMakeBuildSystem*>(buildSystem);
  QTC_ASSERT(cmakeBuildSystem, return);

  cmakeBuildSystem->runCMakeAndScanProjectTree(); // by my experience: every rescan run requires cmake run too
}

auto CMakeManager::updateBuildFileAction() -> void
{
  Node *node = nullptr;
  if (auto currentDocument = Core::EditorManager::currentDocument())
    node = ProjectTree::nodeForFile(currentDocument->filePath());
  enableBuildFileMenus(node);
}

auto CMakeManager::enableBuildFileMenus(Node *node) -> void
{
  m_buildFileAction->setVisible(false);
  m_buildFileAction->setEnabled(false);
  m_buildFileAction->setParameter(QString());
  m_buildFileContextMenu->setEnabled(false);

  if (!node)
    return;
  auto project = ProjectTree::projectForNode(node);
  if (!project)
    return;
  auto target = project->activeTarget();
  if (!target)
    return;
  const auto generator = CMakeGeneratorKitAspect::generator(target->kit());
  if (generator != "Ninja" && !generator.contains("Makefiles"))
    return;

  if (const FileNode *fileNode = node->asFileNode()) {
    const auto type = fileNode->fileType();
    const auto visible = qobject_cast<CMakeProject*>(project) && dynamic_cast<CMakeTargetNode*>(node->parentProjectNode()) && (type == FileType::Source || type == FileType::Header);

    const auto enabled = visible && !BuildManager::isBuilding(project);
    m_buildFileAction->setVisible(visible);
    m_buildFileAction->setEnabled(enabled);
    m_buildFileAction->setParameter(node->filePath().fileName());
    m_buildFileContextMenu->setEnabled(enabled);
  }
}

auto CMakeManager::buildFile(Node *node) -> void
{
  if (!node) {
    auto currentDocument = Core::EditorManager::currentDocument();
    if (!currentDocument)
      return;
    const auto file = currentDocument->filePath();
    node = ProjectTree::nodeForFile(file);
  }
  auto fileNode = node ? node->asFileNode() : nullptr;
  if (!fileNode)
    return;
  auto project = ProjectTree::projectForNode(fileNode);
  if (!project)
    return;
  auto targetNode = dynamic_cast<CMakeTargetNode*>(fileNode->parentProjectNode());
  if (!targetNode)
    return;
  auto target = project->activeTarget();
  QTC_ASSERT(target, return);
  const auto generator = CMakeGeneratorKitAspect::generator(target->kit());
  const auto relativeSource = fileNode->filePath().relativeChildPath(targetNode->filePath()).toString();
  const auto objExtension = Utils::HostOsInfo::isWindowsHost() ? QString(".obj") : QString(".o");
  Utils::FilePath targetBase;
  auto bc = target->activeBuildConfiguration();
  QTC_ASSERT(bc, return);
  if (generator == "Ninja") {
    const auto relativeBuildDir = targetNode->buildDirectory().relativeChildPath(bc->buildDirectory());
    targetBase = relativeBuildDir / "CMakeFiles" / (targetNode->displayName() + ".dir");
  } else if (!generator.contains("Makefiles")) {
    Core::MessageManager::writeFlashing(tr("Build File is not supported for generator \"%1\"").arg(generator));
    return;
  }

  static_cast<CMakeBuildSystem*>(bc->buildSystem())->buildCMakeTarget(targetBase.pathAppended(relativeSource).toString() + objExtension);
}

auto CMakeManager::buildFileContextMenu() -> void
{
  if (auto node = ProjectTree::currentNode())
    buildFile(node);
}
