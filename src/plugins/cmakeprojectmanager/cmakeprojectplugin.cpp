// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprojectplugin.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakebuildstep.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakeeditor.hpp"
#include "cmakekitinformation.hpp"
#include "cmakelocatorfilter.hpp"
#include "cmakeproject.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectmanager.hpp"
#include "cmakeprojectnodes.hpp"
#include "cmakesettingspage.hpp"
#include "cmakespecificsettings.hpp"
#include "cmaketoolmanager.hpp"

#include <core/core-action-container.hpp>
#include <core/core-action-manager.hpp>
#include <core/core-file-icon-provider.hpp>
#include <core/core-interface.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/projectmanager.hpp>
#include <projectexplorer/projecttree.hpp>
#include <texteditor/snippets/snippetprovider.hpp>

#include <utils/parameteraction.hpp>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

class CMakeProjectPluginPrivate {
public:
  CMakeToolManager cmakeToolManager; // have that before the first CMakeKitAspect

  ParameterAction buildTargetContextAction{
    CMakeProjectPlugin::tr("Build"),
    CMakeProjectPlugin::tr("Build \"%1\""),
    ParameterAction::AlwaysEnabled /*handled manually*/
  };

  CMakeSettingsPage settingsPage;
  CMakeSpecificSettingsPage specificSettings{CMakeProjectPlugin::projectTypeSpecificSettings()};

  CMakeManager manager;
  CMakeBuildStepFactory buildStepFactory;
  CMakeBuildConfigurationFactory buildConfigFactory;
  CMakeEditorFactory editorFactor;
  BuildCMakeTargetLocatorFilter buildCMakeTargetLocatorFilter;
  OpenCMakeTargetLocatorFilter openCMakeTargetLocationFilter;

  CMakeKitAspect cmakeKitAspect;
  CMakeGeneratorKitAspect cmakeGeneratorKitAspect;
  CMakeConfigurationKitAspect cmakeConfigurationKitAspect;
};

auto CMakeProjectPlugin::projectTypeSpecificSettings() -> CMakeSpecificSettings*
{
  static CMakeSpecificSettings theSettings;
  return &theSettings;
}

CMakeProjectPlugin::~CMakeProjectPlugin()
{
  delete d;
}

auto CMakeProjectPlugin::initialize(const QStringList & /*arguments*/, QString *errorMessage) -> bool
{
  Q_UNUSED(errorMessage)

  d = new CMakeProjectPluginPrivate;
  projectTypeSpecificSettings()->readSettings(ICore::settings());

  const Context projectContext{CMakeProjectManager::Constants::CMAKE_PROJECT_ID};

  registerIconOverlayForSuffix(Constants::FILE_OVERLAY_CMAKE, "cmake");
  registerIconOverlayForFilename(Constants::FILE_OVERLAY_CMAKE, "CMakeLists.txt");

  TextEditor::SnippetProvider::registerGroup(Constants::CMAKE_SNIPPETS_GROUP_ID, tr("CMake", "SnippetProvider"));
  ProjectManager::registerProjectType<CMakeProject>(Constants::CMAKE_PROJECT_MIMETYPE);

  //register actions
  auto command = ActionManager::registerAction(&d->buildTargetContextAction, Constants::BUILD_TARGET_CONTEXT_MENU, projectContext);
  command->setAttribute(Command::CA_Hide);
  command->setAttribute(Command::CA_UpdateText);
  command->setDescription(d->buildTargetContextAction.text());

  ActionManager::actionContainer(ProjectExplorer::Constants::M_SUBPROJECTCONTEXT)->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);

  // Wire up context menu updates:
  connect(ProjectTree::instance(), &ProjectTree::currentNodeChanged, this, &CMakeProjectPlugin::updateContextActions);

  connect(&d->buildTargetContextAction, &ParameterAction::triggered, this, [] {
    if (auto bs = qobject_cast<CMakeBuildSystem*>(ProjectTree::currentBuildSystem())) {
      auto targetNode = dynamic_cast<const CMakeTargetNode*>(ProjectTree::currentNode());
      bs->buildCMakeTarget(targetNode ? targetNode->displayName() : QString());
    }
  });

  return true;
}

auto CMakeProjectPlugin::extensionsInitialized() -> void
{
  //restore the cmake tools before loading the kits
  CMakeToolManager::restoreCMakeTools();
}

auto CMakeProjectPlugin::updateContextActions(Node *node) -> void
{
  auto targetNode = dynamic_cast<const CMakeTargetNode*>(node);
  const auto targetDisplayName = targetNode ? targetNode->displayName() : QString();

  // Build Target:
  d->buildTargetContextAction.setParameter(targetDisplayName);
  d->buildTargetContextAction.setEnabled(targetNode);
  d->buildTargetContextAction.setVisible(targetNode);
}

} // Internal
} // CMakeProjectManager

