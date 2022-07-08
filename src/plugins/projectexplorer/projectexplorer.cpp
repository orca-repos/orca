// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectexplorer.hpp"

#include "appoutputpane.hpp"
#include "buildpropertiessettings.hpp"
#include "buildsteplist.hpp"
#include "buildsystem.hpp"
#include "compileoutputwindow.hpp"
#include "configtaskhandler.hpp"
#include "customexecutablerunconfiguration.hpp"
#include "customparserssettingspage.hpp"
#include "customwizard/customwizard.hpp"
#include "deployablefile.hpp"
#include "deployconfiguration.hpp"
#include "desktoprunconfiguration.hpp"
#include "environmentwidget.hpp"
#include "extraabi.hpp"
#include "gcctoolchain.hpp"
#ifdef WITH_JOURNALD
#include "journaldwatcher.hpp"
#endif
#include "allprojectsfilter.hpp"
#include "allprojectsfind.hpp"
#include "appoutputpane.hpp"
#include "buildconfiguration.hpp"
#include "buildmanager.hpp"
#include "buildsettingspropertiespage.hpp"
#include "codestylesettingspropertiespage.hpp"
#include "copytaskhandler.hpp"
#include "currentprojectfilter.hpp"
#include "currentprojectfind.hpp"
#include "customtoolchain.hpp"
#include "customwizard/customwizard.hpp"
#include "dependenciespanel.hpp"
#include "devicesupport/desktopdevice.hpp"
#include "devicesupport/desktopdevicefactory.hpp"
#include "devicesupport/devicemanager.hpp"
#include "devicesupport/devicesettingspage.hpp"
#include "devicesupport/sshsettingspage.hpp"
#include "editorsettingspropertiespage.hpp"
#include "filesinallprojectsfind.hpp"
#include "jsonwizard/jsonwizardfactory.hpp"
#include "jsonwizard/jsonwizardgeneratorfactory.hpp"
#include "jsonwizard/jsonwizardpagefactory_p.hpp"
#include "kitfeatureprovider.hpp"
#include "kitinformation.hpp"
#include "kitmanager.hpp"
#include "kitoptionspage.hpp"
#include "miniprojecttargetselector.hpp"
#include "namedwidget.hpp"
#include "parseissuesdialog.hpp"
#include "processstep.hpp"
#include "project.hpp"
#include "projectexplorericons.hpp"
#include "projectexplorersettings.hpp"
#include "projectexplorersettingspage.hpp"
#include "projectfilewizardextension.hpp"
#include "projectmanager.hpp"
#include "projectnodes.hpp"
#include "projectpanelfactory.hpp"
#include "projecttreewidget.hpp"
#include "projectwindow.hpp"
#include "removetaskhandler.hpp"
#include "runconfigurationaspects.hpp"
#include "runsettingspropertiespage.hpp"
#include "selectablefilesmodel.hpp"
#include "session.hpp"
#include "sessiondialog.hpp"
#include "showineditortaskhandler.hpp"
#include "simpleprojectwizard.hpp"
#include "target.hpp"
#include "targetsettingspanel.hpp"
#include "taskhub.hpp"
#include "toolchainmanager.hpp"
#include "toolchainoptionspage.hpp"
#include "vcsannotatetaskhandler.hpp"

#include "windebuginterface.hpp"
#include "msvctoolchain.hpp"

#include "projecttree.hpp"
#include "projectwelcomepage.hpp"

#include <app/app_version.hpp>
#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreconstants.hpp>
#include <core/diffservice.hpp>
#include <core/documentmanager.hpp>
#include <core/editormanager/documentmodel.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/fileutils.hpp>
#include <core/findplaceholder.hpp>
#include <core/foldernavigationwidget.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>
#include <core/idocumentfactory.hpp>
#include <core/imode.hpp>
#include <core/iversioncontrol.hpp>
#include <core/locator/directoryfilter.hpp>
#include <core/minisplitter.hpp>
#include <core/modemanager.hpp>
#include <core/navigationwidget.hpp>
#include <core/outputpane.hpp>
#include <core/vcsmanager.hpp>
#include <extensionsystem/pluginmanager.hpp>
#include <extensionsystem/pluginspec.hpp>
#include <ssh/sshconnection.h>
#include <ssh/sshsettings.h>
#include <texteditor/findinfiles.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditorconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/parameteraction.hpp>
#include <utils/processhandle.hpp>
#include <utils/proxyaction.hpp>
#include <utils/qtcassert.hpp>
#include <utils/removefiledialog.hpp>
#include <utils/stringutils.hpp>
#include <utils/utilsicons.hpp>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPair>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

/*!
    \namespace ProjectExplorer
    The ProjectExplorer namespace contains the classes to explore projects.
*/

/*!
    \namespace ProjectExplorer::Internal
    The ProjectExplorer::Internal namespace is the internal namespace of the
    ProjectExplorer plugin.
    \internal
*/

/*!
    \class ProjectExplorer::ProjectExplorerPlugin

    \brief The ProjectExplorerPlugin class contains static accessor and utility
    functions to obtain the current project, open projects, and so on.
*/

using namespace Core;
using namespace ProjectExplorer::Internal;
using namespace Utils;

namespace ProjectExplorer {

namespace Constants {
const int  P_MODE_SESSION         = 85;

// Actions
constexpr char LOAD[]                     = "ProjectExplorer.Load";
constexpr char UNLOAD[]                   = "ProjectExplorer.Unload";
constexpr char UNLOADCM[]                 = "ProjectExplorer.UnloadCM";
constexpr char UNLOADOTHERSCM[]           = "ProjectExplorer.UnloadOthersCM";
constexpr char CLEARSESSION[]             = "ProjectExplorer.ClearSession";
constexpr char BUILDALLCONFIGS[]          = "ProjectExplorer.BuildProjectForAllConfigs";
constexpr char BUILDPROJECTONLY[]         = "ProjectExplorer.BuildProjectOnly";
constexpr char BUILDCM[]                  = "ProjectExplorer.BuildCM";
constexpr char BUILDDEPENDCM[]            = "ProjectExplorer.BuildDependenciesCM";
constexpr char BUILDSESSION[]             = "ProjectExplorer.BuildSession";
constexpr char BUILDSESSIONALLCONFIGS[]   = "ProjectExplorer.BuildSessionForAllConfigs";
constexpr char REBUILDPROJECTONLY[]       = "ProjectExplorer.RebuildProjectOnly";
constexpr char REBUILD[]                  = "ProjectExplorer.Rebuild";
constexpr char REBUILDALLCONFIGS[]        = "ProjectExplorer.RebuildProjectForAllConfigs";
constexpr char REBUILDCM[]                = "ProjectExplorer.RebuildCM";
constexpr char REBUILDDEPENDCM[]          = "ProjectExplorer.RebuildDependenciesCM";
constexpr char REBUILDSESSION[]           = "ProjectExplorer.RebuildSession";
constexpr char REBUILDSESSIONALLCONFIGS[] = "ProjectExplorer.RebuildSessionForAllConfigs";
constexpr char DEPLOYPROJECTONLY[]        = "ProjectExplorer.DeployProjectOnly";
constexpr char DEPLOY[]                   = "ProjectExplorer.Deploy";
constexpr char DEPLOYCM[]                 = "ProjectExplorer.DeployCM";
constexpr char DEPLOYSESSION[]            = "ProjectExplorer.DeploySession";
constexpr char CLEANPROJECTONLY[]         = "ProjectExplorer.CleanProjectOnly";
constexpr char CLEAN[]                    = "ProjectExplorer.Clean";
constexpr char CLEANALLCONFIGS[]          = "ProjectExplorer.CleanProjectForAllConfigs";
constexpr char CLEANCM[]                  = "ProjectExplorer.CleanCM";
constexpr char CLEANDEPENDCM[]            = "ProjectExplorer.CleanDependenciesCM";
constexpr char CLEANSESSION[]             = "ProjectExplorer.CleanSession";
constexpr char CLEANSESSIONALLCONFIGS[]   = "ProjectExplorer.CleanSessionForAllConfigs";
constexpr char CANCELBUILD[]              = "ProjectExplorer.CancelBuild";
constexpr char RUN[]                      = "ProjectExplorer.Run";
constexpr char RUNWITHOUTDEPLOY[]         = "ProjectExplorer.RunWithoutDeploy";
constexpr char RUNCONTEXTMENU[]           = "ProjectExplorer.RunContextMenu";
constexpr char ADDEXISTINGFILES[]         = "ProjectExplorer.AddExistingFiles";
constexpr char ADDEXISTINGDIRECTORY[]     = "ProjectExplorer.AddExistingDirectory";
constexpr char ADDNEWSUBPROJECT[]         = "ProjectExplorer.AddNewSubproject";
constexpr char REMOVEPROJECT[]            = "ProjectExplorer.RemoveProject";
constexpr char OPENFILE[]                 = "ProjectExplorer.OpenFile";
constexpr char SEARCHONFILESYSTEM[]       = "ProjectExplorer.SearchOnFileSystem";
constexpr char OPENTERMINALHERE[]         = "ProjectExplorer.OpenTerminalHere";
constexpr char SHOWINFILESYSTEMVIEW[]     = "ProjectExplorer.OpenFileSystemView";
constexpr char DUPLICATEFILE[]            = "ProjectExplorer.DuplicateFile";
constexpr char DELETEFILE[]               = "ProjectExplorer.DeleteFile";
constexpr char DIFFFILE[]                 = "ProjectExplorer.DiffFile";
constexpr char SETSTARTUP[]               = "ProjectExplorer.SetStartup";
constexpr char PROJECTTREE_COLLAPSE_ALL[] = "ProjectExplorer.CollapseAll";
constexpr char PROJECTTREE_EXPAND_ALL[]   = "ProjectExplorer.ExpandAll";
constexpr char SELECTTARGET[]             = "ProjectExplorer.SelectTarget";
constexpr char SELECTTARGETQUICK[]        = "ProjectExplorer.SelectTargetQuick";

// Action priorities
constexpr int P_ACTION_RUN            = 100;
constexpr int P_ACTION_BUILDPROJECT   = 80;

// Menus
constexpr char M_RECENTPROJECTS[]                           = "ProjectExplorer.Menu.Recent";
constexpr char M_UNLOADPROJECTS[]                           = "ProjectExplorer.Menu.Unload";
constexpr char M_SESSION[]                                  = "ProjectExplorer.Menu.Session";
constexpr char RUNMENUCONTEXTMENU[]                         = "Project.RunMenu";
constexpr char FOLDER_OPEN_LOCATIONS_CONTEXT_MENU[]         = "Project.F.OpenLocation.CtxMenu";
constexpr char PROJECT_OPEN_LOCATIONS_CONTEXT_MENU[]        = "Project.P.OpenLocation.CtxMenu";
constexpr char RECENTPROJECTS_FILE_NAMES_KEY[]              = "ProjectExplorer/RecentProjects/FileNames";
constexpr char RECENTPROJECTS_DISPLAY_NAMES_KEY[]           = "ProjectExplorer/RecentProjects/DisplayNames";
constexpr char BUILD_BEFORE_DEPLOY_SETTINGS_KEY[]           = "ProjectExplorer/Settings/BuildBeforeDeploy";
constexpr char DEPLOY_BEFORE_RUN_SETTINGS_KEY[]             = "ProjectExplorer/Settings/DeployBeforeRun";
constexpr char SAVE_BEFORE_BUILD_SETTINGS_KEY[]             = "ProjectExplorer/Settings/SaveBeforeBuild";
constexpr char USE_JOM_SETTINGS_KEY[]                       = "ProjectExplorer/Settings/UseJom";
constexpr char AUTO_RESTORE_SESSION_SETTINGS_KEY[]          = "ProjectExplorer/Settings/AutoRestoreLastSession";
constexpr char ADD_LIBRARY_PATHS_TO_RUN_ENV_SETTINGS_KEY[]  = "ProjectExplorer/Settings/AddLibraryPathsToRunEnv";
constexpr char PROMPT_TO_STOP_RUN_CONTROL_SETTINGS_KEY[]    = "ProjectExplorer/Settings/PromptToStopRunControl";
constexpr char AUTO_CREATE_RUN_CONFIGS_SETTINGS_KEY[]       = "ProjectExplorer/Settings/AutomaticallyCreateRunConfigurations";
constexpr char ENVIRONMENT_ID_SETTINGS_KEY[]                = "ProjectExplorer/Settings/EnvironmentId";
constexpr char STOP_BEFORE_BUILD_SETTINGS_KEY[]             = "ProjectExplorer/Settings/StopBeforeBuild";
constexpr char TERMINAL_MODE_SETTINGS_KEY[]                 = "ProjectExplorer/Settings/TerminalMode";
constexpr char CLOSE_FILES_WITH_PROJECT_SETTINGS_KEY[]      = "ProjectExplorer/Settings/CloseFilesWithProject";
constexpr char CLEAR_ISSUES_ON_REBUILD_SETTINGS_KEY[]       = "ProjectExplorer/Settings/ClearIssuesOnRebuild";
constexpr char ABORT_BUILD_ALL_ON_ERROR_SETTINGS_KEY[]      = "ProjectExplorer/Settings/AbortBuildAllOnError";
constexpr char LOW_BUILD_PRIORITY_SETTINGS_KEY[]            = "ProjectExplorer/Settings/LowBuildPriority";
constexpr char CUSTOM_PARSER_COUNT_KEY[]                    = "ProjectExplorer/Settings/CustomParserCount";
constexpr char CUSTOM_PARSER_PREFIX_KEY[]                   = "ProjectExplorer/Settings/CustomParser";

} // namespace Constants

static auto sysEnv(const Project *) -> optional<Environment>
{
  return Environment::systemEnvironment();
}

static auto buildEnv(const Project *project) -> optional<Environment>
{
  if (!project || !project->activeTarget() || !project->activeTarget()->activeBuildConfiguration())
    return {};
  return project->activeTarget()->activeBuildConfiguration()->environment();
}

static auto runConfigForNode(const Target *target, const ProjectNode *node) -> const RunConfiguration*
{
  if (node && node->productType() == ProductType::App) {
    const auto buildKey = node->buildKey();
    for (const RunConfiguration *const rc : target->runConfigurations()) {
      if (rc->buildKey() == buildKey)
        return rc;
    }
  }
  return target->activeRunConfiguration();
}

static auto hideBuildMenu() -> bool
{
  return ICore::settings()->value(Constants::SETTINGS_MENU_HIDE_BUILD, false).toBool();
}

static auto hideDebugMenu() -> bool
{
  return ICore::settings()->value(Constants::SETTINGS_MENU_HIDE_DEBUG, false).toBool();
}

static auto canOpenTerminalWithRunEnv(const Project *project, const ProjectNode *node) -> bool
{
  if (!project)
    return false;
  const Target *const target = project->activeTarget();
  if (!target)
    return false;
  const auto runConfig = runConfigForNode(target, node);
  if (!runConfig)
    return false;
  auto device = runConfig->runnable().device;
  if (!device)
    device = DeviceKitAspect::device(target->kit());
  return device && device->canOpenTerminal();
}

static auto currentBuildConfiguration() -> BuildConfiguration*
{
  const Project *const project = ProjectTree::currentProject();
  const Target *const target = project ? project->activeTarget() : nullptr;
  return target ? target->activeBuildConfiguration() : nullptr;
}

static auto activeTarget() -> Target*
{
  const Project *const project = SessionManager::startupProject();
  return project ? project->activeTarget() : nullptr;
}

static auto activeBuildConfiguration() -> BuildConfiguration*
{
  const Target *const target = activeTarget();
  return target ? target->activeBuildConfiguration() : nullptr;
}

static auto activeRunConfiguration() -> RunConfiguration*
{
  const Target *const target = activeTarget();
  return target ? target->activeRunConfiguration() : nullptr;
}

static auto isTextFile(const FilePath &filePath) -> bool
{
  return mimeTypeForFile(filePath).inherits(TextEditor::Constants::C_TEXTEDITOR_MIMETYPE_TEXT);
}

class ProjectsMode : public IMode {
public:
  ProjectsMode()
  {
    setContext(Context(Constants::C_PROJECTEXPLORER));
    setDisplayName(QCoreApplication::translate("ProjectExplorer::ProjectsMode", "Projects"));
    setIcon(Icon::modeIcon(Icons::MODE_PROJECT_CLASSIC, Icons::MODE_PROJECT_FLAT, Icons::MODE_PROJECT_FLAT_ACTIVE));
    setPriority(Constants::P_MODE_SESSION);
    setId(Constants::MODE_SESSION);
    setContextHelp("Managing Projects");
  }
};

class ProjectEnvironmentWidget : public NamedWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectEnvironmentWidget)
public:
  explicit ProjectEnvironmentWidget(Project *project) : NamedWidget(tr("Project Environment"))
  {
    const auto vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    const auto envWidget = new EnvironmentWidget(this, EnvironmentWidget::TypeLocal);
    envWidget->setOpenTerminalFunc({});
    envWidget->expand();
    vbox->addWidget(envWidget);
    connect(envWidget, &EnvironmentWidget::userChangesChanged, this, [project, envWidget] {
      project->setAdditionalEnvironment(envWidget->userChanges());
    });
    envWidget->setUserChanges(project->additionalEnvironment());
  }
};

class AllProjectFilesFilter : public DirectoryFilter {
public:
  AllProjectFilesFilter();

protected:
  auto saveState(QJsonObject &object) const -> void override;
  auto restoreState(const QJsonObject &object) -> void override;
};

class ProjectExplorerPluginPrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::ProjectExplorerPlugin)

public:
  ProjectExplorerPluginPrivate();

  using EnvironmentGetter = std::function<optional<Environment>(const Project *project)>;

  auto updateContextMenuActions(Node *currentNode) -> void;
  auto updateLocationSubMenus() -> void;
  auto executeRunConfiguration(RunConfiguration *, Id mode) -> void;
  auto buildSettingsEnabledForSession() -> QPair<bool, QString>;
  auto buildSettingsEnabled(const Project *pro) -> QPair<bool, QString>;
  auto addToRecentProjects(const QString &fileName, const QString &displayName) -> void;
  auto startRunControl(RunControl *runControl) -> void;
  auto showOutputPaneForRunControl(RunControl *runControl) -> void;
  auto updateActions() -> void;
  auto updateContext() -> void;
  auto updateDeployActions() -> void;
  auto updateRunWithoutDeployMenu() -> void;
  auto buildQueueFinished(bool success) -> void;
  auto loadAction() -> void;
  auto handleUnloadProject() -> void;
  auto unloadProjectContextMenu() -> void;
  auto unloadOtherProjectsContextMenu() -> void;
  auto closeAllProjects() -> void;
  auto showSessionManager() -> void;
  auto updateSessionMenu() -> void;
  auto setSession(QAction *action) -> void;
  auto determineSessionToRestoreAtStartup() -> void;
  auto restoreSession() -> void;
  auto runProjectContextMenu() -> void;
  auto savePersistentSettings() -> void;
  auto addNewFile() -> void;
  auto handleAddExistingFiles() -> void;
  auto addExistingDirectory() -> void;
  auto addNewSubproject() -> void;
  auto addExistingProjects() -> void;
  auto removeProject() -> void;
  auto openFile() -> void;
  auto searchOnFileSystem() -> void;
  auto showInGraphicalShell() -> void;
  auto showInFileSystemPane() -> void;
  auto removeFile() -> void;
  auto duplicateFile() -> void;
  auto deleteFile() -> void;
  auto handleRenameFile() -> void;
  auto handleSetStartupProject() -> void;
  auto setStartupProject(Project *project) -> void;
  auto closeAllFilesInProject(const Project *project) -> bool;
  auto updateRecentProjectMenu() -> void;
  auto clearRecentProjects() -> void;
  auto openRecentProject(const QString &fileName) -> void;
  auto removeFromRecentProjects(const QString &fileName, const QString &displayName) -> void;
  auto updateUnloadProjectMenu() -> void;
  auto openTerminalHere(const EnvironmentGetter &env) -> void;
  auto openTerminalHereWithRunEnv() -> void;
  auto invalidateProject(Project *project) -> void;
  auto projectAdded(Project *pro) -> void;
  auto projectRemoved(Project *pro) -> void;
  auto projectDisplayNameChanged(Project *pro) -> void;
  auto doUpdateRunActions() -> void;
  auto currentModeChanged(Id mode, Id oldMode) -> void;
  auto updateWelcomePage() -> void;
  auto checkForShutdown() -> void;
  auto timerEvent(QTimerEvent *) -> void override;
  auto recentProjects() const -> QList<QPair<QString, QString>>;
  auto extendFolderNavigationWidgetFactory() -> void;

  QMenu *m_sessionMenu;
  QMenu *m_openWithMenu;
  QMenu *m_openTerminalMenu;
  QMultiMap<int, QObject*> m_actionMap;
  QAction *m_sessionManagerAction;
  QAction *m_newAction;
  QAction *m_loadAction;
  ParameterAction *m_unloadAction;
  ParameterAction *m_unloadActionContextMenu;
  ParameterAction *m_unloadOthersActionContextMenu;
  QAction *m_closeAllProjects;
  QAction *m_buildProjectOnlyAction;
  ParameterAction *m_buildProjectForAllConfigsAction;
  ParameterAction *m_buildAction;
  ParameterAction *m_buildForRunConfigAction;
  ProxyAction *m_modeBarBuildAction;
  QAction *m_buildActionContextMenu;
  QAction *m_buildDependenciesActionContextMenu;
  QAction *m_buildSessionAction;
  QAction *m_buildSessionForAllConfigsAction;
  QAction *m_rebuildProjectOnlyAction;
  QAction *m_rebuildAction;
  QAction *m_rebuildProjectForAllConfigsAction;
  QAction *m_rebuildActionContextMenu;
  QAction *m_rebuildDependenciesActionContextMenu;
  QAction *m_rebuildSessionAction;
  QAction *m_rebuildSessionForAllConfigsAction;
  QAction *m_cleanProjectOnlyAction;
  QAction *m_deployProjectOnlyAction;
  QAction *m_deployAction;
  QAction *m_deployActionContextMenu;
  QAction *m_deploySessionAction;
  QAction *m_cleanAction;
  QAction *m_cleanProjectForAllConfigsAction;
  QAction *m_cleanActionContextMenu;
  QAction *m_cleanDependenciesActionContextMenu;
  QAction *m_cleanSessionAction;
  QAction *m_cleanSessionForAllConfigsAction;
  QAction *m_runAction;
  QAction *m_runActionContextMenu;
  QAction *m_runWithoutDeployAction;
  QAction *m_cancelBuildAction;
  QAction *m_addNewFileAction;
  QAction *m_addExistingFilesAction;
  QAction *m_addExistingDirectoryAction;
  QAction *m_addNewSubprojectAction;
  QAction *m_addExistingProjectsAction;
  QAction *m_removeFileAction;
  QAction *m_duplicateFileAction;
  QAction *m_removeProjectAction;
  QAction *m_deleteFileAction;
  QAction *m_renameFileAction;
  QAction *m_filePropertiesAction = nullptr;
  QAction *m_diffFileAction;
  QAction *m_openFileAction;
  QAction *m_projectTreeCollapseAllAction;
  QAction *m_projectTreeExpandAllAction;
  QAction *m_projectTreeExpandNodeAction = nullptr;
  ParameterAction *m_closeProjectFilesActionFileMenu;
  ParameterAction *m_closeProjectFilesActionContextMenu;
  QAction *m_searchOnFileSystem;
  QAction *m_showInGraphicalShell;
  QAction *m_showFileSystemPane;
  QAction *m_openTerminalHere;
  QAction *m_openTerminalHereBuildEnv;
  QAction *m_openTerminalHereRunEnv;
  ParameterAction *m_setStartupProjectAction;
  QAction *m_projectSelectorAction;
  QAction *m_projectSelectorActionMenu;
  QAction *m_projectSelectorActionQuick;
  QAction *m_runSubProject;
  ProjectWindow *m_proWindow = nullptr;
  QString m_sessionToRestoreAtStartup;
  QStringList m_profileMimeTypes;
  int m_activeRunControlCount = 0;
  int m_shutdownWatchDogId = -1;
  QHash<QString, std::function<Project *(const FilePath &)>> m_projectCreators;
  QList<QPair<QString, QString>> m_recentProjects; // pair of filename, displayname
  static const int m_maxRecentProjects = 25;
  QString m_lastOpenDirectory;
  QPointer<RunConfiguration> m_delayedRunConfiguration;
  QString m_projectFilterString;
  MiniProjectTargetSelector *m_targetSelector;
  ProjectExplorerSettings m_projectExplorerSettings;
  BuildPropertiesSettings m_buildPropertiesSettings;
  QList<CustomParserSettings> m_customParsers;
  bool m_shouldHaveRunConfiguration = false;
  bool m_shuttingDown = false;
  Id m_runMode = Constants::NO_RUN_MODE;
  ToolChainManager *m_toolChainManager = nullptr;
  QStringList m_arguments;
  #ifdef WITH_JOURNALD
    JournaldWatcher m_journalWatcher;
  #endif
  QThreadPool m_threadPool;
  DeviceManager m_deviceManager{true};
  #ifdef Q_OS_WIN
  WinDebugInterface m_winDebugInterface;
  MsvcToolChainFactory m_mscvToolChainFactory;
  ClangClToolChainFactory m_clangClToolChainFactory;
  #else
    LinuxIccToolChainFactory m_linuxToolChainFactory;
  #endif
  #ifndef Q_OS_MACOS
  MingwToolChainFactory m_mingwToolChainFactory; // Mingw offers cross-compiling to windows
  #endif
  GccToolChainFactory m_gccToolChainFactory;
  ClangToolChainFactory m_clangToolChainFactory;
  CustomToolChainFactory m_customToolChainFactory;
  DesktopDeviceFactory m_desktopDeviceFactory;
  ToolChainOptionsPage m_toolChainOptionsPage;
  KitOptionsPage m_kitOptionsPage;
  TaskHub m_taskHub;
  ProjectWelcomePage m_welcomePage;
  CustomWizardMetaFactory<CustomProjectWizard> m_customProjectWizard{IWizardFactory::ProjectWizard};
  CustomWizardMetaFactory<CustomWizard> m_fileWizard{IWizardFactory::FileWizard};
  ProjectsMode m_projectsMode;
  CopyTaskHandler m_copyTaskHandler;
  ShowInEditorTaskHandler m_showInEditorTaskHandler;
  VcsAnnotateTaskHandler m_vcsAnnotateTaskHandler;
  RemoveTaskHandler m_removeTaskHandler;
  ConfigTaskHandler m_configTaskHandler{Task::compilerMissingTask(), Constants::KITS_SETTINGS_PAGE_ID};
  SessionManager m_sessionManager;
  AppOutputPane m_outputPane;
  ProjectTree m_projectTree;
  AllProjectsFilter m_allProjectsFilter;
  CurrentProjectFilter m_currentProjectFilter;
  AllProjectFilesFilter m_allProjectDirectoriesFilter;
  ProcessStepFactory m_processStepFactory;
  AllProjectsFind m_allProjectsFind;
  CurrentProjectFind m_curretProjectFind;
  FilesInAllProjectsFind m_filesInAllProjectsFind;
  CustomExecutableRunConfigurationFactory m_customExecutableRunConfigFactory;
  CustomExecutableRunWorkerFactory m_customExecutableRunWorkerFactory;
  ProjectFileWizardExtension m_projectFileWizardExtension;

  // Settings pages
  ProjectExplorerSettingsPage m_projectExplorerSettingsPage;
  BuildPropertiesSettingsPage m_buildPropertiesSettingsPage{&m_buildPropertiesSettings};
  AppOutputSettingsPage m_appOutputSettingsPage;
  CompileOutputSettingsPage m_compileOutputSettingsPage;
  DeviceSettingsPage m_deviceSettingsPage;
  SshSettingsPage m_sshSettingsPage;
  CustomParsersSettingsPage m_customParsersSettingsPage;
  ProjectTreeWidgetFactory m_projectTreeFactory;
  DefaultDeployConfigurationFactory m_defaultDeployConfigFactory;
  IDocumentFactory m_documentFactory;
  DeviceTypeKitAspect deviceTypeKitAspect;
  DeviceKitAspect deviceKitAspect;
  BuildDeviceKitAspect buildDeviceKitAspect;
  ToolChainKitAspect toolChainKitAspect;
  SysRootKitAspect sysRootKitAspect;
  EnvironmentKitAspect environmentKitAspect;
  DesktopQmakeRunConfigurationFactory qmakeRunConfigFactory;
  QbsRunConfigurationFactory qbsRunConfigFactory;
  CMakeRunConfigurationFactory cmakeRunConfigFactory;
  RunWorkerFactory desktopRunWorkerFactory{RunWorkerFactory::make<SimpleTargetRunner>(), {Constants::NORMAL_RUN_MODE}, {qmakeRunConfigFactory.runConfigurationId(), qbsRunConfigFactory.runConfigurationId(), cmakeRunConfigFactory.runConfigurationId()}};
};

static ProjectExplorerPlugin *m_instance = nullptr;
static ProjectExplorerPluginPrivate *dd = nullptr;

static auto projectFilesInDirectory(const FilePath &path) -> FilePaths
{
  return path.dirEntries({ProjectExplorerPlugin::projectFileGlobs(), QDir::Files});
}

static auto projectsInDirectory(const FilePath &filePath) -> FilePaths
{
  if (!filePath.isReadableDir())
    return {};
  return projectFilesInDirectory(filePath);
}

static auto openProjectsInDirectory(const FilePath &filePath) -> void
{
  const auto projectFiles = projectsInDirectory(filePath);
  if (!projectFiles.isEmpty())
    ICore::openFiles(projectFiles);
}

static auto projectNames(const QVector<FolderNode*> &folders) -> QStringList
{
  const auto names = Utils::transform<QList>(folders, [](FolderNode *n) {
    return n->managingProject()->filePath().fileName();
  });
  return filteredUnique(names);
}

static auto renamableFolderNodes(const FilePath &before, const FilePath &after) -> QVector<FolderNode*>
{
  QVector<FolderNode*> folderNodes;
  ProjectTree::forEachNode([&](Node *node) {
    if (node->asFileNode() && node->filePath() == before && node->parentFolderNode() && node->parentFolderNode()->canRenameFile(before, after)) {
      folderNodes.append(node->parentFolderNode());
    }
  });
  return folderNodes;
}

static auto removableFolderNodes(const FilePath &filePath) -> QVector<FolderNode*>
{
  QVector<FolderNode*> folderNodes;
  ProjectTree::forEachNode([&](Node *node) {
    if (node->asFileNode() && node->filePath() == filePath && node->parentFolderNode() && node->parentFolderNode()->supportsAction(RemoveFile, node)) {
      folderNodes.append(node->parentFolderNode());
    }
  });
  return folderNodes;
}

ProjectExplorerPlugin::ProjectExplorerPlugin()
{
  m_instance = this;
}

ProjectExplorerPlugin::~ProjectExplorerPlugin()
{
  delete dd->m_proWindow; // Needs access to the kit manager.
  JsonWizardFactory::destroyAllFactories();

  // Force sequence of deletion:
  KitManager::destroy(); // remove all the profile information
  delete dd->m_toolChainManager;
  ProjectPanelFactory::destroyFactories();
  delete dd;
  dd = nullptr;
  m_instance = nullptr;

  #ifdef WITH_TESTS
    deleteTestToolchains();
  #endif
}

auto ProjectExplorerPlugin::instance() -> ProjectExplorerPlugin*
{
  return m_instance;
}

auto ProjectExplorerPlugin::initialize(const QStringList &arguments, QString *error) -> bool
{
  Q_UNUSED(error)

  dd = new ProjectExplorerPluginPrivate;

  dd->extendFolderNavigationWidgetFactory();

  qRegisterMetaType<BuildSystem*>();
  qRegisterMetaType<RunControl*>();
  qRegisterMetaType<DeployableFile>("ProjectExplorer::DeployableFile");

  handleCommandLineArguments(arguments);

  dd->m_toolChainManager = new ToolChainManager;

  // Register languages
  ToolChainManager::registerLanguage(Constants::C_LANGUAGE_ID, tr("C"));
  ToolChainManager::registerLanguage(Constants::CXX_LANGUAGE_ID, tr("C++"));

  IWizardFactory::registerFeatureProvider(new KitFeatureProvider);

  IWizardFactory::registerFactoryCreator([]() -> QList<IWizardFactory*> {
    QList<IWizardFactory*> result;
    result << CustomWizard::createWizards();
    result << JsonWizardFactory::createWizardFactories();
    result << new SimpleProjectWizard;
    return result;
  });

  connect(&dd->m_welcomePage, &ProjectWelcomePage::manageSessions, dd, &ProjectExplorerPluginPrivate::showSessionManager);

  auto sessionManager = &dd->m_sessionManager;
  connect(sessionManager, &SessionManager::projectAdded, this, &ProjectExplorerPlugin::fileListChanged);
  connect(sessionManager, &SessionManager::aboutToRemoveProject, dd, &ProjectExplorerPluginPrivate::invalidateProject);
  connect(sessionManager, &SessionManager::projectRemoved, this, &ProjectExplorerPlugin::fileListChanged);
  connect(sessionManager, &SessionManager::projectAdded, dd, &ProjectExplorerPluginPrivate::projectAdded);
  connect(sessionManager, &SessionManager::projectRemoved, dd, &ProjectExplorerPluginPrivate::projectRemoved);
  connect(sessionManager, &SessionManager::projectDisplayNameChanged, dd, &ProjectExplorerPluginPrivate::projectDisplayNameChanged);
  connect(sessionManager, &SessionManager::dependencyChanged, dd, &ProjectExplorerPluginPrivate::updateActions);
  connect(sessionManager, &SessionManager::sessionLoaded, dd, &ProjectExplorerPluginPrivate::updateActions);
  connect(sessionManager, &SessionManager::sessionLoaded, dd, &ProjectExplorerPluginPrivate::updateWelcomePage);

  connect(sessionManager, &SessionManager::projectAdded, dd, [](Project *project) {
    dd->m_allProjectDirectoriesFilter.addDirectory(project->projectDirectory().toString());
  });
  connect(sessionManager, &SessionManager::projectRemoved, dd, [](Project *project) {
    dd->m_allProjectDirectoriesFilter.removeDirectory(project->projectDirectory().toString());
  });

  auto tree = &dd->m_projectTree;
  connect(tree, &ProjectTree::currentProjectChanged, dd, [] {
    dd->updateContextMenuActions(ProjectTree::currentNode());
  });
  connect(tree, &ProjectTree::nodeActionsChanged, dd, [] {
    dd->updateContextMenuActions(ProjectTree::currentNode());
  });
  connect(tree, &ProjectTree::currentNodeChanged, dd, &ProjectExplorerPluginPrivate::updateContextMenuActions);
  connect(tree, &ProjectTree::currentProjectChanged, dd, &ProjectExplorerPluginPrivate::updateActions);
  connect(tree, &ProjectTree::currentProjectChanged, this, [](Project *project) {
    TextEditor::FindInFiles::instance()->setBaseDirectory(project ? project->projectDirectory() : FilePath());
  });

  // For JsonWizard:
  JsonWizardFactory::registerPageFactory(new FieldPageFactory);
  JsonWizardFactory::registerPageFactory(new FilePageFactory);
  JsonWizardFactory::registerPageFactory(new KitsPageFactory);
  JsonWizardFactory::registerPageFactory(new ProjectPageFactory);
  JsonWizardFactory::registerPageFactory(new SummaryPageFactory);

  JsonWizardFactory::registerGeneratorFactory(new FileGeneratorFactory);
  JsonWizardFactory::registerGeneratorFactory(new ScannerGeneratorFactory);

  dd->m_proWindow = new ProjectWindow;

  Context projectTreeContext(Constants::C_PROJECT_TREE);

  auto splitter = new MiniSplitter(Qt::Vertical);
  splitter->addWidget(dd->m_proWindow);
  splitter->addWidget(new OutputPanePlaceHolder(Constants::MODE_SESSION, splitter));
  dd->m_projectsMode.setWidget(splitter);
  dd->m_projectsMode.setEnabled(false);

  ICore::addPreCloseListener([]() -> bool { return coreAboutToClose(); });

  connect(SessionManager::instance(), &SessionManager::projectRemoved, &dd->m_outputPane, &AppOutputPane::projectRemoved);

  // ProjectPanelFactories
  auto panelFactory = new ProjectPanelFactory;
  panelFactory->setPriority(30);
  panelFactory->setDisplayName(QCoreApplication::translate("EditorSettingsPanelFactory", "Editor"));
  panelFactory->setCreateWidgetFunction([](Project *project) { return new EditorSettingsWidget(project); });
  ProjectPanelFactory::registerFactory(panelFactory);

  panelFactory = new ProjectPanelFactory;
  panelFactory->setPriority(40);
  panelFactory->setDisplayName(QCoreApplication::translate("CodeStyleSettingsPanelFactory", "Code Style"));
  panelFactory->setCreateWidgetFunction([](Project *project) { return new CodeStyleSettingsWidget(project); });
  ProjectPanelFactory::registerFactory(panelFactory);

  panelFactory = new ProjectPanelFactory;
  panelFactory->setPriority(50);
  panelFactory->setDisplayName(QCoreApplication::translate("DependenciesPanelFactory", "Dependencies"));
  panelFactory->setCreateWidgetFunction([](Project *project) { return new DependenciesWidget(project); });
  ProjectPanelFactory::registerFactory(panelFactory);

  panelFactory = new ProjectPanelFactory;
  panelFactory->setPriority(60);
  panelFactory->setDisplayName(QCoreApplication::translate("EnvironmentPanelFactory", "Environment"));
  panelFactory->setCreateWidgetFunction([](Project *project) {
    return new ProjectEnvironmentWidget(project);
  });
  ProjectPanelFactory::registerFactory(panelFactory);

  RunConfiguration::registerAspect<CustomParsersAspect>();

  // context menus
  auto msessionContextMenu = ActionManager::createMenu(Constants::M_SESSIONCONTEXT);
  auto mprojectContextMenu = ActionManager::createMenu(Constants::M_PROJECTCONTEXT);
  auto msubProjectContextMenu = ActionManager::createMenu(Constants::M_SUBPROJECTCONTEXT);
  auto mfolderContextMenu = ActionManager::createMenu(Constants::M_FOLDERCONTEXT);
  auto mfileContextMenu = ActionManager::createMenu(Constants::M_FILECONTEXT);

  auto mfile = ActionManager::actionContainer(Core::Constants::M_FILE);
  auto menubar = ActionManager::actionContainer(Core::Constants::MENU_BAR);

  // context menu sub menus:
  auto folderOpenLocationCtxMenu = ActionManager::createMenu(Constants::FOLDER_OPEN_LOCATIONS_CONTEXT_MENU);
  folderOpenLocationCtxMenu->menu()->setTitle(tr("Open..."));
  folderOpenLocationCtxMenu->setOnAllDisabledBehavior(ActionContainer::Hide);

  auto projectOpenLocationCtxMenu = ActionManager::createMenu(Constants::PROJECT_OPEN_LOCATIONS_CONTEXT_MENU);
  projectOpenLocationCtxMenu->menu()->setTitle(tr("Open..."));
  projectOpenLocationCtxMenu->setOnAllDisabledBehavior(ActionContainer::Hide);

  // build menu
  auto mbuild = ActionManager::createMenu(Constants::M_BUILDPROJECT);

  mbuild->menu()->setTitle(tr("&Build"));
  if (!hideBuildMenu())
    menubar->addMenu(mbuild, Core::Constants::G_VIEW);

  // debug menu
  auto mdebug = ActionManager::createMenu(Constants::M_DEBUG);
  mdebug->menu()->setTitle(tr("&Debug"));
  if (!hideDebugMenu())
    menubar->addMenu(mdebug, Core::Constants::G_VIEW);

  auto mstartdebugging = ActionManager::createMenu(Constants::M_DEBUG_STARTDEBUGGING);
  mstartdebugging->menu()->setTitle(tr("&Start Debugging"));
  mdebug->addMenu(mstartdebugging, Core::Constants::G_DEFAULT_ONE);

  //
  // Groups
  //

  mbuild->appendGroup(Constants::G_BUILD_ALLPROJECTS);
  mbuild->appendGroup(Constants::G_BUILD_PROJECT);
  mbuild->appendGroup(Constants::G_BUILD_PRODUCT);
  mbuild->appendGroup(Constants::G_BUILD_SUBPROJECT);
  mbuild->appendGroup(Constants::G_BUILD_FILE);
  mbuild->appendGroup(Constants::G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS);
  mbuild->appendGroup(Constants::G_BUILD_PROJECT_ALLCONFIGURATIONS);
  mbuild->appendGroup(Constants::G_BUILD_CANCEL);
  mbuild->appendGroup(Constants::G_BUILD_BUILD);
  mbuild->appendGroup(Constants::G_BUILD_RUN);

  msessionContextMenu->appendGroup(Constants::G_SESSION_BUILD);
  msessionContextMenu->appendGroup(Constants::G_SESSION_REBUILD);
  msessionContextMenu->appendGroup(Constants::G_SESSION_FILES);
  msessionContextMenu->appendGroup(Constants::G_SESSION_OTHER);
  msessionContextMenu->appendGroup(Constants::G_PROJECT_TREE);

  mprojectContextMenu->appendGroup(Constants::G_PROJECT_FIRST);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_BUILD);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_RUN);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_REBUILD);
  mprojectContextMenu->appendGroup(Constants::G_FOLDER_LOCATIONS);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_FILES);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_LAST);
  mprojectContextMenu->appendGroup(Constants::G_PROJECT_TREE);

  mprojectContextMenu->addMenu(projectOpenLocationCtxMenu, Constants::G_FOLDER_LOCATIONS);
  connect(mprojectContextMenu->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateLocationSubMenus);

  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_FIRST);
  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_BUILD);
  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_RUN);
  msubProjectContextMenu->appendGroup(Constants::G_FOLDER_LOCATIONS);
  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_FILES);
  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_LAST);
  msubProjectContextMenu->appendGroup(Constants::G_PROJECT_TREE);

  msubProjectContextMenu->addMenu(projectOpenLocationCtxMenu, Constants::G_FOLDER_LOCATIONS);
  connect(msubProjectContextMenu->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateLocationSubMenus);

  auto runMenu = ActionManager::createMenu(Constants::RUNMENUCONTEXTMENU);
  runMenu->setOnAllDisabledBehavior(ActionContainer::Hide);
  const auto runSideBarIcon = Icon::sideBarIcon(Icons::RUN, Icons::RUN_FLAT);
  const auto runIcon = Icon::combinedIcon({Utils::Icons::RUN_SMALL.icon(), runSideBarIcon});

  runMenu->menu()->setIcon(runIcon);
  runMenu->menu()->setTitle(tr("Run"));
  msubProjectContextMenu->addMenu(runMenu, Constants::G_PROJECT_RUN);

  mfolderContextMenu->appendGroup(Constants::G_FOLDER_LOCATIONS);
  mfolderContextMenu->appendGroup(Constants::G_FOLDER_FILES);
  mfolderContextMenu->appendGroup(Constants::G_FOLDER_OTHER);
  mfolderContextMenu->appendGroup(Constants::G_FOLDER_CONFIG);
  mfolderContextMenu->appendGroup(Constants::G_PROJECT_TREE);

  mfileContextMenu->appendGroup(Constants::G_FILE_OPEN);
  mfileContextMenu->appendGroup(Constants::G_FILE_OTHER);
  mfileContextMenu->appendGroup(Constants::G_FILE_CONFIG);
  mfileContextMenu->appendGroup(Constants::G_PROJECT_TREE);

  // Open Terminal submenu
  const auto openTerminal = ActionManager::createMenu(Constants::M_OPENTERMINALCONTEXT);
  openTerminal->setOnAllDisabledBehavior(ActionContainer::Show);
  dd->m_openTerminalMenu = openTerminal->menu();
  dd->m_openTerminalMenu->setTitle(Core::FileUtils::msgTerminalWithAction());

  // "open with" submenu
  const auto openWith = ActionManager::createMenu(Constants::M_OPENFILEWITHCONTEXT);
  openWith->setOnAllDisabledBehavior(ActionContainer::Show);
  dd->m_openWithMenu = openWith->menu();
  dd->m_openWithMenu->setTitle(tr("Open With"));

  mfolderContextMenu->addMenu(folderOpenLocationCtxMenu, Constants::G_FOLDER_LOCATIONS);
  connect(mfolderContextMenu->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateLocationSubMenus);

  //
  // Separators
  //

  Command *cmd;

  msessionContextMenu->addSeparator(projectTreeContext, Constants::G_SESSION_REBUILD);

  msessionContextMenu->addSeparator(projectTreeContext, Constants::G_SESSION_FILES);
  mprojectContextMenu->addSeparator(projectTreeContext, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addSeparator(projectTreeContext, Constants::G_PROJECT_FILES);
  mfile->addSeparator(Core::Constants::G_FILE_PROJECT);
  mbuild->addSeparator(Constants::G_BUILD_ALLPROJECTS);
  mbuild->addSeparator(Constants::G_BUILD_PROJECT);
  mbuild->addSeparator(Constants::G_BUILD_PRODUCT);
  mbuild->addSeparator(Constants::G_BUILD_SUBPROJECT);
  mbuild->addSeparator(Constants::G_BUILD_FILE);
  mbuild->addSeparator(Constants::G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS);
  mbuild->addSeparator(Constants::G_BUILD_PROJECT_ALLCONFIGURATIONS);
  msessionContextMenu->addSeparator(Constants::G_SESSION_OTHER);
  mbuild->addSeparator(Constants::G_BUILD_CANCEL);
  mbuild->addSeparator(Constants::G_BUILD_BUILD);
  mbuild->addSeparator(Constants::G_BUILD_RUN);
  mprojectContextMenu->addSeparator(Constants::G_PROJECT_REBUILD);

  //
  // Actions
  //

  // new action
  dd->m_newAction = new QAction(tr("New Project..."), this);
  cmd = ActionManager::registerAction(dd->m_newAction, Core::Constants::NEW, projectTreeContext);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);

  // open action
  dd->m_loadAction = new QAction(tr("Load Project..."), this);
  cmd = ActionManager::registerAction(dd->m_loadAction, Constants::LOAD);
  if (!HostOsInfo::isMacHost())
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+O")));
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);

  // Default open action
  dd->m_openFileAction = new QAction(tr("Open File"), this);
  cmd = ActionManager::registerAction(dd->m_openFileAction, Constants::OPENFILE, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OPEN);

  dd->m_searchOnFileSystem = new QAction(Core::FileUtils::msgFindInDirectory(), this);
  cmd = ActionManager::registerAction(dd->m_searchOnFileSystem, Constants::SEARCHONFILESYSTEM, projectTreeContext);

  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_CONFIG);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  dd->m_showInGraphicalShell = new QAction(Core::FileUtils::msgGraphicalShellAction(), this);
  cmd = ActionManager::registerAction(dd->m_showInGraphicalShell, Core::Constants::SHOWINGRAPHICALSHELL, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OPEN);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);

  // Show in File System View
  dd->m_showFileSystemPane = new QAction(Core::FileUtils::msgFileSystemAction(), this);
  cmd = ActionManager::registerAction(dd->m_showFileSystemPane, Constants::SHOWINFILESYSTEMVIEW, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OPEN);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  // Open Terminal Here menu
  dd->m_openTerminalHere = new QAction(Core::FileUtils::msgTerminalHereAction(), this);
  cmd = ActionManager::registerAction(dd->m_openTerminalHere, Constants::OPENTERMINALHERE, projectTreeContext);

  mfileContextMenu->addAction(cmd, Constants::G_FILE_OPEN);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  mfileContextMenu->addMenu(openTerminal, Constants::G_FILE_OPEN);
  mfolderContextMenu->addMenu(openTerminal, Constants::G_FOLDER_FILES);
  msubProjectContextMenu->addMenu(openTerminal, Constants::G_PROJECT_LAST);
  mprojectContextMenu->addMenu(openTerminal, Constants::G_PROJECT_LAST);

  dd->m_openTerminalHereBuildEnv = new QAction(tr("Build Environment"), this);
  dd->m_openTerminalHereRunEnv = new QAction(tr("Run Environment"), this);
  cmd = ActionManager::registerAction(dd->m_openTerminalHereBuildEnv, "ProjectExplorer.OpenTerminalHereBuildEnv", projectTreeContext);
  dd->m_openTerminalMenu->addAction(dd->m_openTerminalHereBuildEnv);

  cmd = ActionManager::registerAction(dd->m_openTerminalHereRunEnv, "ProjectExplorer.OpenTerminalHereRunEnv", projectTreeContext);
  dd->m_openTerminalMenu->addAction(dd->m_openTerminalHereRunEnv);

  // Open With menu
  mfileContextMenu->addMenu(openWith, Constants::G_FILE_OPEN);

  // recent projects menu
  auto mrecent = ActionManager::createMenu(Constants::M_RECENTPROJECTS);
  mrecent->menu()->setTitle(tr("Recent P&rojects"));
  mrecent->setOnAllDisabledBehavior(ActionContainer::Show);
  mfile->addMenu(mrecent, Core::Constants::G_FILE_OPEN);
  connect(mfile->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateRecentProjectMenu);

  // session menu
  auto msession = ActionManager::createMenu(Constants::M_SESSION);
  msession->menu()->setTitle(tr("S&essions"));
  msession->setOnAllDisabledBehavior(ActionContainer::Show);
  mfile->addMenu(msession, Core::Constants::G_FILE_OPEN);
  dd->m_sessionMenu = msession->menu();
  connect(mfile->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateSessionMenu);

  // session manager action
  dd->m_sessionManagerAction = new QAction(tr("&Manage..."), this);
  dd->m_sessionMenu->addAction(dd->m_sessionManagerAction);
  dd->m_sessionMenu->addSeparator();
  cmd->setDefaultKeySequence(QKeySequence());

  // unload action
  dd->m_unloadAction = new ParameterAction(tr("Close Project"), tr("Close Pro&ject \"%1\""), ParameterAction::AlwaysEnabled, this);
  cmd = ActionManager::registerAction(dd->m_unloadAction, Constants::UNLOAD);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_unloadAction->text());
  mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);

  dd->m_closeProjectFilesActionFileMenu = new ParameterAction(tr("Close All Files in Project"), tr("Close All Files in Project \"%1\""), ParameterAction::AlwaysEnabled, this);
  cmd = ActionManager::registerAction(dd->m_closeProjectFilesActionFileMenu, "ProjectExplorer.CloseProjectFilesFileMenu");
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_closeProjectFilesActionFileMenu->text());
  mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);

  auto munload = ActionManager::createMenu(Constants::M_UNLOADPROJECTS);
  munload->menu()->setTitle(tr("Close Pro&ject"));
  munload->setOnAllDisabledBehavior(ActionContainer::Show);
  mfile->addMenu(munload, Core::Constants::G_FILE_PROJECT);
  connect(mfile->menu(), &QMenu::aboutToShow, dd, &ProjectExplorerPluginPrivate::updateUnloadProjectMenu);

  // unload session action
  dd->m_closeAllProjects = new QAction(tr("Close All Projects and Editors"), this);
  cmd = ActionManager::registerAction(dd->m_closeAllProjects, Constants::CLEARSESSION);
  mfile->addAction(cmd, Core::Constants::G_FILE_PROJECT);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_FILES);

  // build session action
  const auto sideBarIcon = Icon::sideBarIcon(Icons::BUILD, Icons::BUILD_FLAT);
  const auto buildIcon = Icon::combinedIcon({Icons::BUILD_SMALL.icon(), sideBarIcon});
  dd->m_buildSessionAction = new QAction(buildIcon, tr("Build All Projects"), this);
  cmd = ActionManager::registerAction(dd->m_buildSessionAction, Constants::BUILDSESSION);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+B")));
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);

  dd->m_buildSessionForAllConfigsAction = new QAction(buildIcon, tr("Build All Projects for All Configurations"), this);
  cmd = ActionManager::registerAction(dd->m_buildSessionForAllConfigsAction, Constants::BUILDSESSIONALLCONFIGS);
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);

  // deploy session
  dd->m_deploySessionAction = new QAction(tr("Deploy"), this);
  dd->m_deploySessionAction->setWhatsThis(tr("Deploy All Projects"));
  cmd = ActionManager::registerAction(dd->m_deploySessionAction, Constants::DEPLOYSESSION);
  cmd->setDescription(dd->m_deploySessionAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_BUILD);

  // rebuild session action
  dd->m_rebuildSessionAction = new QAction(Icons::REBUILD.icon(), tr("Rebuild"), this);
  dd->m_rebuildSessionAction->setWhatsThis(tr("Rebuild All Projects"));
  cmd = ActionManager::registerAction(dd->m_rebuildSessionAction, Constants::REBUILDSESSION);
  cmd->setDescription(dd->m_rebuildSessionAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_REBUILD);

  dd->m_rebuildSessionForAllConfigsAction = new QAction(Icons::REBUILD.icon(), tr("Rebuild"), this);
  dd->m_rebuildSessionForAllConfigsAction->setWhatsThis(tr("Rebuild All Projects for All Configurations"));
  cmd = ActionManager::registerAction(dd->m_rebuildSessionForAllConfigsAction, Constants::REBUILDSESSIONALLCONFIGS);
  cmd->setDescription(dd->m_rebuildSessionForAllConfigsAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_REBUILD);

  // clean session
  dd->m_cleanSessionAction = new QAction(Utils::Icons::CLEAN.icon(), tr("Clean"), this);
  dd->m_cleanSessionAction->setWhatsThis(tr("Clean All Projects"));
  cmd = ActionManager::registerAction(dd->m_cleanSessionAction, Constants::CLEANSESSION);
  cmd->setDescription(dd->m_cleanSessionAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_REBUILD);

  dd->m_cleanSessionForAllConfigsAction = new QAction(Utils::Icons::CLEAN.icon(), tr("Clean"), this);
  dd->m_cleanSessionForAllConfigsAction->setWhatsThis(tr("Clean All Projects for All Configurations"));
  cmd = ActionManager::registerAction(dd->m_cleanSessionForAllConfigsAction, Constants::CLEANSESSIONALLCONFIGS);
  cmd->setDescription(dd->m_cleanSessionForAllConfigsAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_ALLPROJECTS_ALLCONFIGURATIONS);
  msessionContextMenu->addAction(cmd, Constants::G_SESSION_REBUILD);

  // build action
  dd->m_buildAction = new ParameterAction(tr("Build Project"), tr("Build Project \"%1\""), ParameterAction::AlwaysEnabled, this);
  dd->m_buildAction->setIcon(buildIcon);
  cmd = ActionManager::registerAction(dd->m_buildAction, Constants::BUILD);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_buildAction->text());
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+B")));
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);

  dd->m_buildProjectForAllConfigsAction = new ParameterAction(tr("Build Project for All Configurations"), tr("Build Project \"%1\" for All Configurations"), ParameterAction::AlwaysEnabled, this);
  dd->m_buildProjectForAllConfigsAction->setIcon(buildIcon);
  cmd = ActionManager::registerAction(dd->m_buildProjectForAllConfigsAction, Constants::BUILDALLCONFIGS);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_buildProjectForAllConfigsAction->text());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT_ALLCONFIGURATIONS);

  // Add to mode bar
  dd->m_modeBarBuildAction = new ProxyAction(this);
  dd->m_modeBarBuildAction->setObjectName("Build"); // used for UI introduction
  dd->m_modeBarBuildAction->initialize(cmd->action());
  dd->m_modeBarBuildAction->setAttribute(ProxyAction::UpdateText);
  dd->m_modeBarBuildAction->setAction(cmd->action());
  if (!hideBuildMenu())
    ModeManager::addAction(dd->m_modeBarBuildAction, Constants::P_ACTION_BUILDPROJECT);

  // build for run config
  dd->m_buildForRunConfigAction = new ParameterAction(tr("Build for &Run Configuration"), tr("Build for &Run Configuration \"%1\""), ParameterAction::EnabledWithParameter, this);
  dd->m_buildForRunConfigAction->setIcon(buildIcon);
  cmd = ActionManager::registerAction(dd->m_buildForRunConfigAction, "ProjectExplorer.BuildForRunConfig");
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_buildForRunConfigAction->text());
  mbuild->addAction(cmd, Constants::G_BUILD_BUILD);

  // deploy action
  dd->m_deployAction = new QAction(tr("Deploy"), this);
  dd->m_deployAction->setWhatsThis(tr("Deploy Project"));
  cmd = ActionManager::registerAction(dd->m_deployAction, Constants::DEPLOY);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_deployAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);

  // rebuild action
  dd->m_rebuildAction = new QAction(Icons::REBUILD.icon(), tr("Rebuild"), this);
  dd->m_rebuildAction->setWhatsThis(tr("Rebuild Project"));
  cmd = ActionManager::registerAction(dd->m_rebuildAction, Constants::REBUILD);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_rebuildAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);

  dd->m_rebuildProjectForAllConfigsAction = new QAction(Icons::REBUILD.icon(), tr("Rebuild"), this);
  dd->m_rebuildProjectForAllConfigsAction->setWhatsThis(tr("Rebuild Project for All Configurations"));
  cmd = ActionManager::registerAction(dd->m_rebuildProjectForAllConfigsAction, Constants::REBUILDALLCONFIGS);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_rebuildProjectForAllConfigsAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT_ALLCONFIGURATIONS);

  // clean action
  dd->m_cleanAction = new QAction(Utils::Icons::CLEAN.icon(), tr("Clean"), this);
  dd->m_cleanAction->setWhatsThis(tr("Clean Project"));
  cmd = ActionManager::registerAction(dd->m_cleanAction, Constants::CLEAN);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_cleanAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT);

  dd->m_cleanProjectForAllConfigsAction = new QAction(Utils::Icons::CLEAN.icon(), tr("Clean"), this);
  dd->m_cleanProjectForAllConfigsAction->setWhatsThis(tr("Clean Project for All Configurations"));
  cmd = ActionManager::registerAction(dd->m_cleanProjectForAllConfigsAction, Constants::CLEANALLCONFIGS);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_cleanProjectForAllConfigsAction->whatsThis());
  mbuild->addAction(cmd, Constants::G_BUILD_PROJECT_ALLCONFIGURATIONS);

  // cancel build action
  dd->m_cancelBuildAction = new QAction(Utils::Icons::STOP_SMALL.icon(), tr("Cancel Build"), this);
  cmd = ActionManager::registerAction(dd->m_cancelBuildAction, Constants::CANCELBUILD);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Backspace") : tr("Alt+Backspace")));
  mbuild->addAction(cmd, Constants::G_BUILD_CANCEL);

  // run action
  dd->m_runAction = new QAction(runIcon, tr("Run"), this);
  cmd = ActionManager::registerAction(dd->m_runAction, Constants::RUN);
  cmd->setAttribute(Command::CA_UpdateText);

  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+R")));
  mbuild->addAction(cmd, Constants::G_BUILD_RUN);

  cmd->action()->setObjectName("Run"); // used for UI introduction
  ModeManager::addAction(cmd->action(), Constants::P_ACTION_RUN);

  // Run without deployment action
  dd->m_runWithoutDeployAction = new QAction(tr("Run Without Deployment"), this);
  cmd = ActionManager::registerAction(dd->m_runWithoutDeployAction, Constants::RUNWITHOUTDEPLOY);
  mbuild->addAction(cmd, Constants::G_BUILD_RUN);

  // build action with dependencies (context menu)
  dd->m_buildDependenciesActionContextMenu = new QAction(tr("Build"), this);
  cmd = ActionManager::registerAction(dd->m_buildDependenciesActionContextMenu, Constants::BUILDDEPENDCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_BUILD);

  // build action (context menu)
  dd->m_buildActionContextMenu = new QAction(tr("Build Without Dependencies"), this);
  cmd = ActionManager::registerAction(dd->m_buildActionContextMenu, Constants::BUILDCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_BUILD);

  // rebuild action with dependencies (context menu)
  dd->m_rebuildDependenciesActionContextMenu = new QAction(tr("Rebuild"), this);
  cmd = ActionManager::registerAction(dd->m_rebuildDependenciesActionContextMenu, Constants::REBUILDDEPENDCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_REBUILD);

  // rebuild action (context menu)
  dd->m_rebuildActionContextMenu = new QAction(tr("Rebuild Without Dependencies"), this);
  cmd = ActionManager::registerAction(dd->m_rebuildActionContextMenu, Constants::REBUILDCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_REBUILD);

  // clean action with dependencies (context menu)
  dd->m_cleanDependenciesActionContextMenu = new QAction(tr("Clean"), this);
  cmd = ActionManager::registerAction(dd->m_cleanDependenciesActionContextMenu, Constants::CLEANDEPENDCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_REBUILD);

  // clean action (context menu)
  dd->m_cleanActionContextMenu = new QAction(tr("Clean Without Dependencies"), this);
  cmd = ActionManager::registerAction(dd->m_cleanActionContextMenu, Constants::CLEANCM, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_REBUILD);

  // build without dependencies action
  dd->m_buildProjectOnlyAction = new QAction(tr("Build Without Dependencies"), this);
  ActionManager::registerAction(dd->m_buildProjectOnlyAction, Constants::BUILDPROJECTONLY);

  // rebuild without dependencies action
  dd->m_rebuildProjectOnlyAction = new QAction(tr("Rebuild Without Dependencies"), this);
  ActionManager::registerAction(dd->m_rebuildProjectOnlyAction, Constants::REBUILDPROJECTONLY);

  // deploy without dependencies action
  dd->m_deployProjectOnlyAction = new QAction(tr("Deploy Without Dependencies"), this);
  ActionManager::registerAction(dd->m_deployProjectOnlyAction, Constants::DEPLOYPROJECTONLY);

  // clean without dependencies action
  dd->m_cleanProjectOnlyAction = new QAction(tr("Clean Without Dependencies"), this);
  ActionManager::registerAction(dd->m_cleanProjectOnlyAction, Constants::CLEANPROJECTONLY);

  // deploy action (context menu)
  dd->m_deployActionContextMenu = new QAction(tr("Deploy"), this);
  cmd = ActionManager::registerAction(dd->m_deployActionContextMenu, Constants::DEPLOYCM, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_RUN);

  dd->m_runActionContextMenu = new QAction(runIcon, tr("Run"), this);
  cmd = ActionManager::registerAction(dd->m_runActionContextMenu, Constants::RUNCONTEXTMENU, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_RUN);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_RUN);

  // add new file action
  dd->m_addNewFileAction = new QAction(tr("Add New..."), this);
  cmd = ActionManager::registerAction(dd->m_addNewFileAction, Constants::ADDNEWFILE, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);

  // add existing file action
  dd->m_addExistingFilesAction = new QAction(tr("Add Existing Files..."), this);
  cmd = ActionManager::registerAction(dd->m_addExistingFilesAction, Constants::ADDEXISTINGFILES, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);

  // add existing projects action
  dd->m_addExistingProjectsAction = new QAction(tr("Add Existing Projects..."), this);
  cmd = ActionManager::registerAction(dd->m_addExistingProjectsAction, "ProjectExplorer.AddExistingProjects", projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);

  // add existing directory action
  dd->m_addExistingDirectoryAction = new QAction(tr("Add Existing Directory..."), this);
  cmd = ActionManager::registerAction(dd->m_addExistingDirectoryAction, Constants::ADDEXISTINGDIRECTORY, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  mfolderContextMenu->addAction(cmd, Constants::G_FOLDER_FILES);

  // new subproject action
  dd->m_addNewSubprojectAction = new QAction(tr("New Subproject..."), this);
  cmd = ActionManager::registerAction(dd->m_addNewSubprojectAction, Constants::ADDNEWSUBPROJECT, projectTreeContext);
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);

  dd->m_closeProjectFilesActionContextMenu = new ParameterAction(tr("Close All Files"), tr("Close All Files in Project \"%1\""), ParameterAction::EnabledWithParameter, this);
  cmd = ActionManager::registerAction(dd->m_closeProjectFilesActionContextMenu, "ProjectExplorer.CloseAllFilesInProjectContextMenu");
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_closeProjectFilesActionContextMenu->text());
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  // unload project again, in right position
  dd->m_unloadActionContextMenu = new ParameterAction(tr("Close Project"), tr("Close Project \"%1\""), ParameterAction::EnabledWithParameter, this);
  cmd = ActionManager::registerAction(dd->m_unloadActionContextMenu, Constants::UNLOADCM);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_unloadActionContextMenu->text());
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  dd->m_unloadOthersActionContextMenu = new ParameterAction(tr("Close Other Projects"), tr("Close All Projects Except \"%1\""), ParameterAction::EnabledWithParameter, this);
  cmd = ActionManager::registerAction(dd->m_unloadOthersActionContextMenu, Constants::UNLOADOTHERSCM);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_unloadOthersActionContextMenu->text());
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_LAST);

  // file properties action
  dd->m_filePropertiesAction = new QAction(tr("Properties..."), this);
  cmd = ActionManager::registerAction(dd->m_filePropertiesAction, Constants::FILEPROPERTIES, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  // remove file action
  dd->m_removeFileAction = new QAction(tr("Remove..."), this);
  cmd = ActionManager::registerAction(dd->m_removeFileAction, Constants::REMOVEFILE, projectTreeContext);
  cmd->setDefaultKeySequences({QKeySequence::Delete, QKeySequence::Backspace});
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  // duplicate file action
  dd->m_duplicateFileAction = new QAction(tr("Duplicate File..."), this);
  cmd = ActionManager::registerAction(dd->m_duplicateFileAction, Constants::DUPLICATEFILE, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  //: Remove project from parent profile (Project explorer view); will not physically delete any files.
  dd->m_removeProjectAction = new QAction(tr("Remove Project..."), this);
  cmd = ActionManager::registerAction(dd->m_removeProjectAction, Constants::REMOVEPROJECT, projectTreeContext);
  msubProjectContextMenu->addAction(cmd, Constants::G_PROJECT_FILES);

  // delete file action
  dd->m_deleteFileAction = new QAction(tr("Delete File..."), this);
  cmd = ActionManager::registerAction(dd->m_deleteFileAction, Constants::DELETEFILE, projectTreeContext);
  cmd->setDefaultKeySequences({QKeySequence::Delete, QKeySequence::Backspace});
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  // renamefile action
  dd->m_renameFileAction = new QAction(tr("Rename..."), this);
  cmd = ActionManager::registerAction(dd->m_renameFileAction, Constants::RENAMEFILE, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  // diff file action
  dd->m_diffFileAction = TextEditor::TextDocument::createDiffAgainstCurrentFileAction(this, &ProjectTree::currentFilePath);
  cmd = ActionManager::registerAction(dd->m_diffFileAction, Constants::DIFFFILE, projectTreeContext);
  mfileContextMenu->addAction(cmd, Constants::G_FILE_OTHER);

  // Not yet used by anyone, so Hide for now
  //    mfolder->addAction(cmd, Constants::G_FOLDER_FILES);
  //    msubProject->addAction(cmd, Constants::G_FOLDER_FILES);
  //    mproject->addAction(cmd, Constants::G_FOLDER_FILES);

  // set startup project action
  dd->m_setStartupProjectAction = new ParameterAction(tr("Set as Active Project"), tr("Set \"%1\" as Active Project"), ParameterAction::AlwaysEnabled, this);
  cmd = ActionManager::registerAction(dd->m_setStartupProjectAction, Constants::SETSTARTUP, projectTreeContext);
  cmd->setAttribute(Command::CA_UpdateText);
  cmd->setDescription(dd->m_setStartupProjectAction->text());
  mprojectContextMenu->addAction(cmd, Constants::G_PROJECT_FIRST);

  // Collapse & Expand.
  const Id treeGroup = Constants::G_PROJECT_TREE;

  dd->m_projectTreeExpandNodeAction = new QAction(tr("Expand"), this);
  connect(dd->m_projectTreeExpandNodeAction, &QAction::triggered, ProjectTree::instance(), &ProjectTree::expandCurrentNodeRecursively);
  const auto expandNodeCmd = ActionManager::registerAction(dd->m_projectTreeExpandNodeAction, "ProjectExplorer.ExpandNode", projectTreeContext);
  dd->m_projectTreeCollapseAllAction = new QAction(tr("Collapse All"), this);
  const auto collapseCmd = ActionManager::registerAction(dd->m_projectTreeCollapseAllAction, Constants::PROJECTTREE_COLLAPSE_ALL, projectTreeContext);
  dd->m_projectTreeExpandAllAction = new QAction(tr("Expand All"), this);
  const auto expandCmd = ActionManager::registerAction(dd->m_projectTreeExpandAllAction, Constants::PROJECTTREE_EXPAND_ALL, projectTreeContext);
  for (const auto ac : {mfileContextMenu, msubProjectContextMenu, mfolderContextMenu, mprojectContextMenu, msessionContextMenu}) {
    ac->addSeparator(treeGroup);
    ac->addAction(expandNodeCmd, treeGroup);
    ac->addAction(collapseCmd, treeGroup);
    ac->addAction(expandCmd, treeGroup);
  }

  // target selector
  dd->m_projectSelectorAction = new QAction(this);
  dd->m_projectSelectorAction->setObjectName("KitSelector"); // used for UI introduction
  dd->m_projectSelectorAction->setCheckable(true);
  dd->m_projectSelectorAction->setEnabled(false);
  dd->m_targetSelector = new MiniProjectTargetSelector(dd->m_projectSelectorAction, ICore::dialogParent());
  connect(dd->m_projectSelectorAction, &QAction::triggered, dd->m_targetSelector, &QWidget::show);
  ModeManager::addProjectSelector(dd->m_projectSelectorAction);

  dd->m_projectSelectorActionMenu = new QAction(this);
  dd->m_projectSelectorActionMenu->setEnabled(false);
  dd->m_projectSelectorActionMenu->setText(tr("Open Build and Run Kit Selector..."));
  connect(dd->m_projectSelectorActionMenu, &QAction::triggered, dd->m_targetSelector, &MiniProjectTargetSelector::toggleVisible);
  cmd = ActionManager::registerAction(dd->m_projectSelectorActionMenu, Constants::SELECTTARGET);
  mbuild->addAction(cmd, Constants::G_BUILD_RUN);

  dd->m_projectSelectorActionQuick = new QAction(this);
  dd->m_projectSelectorActionQuick->setEnabled(false);
  dd->m_projectSelectorActionQuick->setText(tr("Quick Switch Kit Selector"));
  connect(dd->m_projectSelectorActionQuick, &QAction::triggered, dd->m_targetSelector, &MiniProjectTargetSelector::nextOrShow);
  cmd = ActionManager::registerAction(dd->m_projectSelectorActionQuick, Constants::SELECTTARGETQUICK);
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+T")));

  connect(ICore::instance(), &ICore::saveSettingsRequested, dd, &ProjectExplorerPluginPrivate::savePersistentSettings);
  connect(EditorManager::instance(), &EditorManager::autoSaved, this, [] {
    if (!dd->m_shuttingDown && !SessionManager::loadingSession())
      SessionManager::save();
  });
  connect(qApp, &QApplication::applicationStateChanged, this, [](Qt::ApplicationState state) {
    if (!dd->m_shuttingDown && state == Qt::ApplicationActive)
      dd->updateWelcomePage();
  });

  QSettings *s = ICore::settings();
  const auto fileNames = s->value(Constants::RECENTPROJECTS_FILE_NAMES_KEY).toStringList();
  const auto displayNames = s->value(Constants::RECENTPROJECTS_DISPLAY_NAMES_KEY).toStringList();
  if (fileNames.size() == displayNames.size()) {
    for (auto i = 0; i < fileNames.size(); ++i) {
      dd->m_recentProjects.append(qMakePair(fileNames.at(i), displayNames.at(i)));
    }
  }

  const auto buildBeforeDeploy = s->value(Constants::BUILD_BEFORE_DEPLOY_SETTINGS_KEY);
  const auto buildBeforeDeployString = buildBeforeDeploy.toString();
  if (buildBeforeDeployString == "true") {
    // backward compatibility with QtC < 4.12
    dd->m_projectExplorerSettings.buildBeforeDeploy = BuildBeforeRunMode::WholeProject;
  } else if (buildBeforeDeployString == "false") {
    dd->m_projectExplorerSettings.buildBeforeDeploy = BuildBeforeRunMode::Off;
  } else if (buildBeforeDeploy.isValid()) {
    dd->m_projectExplorerSettings.buildBeforeDeploy = static_cast<BuildBeforeRunMode>(buildBeforeDeploy.toInt());
  }

  static const ProjectExplorerSettings defaultSettings;

  dd->m_projectExplorerSettings.deployBeforeRun = s->value(Constants::DEPLOY_BEFORE_RUN_SETTINGS_KEY, defaultSettings.deployBeforeRun).toBool();
  dd->m_projectExplorerSettings.saveBeforeBuild = s->value(Constants::SAVE_BEFORE_BUILD_SETTINGS_KEY, defaultSettings.saveBeforeBuild).toBool();
  dd->m_projectExplorerSettings.useJom = s->value(Constants::USE_JOM_SETTINGS_KEY, defaultSettings.useJom).toBool();
  dd->m_projectExplorerSettings.autorestoreLastSession = s->value(Constants::AUTO_RESTORE_SESSION_SETTINGS_KEY, defaultSettings.autorestoreLastSession).toBool();
  dd->m_projectExplorerSettings.addLibraryPathsToRunEnv = s->value(Constants::ADD_LIBRARY_PATHS_TO_RUN_ENV_SETTINGS_KEY, defaultSettings.addLibraryPathsToRunEnv).toBool();
  dd->m_projectExplorerSettings.prompToStopRunControl = s->value(Constants::PROMPT_TO_STOP_RUN_CONTROL_SETTINGS_KEY, defaultSettings.prompToStopRunControl).toBool();
  dd->m_projectExplorerSettings.automaticallyCreateRunConfigurations = s->value(Constants::AUTO_CREATE_RUN_CONFIGS_SETTINGS_KEY, defaultSettings.automaticallyCreateRunConfigurations).toBool();
  dd->m_projectExplorerSettings.environmentId = QUuid(s->value(Constants::ENVIRONMENT_ID_SETTINGS_KEY).toByteArray());
  if (dd->m_projectExplorerSettings.environmentId.isNull())
    dd->m_projectExplorerSettings.environmentId = QUuid::createUuid();
  auto tmp = s->value(Constants::STOP_BEFORE_BUILD_SETTINGS_KEY, int(defaultSettings.stopBeforeBuild)).toInt();
  if (tmp < 0 || tmp > int(StopBeforeBuild::SameApp))
    tmp = int(defaultSettings.stopBeforeBuild);
  dd->m_projectExplorerSettings.stopBeforeBuild = StopBeforeBuild(tmp);
  dd->m_projectExplorerSettings.terminalMode = static_cast<TerminalMode>(s->value(Constants::TERMINAL_MODE_SETTINGS_KEY, int(defaultSettings.terminalMode)).toInt());
  dd->m_projectExplorerSettings.closeSourceFilesWithProject = s->value(Constants::CLOSE_FILES_WITH_PROJECT_SETTINGS_KEY, defaultSettings.closeSourceFilesWithProject).toBool();
  dd->m_projectExplorerSettings.clearIssuesOnRebuild = s->value(Constants::CLEAR_ISSUES_ON_REBUILD_SETTINGS_KEY, defaultSettings.clearIssuesOnRebuild).toBool();
  dd->m_projectExplorerSettings.abortBuildAllOnError = s->value(Constants::ABORT_BUILD_ALL_ON_ERROR_SETTINGS_KEY, defaultSettings.abortBuildAllOnError).toBool();
  dd->m_projectExplorerSettings.lowBuildPriority = s->value(Constants::LOW_BUILD_PRIORITY_SETTINGS_KEY, defaultSettings.lowBuildPriority).toBool();

  dd->m_buildPropertiesSettings.readSettings(s);

  const auto customParserCount = s->value(Constants::CUSTOM_PARSER_COUNT_KEY).toInt();
  for (auto i = 0; i < customParserCount; ++i) {
    CustomParserSettings settings;
    settings.fromMap(s->value(Constants::CUSTOM_PARSER_PREFIX_KEY + QString::number(i)).toMap());
    dd->m_customParsers << settings;
  }

  auto buildManager = new BuildManager(this, dd->m_cancelBuildAction);
  connect(buildManager, &BuildManager::buildStateChanged, dd, &ProjectExplorerPluginPrivate::updateActions);
  connect(buildManager, &BuildManager::buildQueueFinished, dd, &ProjectExplorerPluginPrivate::buildQueueFinished, Qt::QueuedConnection);

  connect(dd->m_sessionManagerAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::showSessionManager);
  connect(dd->m_newAction, &QAction::triggered, dd, &ProjectExplorerPlugin::openNewProjectDialog);
  connect(dd->m_loadAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::loadAction);
  connect(dd->m_buildProjectOnlyAction, &QAction::triggered, dd, [] {
    BuildManager::buildProjectWithoutDependencies(SessionManager::startupProject());
  });
  connect(dd->m_buildAction, &QAction::triggered, dd, [] {
    BuildManager::buildProjectWithDependencies(SessionManager::startupProject());
  });
  connect(dd->m_buildProjectForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::buildProjectWithDependencies(SessionManager::startupProject(), ConfigSelection::All);
  });
  connect(dd->m_buildActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::buildProjectWithoutDependencies(ProjectTree::currentProject());
  });
  connect(dd->m_buildForRunConfigAction, &QAction::triggered, dd, [] {
    const Project *const project = SessionManager::startupProject();
    QTC_ASSERT(project, return);
    const Target *const target = project->activeTarget();
    QTC_ASSERT(target, return);
    const RunConfiguration *const runConfig = target->activeRunConfiguration();
    QTC_ASSERT(runConfig, return);
    const auto productNode = runConfig->productNode();
    QTC_ASSERT(productNode, return);
    QTC_ASSERT(productNode->isProduct(), return);
    productNode->build();
  });
  connect(dd->m_buildDependenciesActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::buildProjectWithDependencies(ProjectTree::currentProject());
  });
  connect(dd->m_buildSessionAction, &QAction::triggered, dd, [] {
    BuildManager::buildProjects(SessionManager::projectOrder(), ConfigSelection::Active);
  });
  connect(dd->m_buildSessionForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::buildProjects(SessionManager::projectOrder(), ConfigSelection::All);
  });
  connect(dd->m_rebuildProjectOnlyAction, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjectWithoutDependencies(SessionManager::startupProject());
  });
  connect(dd->m_rebuildAction, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjectWithDependencies(SessionManager::startupProject(), ConfigSelection::Active);
  });
  connect(dd->m_rebuildProjectForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjectWithDependencies(SessionManager::startupProject(), ConfigSelection::All);
  });
  connect(dd->m_rebuildActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjectWithoutDependencies(ProjectTree::currentProject());
  });
  connect(dd->m_rebuildDependenciesActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjectWithDependencies(ProjectTree::currentProject(), ConfigSelection::Active);
  });
  connect(dd->m_rebuildSessionAction, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjects(SessionManager::projectOrder(), ConfigSelection::Active);
  });
  connect(dd->m_rebuildSessionForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::rebuildProjects(SessionManager::projectOrder(), ConfigSelection::All);
  });
  connect(dd->m_deployProjectOnlyAction, &QAction::triggered, dd, [] {
    BuildManager::deployProjects({SessionManager::startupProject()});
  });
  connect(dd->m_deployAction, &QAction::triggered, dd, [] {
    BuildManager::deployProjects(SessionManager::projectOrder(SessionManager::startupProject()));
  });
  connect(dd->m_deployActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::deployProjects({ProjectTree::currentProject()});
  });
  connect(dd->m_deploySessionAction, &QAction::triggered, dd, [] {
    BuildManager::deployProjects(SessionManager::projectOrder());
  });
  connect(dd->m_cleanProjectOnlyAction, &QAction::triggered, dd, [] {
    BuildManager::cleanProjectWithoutDependencies(SessionManager::startupProject());
  });
  connect(dd->m_cleanAction, &QAction::triggered, dd, [] {
    BuildManager::cleanProjectWithDependencies(SessionManager::startupProject(), ConfigSelection::Active);
  });
  connect(dd->m_cleanProjectForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::cleanProjectWithDependencies(SessionManager::startupProject(), ConfigSelection::All);
  });
  connect(dd->m_cleanActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::cleanProjectWithoutDependencies(ProjectTree::currentProject());
  });
  connect(dd->m_cleanDependenciesActionContextMenu, &QAction::triggered, dd, [] {
    BuildManager::cleanProjectWithDependencies(ProjectTree::currentProject(), ConfigSelection::Active);
  });
  connect(dd->m_cleanSessionAction, &QAction::triggered, dd, [] {
    BuildManager::cleanProjects(SessionManager::projectOrder(), ConfigSelection::Active);
  });
  connect(dd->m_cleanSessionForAllConfigsAction, &QAction::triggered, dd, [] {
    BuildManager::cleanProjects(SessionManager::projectOrder(), ConfigSelection::All);
  });
  connect(dd->m_runAction, &QAction::triggered, dd, []() { runStartupProject(Constants::NORMAL_RUN_MODE); });
  connect(dd->m_runActionContextMenu, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::runProjectContextMenu);
  connect(dd->m_runWithoutDeployAction, &QAction::triggered, dd, []() { runStartupProject(Constants::NORMAL_RUN_MODE, true); });
  connect(dd->m_cancelBuildAction, &QAction::triggered, BuildManager::instance(), &BuildManager::cancel);
  connect(dd->m_unloadAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::handleUnloadProject);
  connect(dd->m_unloadActionContextMenu, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::unloadProjectContextMenu);
  connect(dd->m_unloadOthersActionContextMenu, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::unloadOtherProjectsContextMenu);
  connect(dd->m_closeAllProjects, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::closeAllProjects);
  connect(dd->m_addNewFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::addNewFile);
  connect(dd->m_addExistingFilesAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::handleAddExistingFiles);
  connect(dd->m_addExistingDirectoryAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::addExistingDirectory);
  connect(dd->m_addNewSubprojectAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::addNewSubproject);
  connect(dd->m_addExistingProjectsAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::addExistingProjects);
  connect(dd->m_removeProjectAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::removeProject);
  connect(dd->m_openFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::openFile);
  connect(dd->m_searchOnFileSystem, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::searchOnFileSystem);
  connect(dd->m_showInGraphicalShell, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::showInGraphicalShell);
  // the following can delete the projects view that triggered the action, so make sure we
  // are out of the context menu before actually doing it by queuing the action
  connect(dd->m_showFileSystemPane, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::showInFileSystemPane, Qt::QueuedConnection);

  connect(dd->m_openTerminalHere, &QAction::triggered, dd, []() { dd->openTerminalHere(sysEnv); });
  connect(dd->m_openTerminalHereBuildEnv, &QAction::triggered, dd, []() { dd->openTerminalHere(buildEnv); });
  connect(dd->m_openTerminalHereRunEnv, &QAction::triggered, dd, []() { dd->openTerminalHereWithRunEnv(); });

  connect(dd->m_filePropertiesAction, &QAction::triggered, this, []() {
    const Node *currentNode = ProjectTree::currentNode();
    QTC_ASSERT(currentNode && currentNode->asFileNode(), return);
    ProjectTree::CurrentNodeKeeper nodeKeeper;
    DocumentManager::showFilePropertiesDialog(currentNode->filePath());
  });
  connect(dd->m_removeFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::removeFile);
  connect(dd->m_duplicateFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::duplicateFile);
  connect(dd->m_deleteFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::deleteFile);
  connect(dd->m_renameFileAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::handleRenameFile);
  connect(dd->m_setStartupProjectAction, &QAction::triggered, dd, &ProjectExplorerPluginPrivate::handleSetStartupProject);
  connect(dd->m_closeProjectFilesActionFileMenu, &QAction::triggered, dd, [] { dd->closeAllFilesInProject(SessionManager::projects().first()); });
  connect(dd->m_closeProjectFilesActionContextMenu, &QAction::triggered, dd, [] { dd->closeAllFilesInProject(ProjectTree::currentProject()); });
  connect(dd->m_projectTreeCollapseAllAction, &QAction::triggered, ProjectTree::instance(), &ProjectTree::collapseAll);
  connect(dd->m_projectTreeExpandAllAction, &QAction::triggered, ProjectTree::instance(), &ProjectTree::expandAll);

  connect(this, &ProjectExplorerPlugin::settingsChanged, dd, &ProjectExplorerPluginPrivate::updateRunWithoutDeployMenu);

  connect(ICore::instance(), &ICore::newItemDialogStateChanged, dd, [] {
    dd->updateContextMenuActions(ProjectTree::currentNode());
  });

  dd->updateWelcomePage();

  // FIXME: These are mostly "legacy"/"convenience" entries, relying on
  // the global entry point ProjectExplorer::currentProject(). They should
  // not be used in the Run/Build configuration pages.
  // TODO: Remove the CurrentProject versions in ~4.16
  auto expander = globalMacroExpander();
  expander->registerFileVariables(Constants::VAR_CURRENTPROJECT_PREFIX, tr("Current project's main file."), []() -> FilePath {
    FilePath projectFilePath;
    if (const auto project = ProjectTree::currentProject())
      projectFilePath = project->projectFilePath();
    return projectFilePath;
  }, false);
  expander->registerFileVariables("CurrentDocument:Project", tr("Main file of the project the current document belongs to."), []() -> FilePath {
    FilePath projectFilePath;
    if (const auto project = ProjectTree::currentProject())
      projectFilePath = project->projectFilePath();
    return projectFilePath;
  }, false);

  expander->registerVariable(Constants::VAR_CURRENTPROJECT_NAME, tr("The name of the current project."), []() -> QString {
    const auto project = ProjectTree::currentProject();
    return project ? project->displayName() : QString();
  }, false);
  expander->registerVariable("CurrentDocument:Project:Name", tr("The name of the project the current document belongs to."), []() -> QString {
    const auto project = ProjectTree::currentProject();
    return project ? project->displayName() : QString();
  });

  expander->registerPrefix(Constants::VAR_CURRENTBUILD_ENV, BuildConfiguration::tr("Variables in the current build environment."), [](const QString &var) {
    if (const auto bc = currentBuildConfiguration())
      return bc->environment().expandedValueForKey(var);
    return QString();
  }, false);
  const char currentBuildEnvVar[] = "CurrentDocument:Project:BuildConfig:Env";
  expander->registerPrefix(currentBuildEnvVar, BuildConfiguration::tr("Variables in the active build environment " "of the project containing the currently open document."), [](const QString &var) {
    if (const auto bc = currentBuildConfiguration())
      return bc->environment().expandedValueForKey(var);
    return QString();
  });
  EnvironmentProvider::addProvider({
    currentBuildEnvVar,
    tr("Current Build Environment"),
    []() {
      if (const auto bc = currentBuildConfiguration())
        return bc->environment();
      return Environment::systemEnvironment();
    }
  });
  EnvironmentProvider::addProvider({
    "CurrentDocument:Project:RunConfig:Env",
    tr("Current Run Environment"),
    []() {
      const Project *const project = ProjectTree::currentProject();
      const Target *const target = project ? project->activeTarget() : nullptr;
      const RunConfiguration *const rc = target ? target->activeRunConfiguration() : nullptr;
      if (rc) {
        if (const auto envAspect = rc->aspect<EnvironmentAspect>())
          return envAspect->environment();
      }
      return Environment::systemEnvironment();
    }
  });

  // Global variables for the active project.
  expander->registerVariable("ActiveProject:Name", tr("The name of the active project."), []() -> QString {
    if (const Project *const project = SessionManager::startupProject())
      return project->displayName();
    return {};
  });
  expander->registerFileVariables("ActiveProject", tr("Active project's main file."), []() -> FilePath {
    if (const Project *const project = SessionManager::startupProject())
      return project->projectFilePath();
    return {};
  });
  expander->registerVariable("ActiveProject:Kit:Name", "The name of the active project's active kit.", // TODO: tr()
                             []() -> QString {
                               if (const Target *const target = activeTarget())
                                 return target->kit()->displayName();
                               return {};
                             });
  expander->registerVariable("ActiveProject:BuildConfig:Name", "The name of the active project's active build configuration.", // TODO: tr()
                             []() -> QString {
                               if (const BuildConfiguration *const bc = activeBuildConfiguration())
                                 return bc->displayName();
                               return {};
                             });
  expander->registerVariable("ActiveProject:BuildConfig:Type", tr("The type of the active project's active build configuration."), []() -> QString {
    const BuildConfiguration *const bc = activeBuildConfiguration();
    const auto type = bc ? bc->buildType() : BuildConfiguration::Unknown;
    return BuildConfiguration::buildTypeName(type);
  });
  expander->registerVariable("ActiveProject:BuildConfig:Path", tr("Full build path of the active project's active build configuration."), []() -> QString {
    if (const BuildConfiguration *const bc = activeBuildConfiguration())
      return bc->buildDirectory().toUserOutput();
    return {};
  });
  const char activeBuildEnvVar[] = "ActiveProject:BuildConfig:Env";
  EnvironmentProvider::addProvider({
    activeBuildEnvVar,
    tr("Active build environment of the active project."),
    [] {
      if (const BuildConfiguration *const bc = activeBuildConfiguration())
        return bc->environment();
      return Environment::systemEnvironment();
    }
  });
  expander->registerPrefix(activeBuildEnvVar, BuildConfiguration::tr("Variables in the active build environment " "of the active project."), [](const QString &var) {
    if (const auto bc = activeBuildConfiguration())
      return bc->environment().expandedValueForKey(var);
    return QString();
  });

  expander->registerVariable("ActiveProject:RunConfig:Name", tr("Name of the active project's active run configuration."), []() -> QString {
    if (const RunConfiguration *const rc = activeRunConfiguration())
      return rc->displayName();
    return QString();
  });
  expander->registerFileVariables("ActiveProject:RunConfig:Executable", tr("The executable of the active project's active run configuration."), []() -> FilePath {
    if (const RunConfiguration *const rc = activeRunConfiguration())
      return rc->commandLine().executable();
    return {};
  });
  const char activeRunEnvVar[] = "ActiveProject:RunConfig:Env";
  EnvironmentProvider::addProvider({
    activeRunEnvVar,
    tr("Active run environment of the active project."),
    [] {
      if (const RunConfiguration *const rc = activeRunConfiguration()) {
        if (const auto envAspect = rc->aspect<EnvironmentAspect>())
          return envAspect->environment();
      }
      return Environment::systemEnvironment();
    }
  });
  expander->registerPrefix(activeRunEnvVar, tr("Variables in the environment of the active project's active run configuration."), [](const QString &var) {
    if (const RunConfiguration *const rc = activeRunConfiguration()) {
      if (const auto envAspect = rc->aspect<EnvironmentAspect>())
        return envAspect->environment().expandedValueForKey(var);
    }
    return QString();
  });
  expander->registerVariable("ActiveProject:RunConfig:WorkingDir", tr("The working directory of the active project's active run configuration."), [] {
    if (const RunConfiguration *const rc = activeRunConfiguration()) {
      if (const auto wdAspect = rc->aspect<WorkingDirectoryAspect>())
        return wdAspect->workingDirectory().toString();
    }
    return QString();
  });

  const auto fileHandler = [] {
    return SessionManager::sessionNameToFileName(SessionManager::activeSession());
  };
  expander->registerFileVariables("Session", tr("File where current session is saved."), fileHandler);
  expander->registerVariable("Session:Name", tr("Name of current session."), [] {
    return SessionManager::activeSession();
  });

  DeviceManager::instance()->addDevice(IDevice::Ptr(new DesktopDevice));

  return true;
}

auto ProjectExplorerPluginPrivate::loadAction() -> void
{
  auto dir = dd->m_lastOpenDirectory;

  // for your special convenience, we preselect a pro file if it is
  // the current file
  if (const IDocument *document = EditorManager::currentDocument()) {
    const auto fn = document->filePath().toString();
    const auto isProject = dd->m_profileMimeTypes.contains(document->mimeType());
    dir = isProject ? fn : QFileInfo(fn).absolutePath();
  }

  const auto filePath = Utils::FileUtils::getOpenFilePath(nullptr, tr("Load Project"), FilePath::fromString(dir), dd->m_projectFilterString);
  if (filePath.isEmpty())
    return;

  const auto result = ProjectExplorerPlugin::openProject(filePath);
  if (!result)
    ProjectExplorerPlugin::showOpenProjectError(result);

  updateActions();
}

auto ProjectExplorerPluginPrivate::unloadProjectContextMenu() -> void
{
  if (const auto p = ProjectTree::currentProject())
    ProjectExplorerPlugin::unloadProject(p);
}

auto ProjectExplorerPluginPrivate::unloadOtherProjectsContextMenu() -> void
{
  if (const auto currentProject = ProjectTree::currentProject()) {
    const auto projects = SessionManager::projects();
    QTC_ASSERT(!projects.isEmpty(), return);

    for (const auto p : projects) {
      if (p == currentProject)
        continue;
      ProjectExplorerPlugin::unloadProject(p);
    }
  }
}

auto ProjectExplorerPluginPrivate::handleUnloadProject() -> void
{
  auto projects = SessionManager::projects();
  QTC_ASSERT(!projects.isEmpty(), return);

  ProjectExplorerPlugin::unloadProject(projects.first());
}

auto ProjectExplorerPlugin::unloadProject(Project *project) -> void
{
  if (BuildManager::isBuilding(project)) {
    QMessageBox box;
    auto closeAnyway = box.addButton(tr("Cancel Build && Unload"), QMessageBox::AcceptRole);
    const auto cancelClose = box.addButton(tr("Do Not Unload"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelClose);
    box.setWindowTitle(tr("Unload Project %1?").arg(project->displayName()));
    box.setText(tr("The project %1 is currently being built.").arg(project->displayName()));
    box.setInformativeText(tr("Do you want to cancel the build process and unload the project anyway?"));
    box.exec();
    if (box.clickedButton() != closeAnyway)
      return;
    BuildManager::cancel();
  }

  if (projectExplorerSettings().closeSourceFilesWithProject && !dd->closeAllFilesInProject(project))
    return;

  dd->addToRecentProjects(project->projectFilePath().toString(), project->displayName());

  SessionManager::removeProject(project);
  dd->updateActions();
}

auto ProjectExplorerPluginPrivate::closeAllProjects() -> void
{
  if (!EditorManager::closeAllDocuments())
    return; // Action has been cancelled

  SessionManager::closeAllProjects();
  updateActions();

  ModeManager::activateMode(Core::Constants::MODE_WELCOME);
}

auto ProjectExplorerPlugin::extensionsInitialized() -> void
{
  // Register factories for all project managers
  QStringList allGlobPatterns;

  const QString filterSeparator = QLatin1String(";;");
  QStringList filterStrings;

  dd->m_documentFactory.setOpener([](FilePath filePath) {
    if (filePath.isDir()) {
      const auto files = projectFilesInDirectory(filePath.absoluteFilePath());
      if (!files.isEmpty())
        filePath = files.front();
    }

    const auto result = openProject(filePath);
    if (!result)
      showOpenProjectError(result);
    return nullptr;
  });

  dd->m_documentFactory.addMimeType(QStringLiteral("inode/directory"));
  for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
    const auto &mimeType = it.key();
    dd->m_documentFactory.addMimeType(mimeType);
    auto mime = mimeTypeForName(mimeType);
    allGlobPatterns.append(mime.globPatterns());
    filterStrings.append(mime.filterString());
    dd->m_profileMimeTypes += mimeType;
  }

  auto allProjectsFilter = tr("All Projects");
  allProjectsFilter += QLatin1String(" (") + allGlobPatterns.join(QLatin1Char(' ')) + QLatin1Char(')');
  filterStrings.prepend(allProjectsFilter);
  dd->m_projectFilterString = filterStrings.join(filterSeparator);

  BuildManager::extensionsInitialized();

  QSsh::SshSettings::loadSettings(ICore::settings());
  const auto searchPathRetriever = [] {
    FilePaths searchPaths = {ICore::libexecPath()};
    if (HostOsInfo::isWindowsHost()) {
      const auto gitBinary = ICore::settings()->value("Git/BinaryPath", "git").toString();
      const auto rawGitSearchPaths = ICore::settings()->value("Git/Path").toString().split(':', Qt::SkipEmptyParts);
      const auto gitSearchPaths = transform(rawGitSearchPaths, [](const QString &rawPath) { return FilePath::fromString(rawPath); });
      const auto fullGitPath = Environment::systemEnvironment().searchInPath(gitBinary, gitSearchPaths);
      if (!fullGitPath.isEmpty()) {
        searchPaths << fullGitPath.parentDir() << fullGitPath.parentDir().parentDir() + "/usr/bin";
      }
    }
    return searchPaths;
  };
  QSsh::SshSettings::setExtraSearchPathRetriever(searchPathRetriever);

  const auto parseIssuesAction = new QAction(tr("Parse Build Output..."), this);
  const auto mtools = ActionManager::actionContainer(Core::Constants::M_TOOLS);
  const auto cmd = ActionManager::registerAction(parseIssuesAction, "ProjectExplorer.ParseIssuesAction");
  connect(parseIssuesAction, &QAction::triggered, this, [] {
    ParseIssuesDialog dlg(ICore::dialogParent());
    dlg.exec();
  });
  mtools->addAction(cmd);

  // delay restoring kits until UI is shown for improved perceived startup performance
  QTimer::singleShot(0, this, &ProjectExplorerPlugin::restoreKits);
}

auto ProjectExplorerPlugin::restoreKits() -> void
{
  dd->determineSessionToRestoreAtStartup();
  ExtraAbi::load(); // Load this before Toolchains!
  DeviceManager::instance()->load();
  ToolChainManager::restoreToolChains();
  KitManager::restoreKits();
  QTimer::singleShot(0, dd, &ProjectExplorerPluginPrivate::restoreSession); // delay a bit...
}

auto ProjectExplorerPluginPrivate::updateRunWithoutDeployMenu() -> void
{
  m_runWithoutDeployAction->setVisible(m_projectExplorerSettings.deployBeforeRun);
}

auto ProjectExplorerPlugin::aboutToShutdown() -> ShutdownFlag
{
  disconnect(ModeManager::instance(), &ModeManager::currentModeChanged, dd, &ProjectExplorerPluginPrivate::currentModeChanged);
  ProjectTree::aboutToShutDown();
  ToolChainManager::aboutToShutdown();
  SessionManager::closeAllProjects();

  dd->m_shuttingDown = true;

  // Attempt to synchronously shutdown all run controls.
  // If that fails, fall back to asynchronous shutdown (Debugger run controls
  // might shutdown asynchronously).
  if (dd->m_activeRunControlCount == 0)
    return SynchronousShutdown;

  dd->m_outputPane.closeTabs(AppOutputPane::CloseTabNoPrompt /* No prompt any more */);
  dd->m_shutdownWatchDogId = dd->startTimer(10 * 1000); // Make sure we shutdown *somehow*
  return AsynchronousShutdown;
}

auto ProjectExplorerPlugin::showSessionManager() -> void
{
  dd->showSessionManager();
}

auto ProjectExplorerPlugin::openNewProjectDialog() -> void
{
  if (!ICore::isNewItemDialogRunning()) {
    ICore::showNewItemDialog(tr("New Project", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), [](IWizardFactory *f) { return !f->supportedProjectTypes().isEmpty(); }));
  } else {
    ICore::raiseWindow(ICore::newItemDialog());
  }
}

auto ProjectExplorerPluginPrivate::showSessionManager() -> void
{
  SessionManager::save();
  SessionDialog sessionDialog(ICore::dialogParent());
  sessionDialog.setAutoLoadSession(dd->m_projectExplorerSettings.autorestoreLastSession);
  sessionDialog.exec();
  dd->m_projectExplorerSettings.autorestoreLastSession = sessionDialog.autoLoadSession();

  updateActions();

  if (ModeManager::currentModeId() == Core::Constants::MODE_WELCOME)
    updateWelcomePage();
}

auto ProjectExplorerPluginPrivate::setStartupProject(Project *project) -> void
{
  if (!project)
    return;
  SessionManager::setStartupProject(project);
  updateActions();
}

auto ProjectExplorerPluginPrivate::closeAllFilesInProject(const Project *project) -> bool
{
  QTC_ASSERT(project, return false);
  auto openFiles = DocumentModel::entries();
  Utils::erase(openFiles, [project](const DocumentModel::Entry *entry) {
    return entry->pinned || !project->isKnownFile(entry->fileName());
  });
  for (const Project *const otherProject : SessionManager::projects()) {
    if (otherProject == project)
      continue;
    Utils::erase(openFiles, [otherProject](const DocumentModel::Entry *entry) {
      return otherProject->isKnownFile(entry->fileName());
    });
  }
  return EditorManager::closeDocuments(openFiles);
}

auto ProjectExplorerPluginPrivate::savePersistentSettings() -> void
{
  if (dd->m_shuttingDown)
    return;

  if (!SessionManager::loadingSession()) {
    for (const auto pro : SessionManager::projects())
      pro->saveSettings();

    SessionManager::save();
  }

  const auto s = ICore::settings();
  if (SessionManager::isDefaultVirgin()) {
    s->remove(Constants::STARTUPSESSION_KEY);
  } else {
    s->setValue(Constants::STARTUPSESSION_KEY, SessionManager::activeSession());
    s->setValue(Constants::LASTSESSION_KEY, SessionManager::activeSession());
  }
  s->remove(QLatin1String("ProjectExplorer/RecentProjects/Files"));

  QStringList fileNames;
  QStringList displayNames;
  QList<QPair<QString, QString>>::const_iterator it, end;
  end = dd->m_recentProjects.constEnd();
  for (it = dd->m_recentProjects.constBegin(); it != end; ++it) {
    fileNames << (*it).first;
    displayNames << (*it).second;
  }

  s->setValueWithDefault(Constants::RECENTPROJECTS_FILE_NAMES_KEY, fileNames);
  s->setValueWithDefault(Constants::RECENTPROJECTS_DISPLAY_NAMES_KEY, displayNames);

  static const ProjectExplorerSettings defaultSettings;

  s->setValueWithDefault(Constants::BUILD_BEFORE_DEPLOY_SETTINGS_KEY, int(dd->m_projectExplorerSettings.buildBeforeDeploy), int(defaultSettings.buildBeforeDeploy));
  s->setValueWithDefault(Constants::DEPLOY_BEFORE_RUN_SETTINGS_KEY, dd->m_projectExplorerSettings.deployBeforeRun, defaultSettings.deployBeforeRun);
  s->setValueWithDefault(Constants::SAVE_BEFORE_BUILD_SETTINGS_KEY, dd->m_projectExplorerSettings.saveBeforeBuild, defaultSettings.saveBeforeBuild);
  s->setValueWithDefault(Constants::USE_JOM_SETTINGS_KEY, dd->m_projectExplorerSettings.useJom, defaultSettings.useJom);
  s->setValueWithDefault(Constants::AUTO_RESTORE_SESSION_SETTINGS_KEY, dd->m_projectExplorerSettings.autorestoreLastSession, defaultSettings.autorestoreLastSession);
  s->setValueWithDefault(Constants::ADD_LIBRARY_PATHS_TO_RUN_ENV_SETTINGS_KEY, dd->m_projectExplorerSettings.addLibraryPathsToRunEnv, defaultSettings.addLibraryPathsToRunEnv);
  s->setValueWithDefault(Constants::PROMPT_TO_STOP_RUN_CONTROL_SETTINGS_KEY, dd->m_projectExplorerSettings.prompToStopRunControl, defaultSettings.prompToStopRunControl);
  s->setValueWithDefault(Constants::TERMINAL_MODE_SETTINGS_KEY, int(dd->m_projectExplorerSettings.terminalMode), int(defaultSettings.terminalMode));
  s->setValueWithDefault(Constants::CLOSE_FILES_WITH_PROJECT_SETTINGS_KEY, dd->m_projectExplorerSettings.closeSourceFilesWithProject, defaultSettings.closeSourceFilesWithProject);
  s->setValueWithDefault(Constants::CLEAR_ISSUES_ON_REBUILD_SETTINGS_KEY, dd->m_projectExplorerSettings.clearIssuesOnRebuild, defaultSettings.clearIssuesOnRebuild);
  s->setValueWithDefault(Constants::ABORT_BUILD_ALL_ON_ERROR_SETTINGS_KEY, dd->m_projectExplorerSettings.abortBuildAllOnError, defaultSettings.abortBuildAllOnError);
  s->setValueWithDefault(Constants::LOW_BUILD_PRIORITY_SETTINGS_KEY, dd->m_projectExplorerSettings.lowBuildPriority, defaultSettings.lowBuildPriority);
  s->setValueWithDefault(Constants::AUTO_CREATE_RUN_CONFIGS_SETTINGS_KEY, dd->m_projectExplorerSettings.automaticallyCreateRunConfigurations, defaultSettings.automaticallyCreateRunConfigurations);
  s->setValueWithDefault(Constants::ENVIRONMENT_ID_SETTINGS_KEY, dd->m_projectExplorerSettings.environmentId.toByteArray());
  s->setValueWithDefault(Constants::STOP_BEFORE_BUILD_SETTINGS_KEY, int(dd->m_projectExplorerSettings.stopBeforeBuild), int(defaultSettings.stopBeforeBuild));

  dd->m_buildPropertiesSettings.writeSettings(s);

  s->setValueWithDefault(Constants::CUSTOM_PARSER_COUNT_KEY, int(dd->m_customParsers.count()), 0);
  for (auto i = 0; i < dd->m_customParsers.count(); ++i) {
    s->setValue(Constants::CUSTOM_PARSER_PREFIX_KEY + QString::number(i), dd->m_customParsers.at(i).toMap());
  }
}

auto ProjectExplorerPlugin::openProjectWelcomePage(const QString &fileName) -> void
{
  const auto result = openProject(FilePath::fromUserInput(fileName));
  if (!result)
    showOpenProjectError(result);
}

auto ProjectExplorerPlugin::openProject(const FilePath &filePath) -> OpenProjectResult
{
  auto result = openProjects({filePath});
  const auto project = result.project();
  if (!project)
    return result;
  dd->addToRecentProjects(filePath.toString(), project->displayName());
  SessionManager::setStartupProject(project);
  return result;
}

auto ProjectExplorerPlugin::showOpenProjectError(const OpenProjectResult &result) -> void
{
  if (result)
    return;

  // Potentially both errorMessage and alreadyOpen could contain information
  // that should be shown to the user.
  // BUT, if Creator opens only a single project, this can lead
  // to either
  // - No error
  // - A errorMessage
  // - A single project in alreadyOpen

  // The only place where multiple projects are opened is in session restore
  // where the already open case should never happen, thus
  // the following code uses those assumptions to make the code simpler

  const auto errorMessage = result.errorMessage();
  if (!errorMessage.isEmpty()) {
    // ignore alreadyOpen
    QMessageBox::critical(ICore::dialogParent(), tr("Failed to Open Project"), errorMessage);
  } else {
    // ignore multiple alreadyOpen
    const auto alreadyOpen = result.alreadyOpen().constFirst();
    ProjectTree::highlightProject(alreadyOpen, tr("<h3>Project already open</h3>"));
  }
}

static auto appendError(QString &errorString, const QString &error) -> void
{
  if (error.isEmpty())
    return;

  if (!errorString.isEmpty())
    errorString.append(QLatin1Char('\n'));
  errorString.append(error);
}

auto ProjectExplorerPlugin::openProjects(const FilePaths &filePaths) -> OpenProjectResult
{
  QList<Project*> openedPro;
  QList<Project*> alreadyOpen;
  QString errorString;
  for (const auto &fileName : filePaths) {
    QTC_ASSERT(!fileName.isEmpty(), continue);
    const auto filePath = fileName.absoluteFilePath();

    const auto found = findOrDefault(SessionManager::projects(), equal(&Project::projectFilePath, filePath));
    if (found) {
      alreadyOpen.append(found);
      SessionManager::reportProjectLoadingProgress();
      continue;
    }

    auto mt = mimeTypeForFile(filePath);
    if (ProjectManager::canOpenProjectForMimeType(mt)) {
      if (!filePath.isFile()) {
        appendError(errorString, tr("Failed opening project \"%1\": Project is not a file.").arg(filePath.toUserOutput()));
      } else if (const auto pro = ProjectManager::openProject(mt, filePath)) {
        QString restoreError;
        const auto restoreResult = pro->restoreSettings(&restoreError);
        if (restoreResult == Project::RestoreResult::Ok) {
          connect(pro, &Project::fileListChanged, m_instance, &ProjectExplorerPlugin::fileListChanged);
          SessionManager::addProject(pro);
          openedPro += pro;
        } else {
          if (restoreResult == Project::RestoreResult::Error)
            appendError(errorString, restoreError);
          delete pro;
        }
      }
    } else {
      appendError(errorString, tr("Failed opening project \"%1\": No plugin can open project type \"%2\".").arg(filePath.toUserOutput()).arg(mt.name()));
    }
    if (filePaths.size() > 1)
      SessionManager::reportProjectLoadingProgress();
  }
  dd->updateActions();

  const auto switchToProjectsMode = anyOf(openedPro, &Project::needsConfiguration);
  const auto switchToEditMode = allOf(openedPro, [](Project *p) { return p->isEditModePreferred(); });
  if (!openedPro.isEmpty()) {
    if (switchToProjectsMode)
      ModeManager::activateMode(Constants::MODE_SESSION);
    else if (switchToEditMode)
      ModeManager::activateMode(Core::Constants::MODE_EDIT);
    ModeManager::setFocusToCurrentMode();
  }

  return OpenProjectResult(openedPro, alreadyOpen, errorString);
}

auto ProjectExplorerPluginPrivate::updateWelcomePage() -> void
{
  m_welcomePage.reloadWelcomeScreenData();
}

auto ProjectExplorerPluginPrivate::currentModeChanged(Id mode, Id oldMode) -> void
{
  if (oldMode == Constants::MODE_SESSION) {
    // Saving settings directly in a mode change is not a good idea, since the mode change
    // can be part of a bigger change. Save settings after that bigger change had a chance to
    // complete.
    QTimer::singleShot(0, ICore::instance(), [] { ICore::saveSettings(ICore::ModeChanged); });
  }
  if (mode == Core::Constants::MODE_WELCOME)
    updateWelcomePage();
}

auto ProjectExplorerPluginPrivate::determineSessionToRestoreAtStartup() -> void
{
  // Process command line arguments first:
  const auto lastSessionArg = m_instance->pluginSpec()->arguments().contains("-lastsession");
  m_sessionToRestoreAtStartup = lastSessionArg ? SessionManager::startupSession() : QString();
  auto arguments = ExtensionSystem::PluginManager::arguments();
  if (!lastSessionArg) {
    const auto sessions = SessionManager::sessions();
    // We have command line arguments, try to find a session in them
    // Default to no session loading
    foreach(const QString &arg, arguments) {
      if (sessions.contains(arg)) {
        // Session argument
        m_sessionToRestoreAtStartup = arg;
        break;
      }
    }
  }
  // Handle settings only after command line arguments:
  if (m_sessionToRestoreAtStartup.isEmpty() && m_projectExplorerSettings.autorestoreLastSession)
    m_sessionToRestoreAtStartup = SessionManager::startupSession();

  if (!m_sessionToRestoreAtStartup.isEmpty())
    ModeManager::activateMode(Core::Constants::MODE_EDIT);
}

// Return a list of glob patterns for project files ("*.pro", etc), use first, main pattern only.
auto ProjectExplorerPlugin::projectFileGlobs() -> QStringList
{
  QStringList result;
  for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
    auto mimeType = mimeTypeForName(it.key());
    if (mimeType.isValid()) {
      const auto patterns = mimeType.globPatterns();
      if (!patterns.isEmpty())
        result.append(patterns.front());
    }
  }
  return result;
}

auto ProjectExplorerPlugin::sharedThreadPool() -> QThreadPool*
{
  return &(dd->m_threadPool);
}

auto ProjectExplorerPlugin::targetSelector() -> MiniProjectTargetSelector*
{
  return dd->m_targetSelector;
}

/*!
    This function is connected to the ICore::coreOpened signal.  If
    there was no session explicitly loaded, it creates an empty new
    default session and puts the list of recent projects and sessions
    onto the welcome page.
*/
auto ProjectExplorerPluginPrivate::restoreSession() -> void
{
  // We have command line arguments, try to find a session in them
  auto arguments = ExtensionSystem::PluginManager::arguments();
  if (!dd->m_sessionToRestoreAtStartup.isEmpty() && !arguments.isEmpty())
    arguments.removeOne(dd->m_sessionToRestoreAtStartup);

  // Massage the argument list.
  // Be smart about directories: If there is a session of that name, load it.
  //   Other than that, look for project files in it. The idea is to achieve
  //   'Do what I mean' functionality when starting Creator in a directory with
  //   the single command line argument '.' and avoid editor warnings about not
  //   being able to open directories.
  // In addition, convert "filename" "+45" or "filename" ":23" into
  //   "filename+45"   and "filename:23".
  if (!arguments.isEmpty()) {
    const auto sessions = SessionManager::sessions();
    for (auto a = 0; a < arguments.size();) {
      const auto &arg = arguments.at(a);
      const QFileInfo fi(arg);
      if (fi.isDir()) {
        const QDir dir(fi.absoluteFilePath());
        // Does the directory name match a session?
        if (dd->m_sessionToRestoreAtStartup.isEmpty() && sessions.contains(dir.dirName())) {
          dd->m_sessionToRestoreAtStartup = dir.dirName();
          arguments.removeAt(a);
          continue;
        }
      } // Done directories.
      // Converts "filename" "+45" or "filename" ":23" into "filename+45" and "filename:23"
      if (a && (arg.startsWith(QLatin1Char('+')) || arg.startsWith(QLatin1Char(':')))) {
        arguments[a - 1].append(arguments.takeAt(a));
        continue;
      }
      ++a;
    } // for arguments
  }   // !arguments.isEmpty()
  // Restore latest session or what was passed on the command line

  SessionManager::loadSession(!dd->m_sessionToRestoreAtStartup.isEmpty() ? dd->m_sessionToRestoreAtStartup : QString(), true);

  // update welcome page
  connect(ModeManager::instance(), &ModeManager::currentModeChanged, dd, &ProjectExplorerPluginPrivate::currentModeChanged);
  connect(&dd->m_welcomePage, &ProjectWelcomePage::requestProject, m_instance, &ProjectExplorerPlugin::openProjectWelcomePage);
  dd->m_arguments = arguments;
  // delay opening projects from the command line even more
  QTimer::singleShot(0, m_instance, []() {
    ICore::openFiles(transform(dd->m_arguments, &FilePath::fromUserInput), ICore::OpenFilesFlags(ICore::CanContainLineAndColumnNumbers | ICore::SwitchMode));
    emit m_instance->finishedInitialization();
  });
  updateActions();
}

auto ProjectExplorerPluginPrivate::executeRunConfiguration(RunConfiguration *runConfiguration, Id runMode) -> void
{
  const auto runConfigIssues = runConfiguration->checkForIssues();
  if (!runConfigIssues.isEmpty()) {
    for (const auto &t : runConfigIssues)
      TaskHub::addTask(t);
    // TODO: Insert an extra task with a "link" to the run settings page?
    TaskHub::requestPopup();
    return;
  }

  const auto runControl = new RunControl(runMode);
  runControl->setRunConfiguration(runConfiguration);

  // A user needed interaction may have cancelled the run
  // (by example asking for a process pid or server url).
  if (!runControl->createMainWorker()) {
    delete runControl;
    return;
  }

  startRunControl(runControl);
}

auto ProjectExplorerPlugin::startRunControl(RunControl *runControl) -> void
{
  dd->startRunControl(runControl);
}

auto ProjectExplorerPlugin::showOutputPaneForRunControl(RunControl *runControl) -> void
{
  dd->showOutputPaneForRunControl(runControl);
}

auto ProjectExplorerPluginPrivate::startRunControl(RunControl *runControl) -> void
{
  m_outputPane.createNewOutputWindow(runControl);
  m_outputPane.flash(); // one flash for starting
  m_outputPane.showTabFor(runControl);
  const auto runMode = runControl->runMode();
  const auto popupMode = runMode == Constants::NORMAL_RUN_MODE ? m_outputPane.settings().runOutputMode : runMode == Constants::DEBUG_RUN_MODE ? m_outputPane.settings().debugOutputMode : AppOutputPaneMode::FlashOnOutput;
  m_outputPane.setBehaviorOnOutput(runControl, popupMode);
  connect(runControl, &QObject::destroyed, this, &ProjectExplorerPluginPrivate::checkForShutdown, Qt::QueuedConnection);
  ++m_activeRunControlCount;
  runControl->initiateStart();
  doUpdateRunActions();
}

auto ProjectExplorerPluginPrivate::showOutputPaneForRunControl(RunControl *runControl) -> void
{
  m_outputPane.showTabFor(runControl);
  m_outputPane.popup(IOutputPane::NoModeSwitch | IOutputPane::WithFocus);
}

auto ProjectExplorerPluginPrivate::checkForShutdown() -> void
{
  --m_activeRunControlCount;
  QTC_ASSERT(m_activeRunControlCount >= 0, m_activeRunControlCount = 0);
  if (m_shuttingDown && m_activeRunControlCount == 0) emit m_instance->asynchronousShutdownFinished();
}

auto ProjectExplorerPluginPrivate::timerEvent(QTimerEvent *ev) -> void
{
  if (m_shutdownWatchDogId == ev->timerId()) emit m_instance->asynchronousShutdownFinished();
}

auto ProjectExplorerPlugin::initiateInlineRenaming() -> void
{
  dd->handleRenameFile();
}

auto ProjectExplorerPluginPrivate::buildQueueFinished(bool success) -> void
{
  updateActions();

  auto ignoreErrors = true;
  if (!m_delayedRunConfiguration.isNull() && success && BuildManager::getErrorTaskCount() > 0) {
    ignoreErrors = QMessageBox::question(ICore::dialogParent(), ProjectExplorerPlugin::tr("Ignore All Errors?"), ProjectExplorerPlugin::tr("Found some build errors in current task.\n" "Do you want to ignore them?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
  }
  if (m_delayedRunConfiguration.isNull() && m_shouldHaveRunConfiguration) {
    QMessageBox::warning(ICore::dialogParent(), ProjectExplorerPlugin::tr("Run Configuration Removed"), ProjectExplorerPlugin::tr("The configuration that was supposed to run is no longer " "available."), QMessageBox::Ok);
  }

  if (success && ignoreErrors && !m_delayedRunConfiguration.isNull()) {
    executeRunConfiguration(m_delayedRunConfiguration.data(), m_runMode);
  } else {
    if (BuildManager::tasksAvailable())
      BuildManager::showTaskWindow();
  }
  m_delayedRunConfiguration = nullptr;
  m_shouldHaveRunConfiguration = false;
  m_runMode = Constants::NO_RUN_MODE;
  doUpdateRunActions();
}

auto ProjectExplorerPluginPrivate::recentProjects() const -> QList<QPair<QString, QString>>
{
  return filtered(dd->m_recentProjects, [](const QPair<QString, QString> &p) {
    return QFileInfo(p.first).isFile();
  });
}

auto ProjectExplorerPluginPrivate::updateActions() -> void
{
  const Project *const project = SessionManager::startupProject();
  const Project *const currentProject = ProjectTree::currentProject(); // for context menu actions

  const auto buildActionState = buildSettingsEnabled(project);
  const auto buildActionContextState = buildSettingsEnabled(currentProject);
  const auto buildSessionState = buildSettingsEnabledForSession();
  const auto isBuilding = BuildManager::isBuilding(project);

  const auto projectName = project ? project->displayName() : QString();
  const auto projectNameContextMenu = currentProject ? currentProject->displayName() : QString();

  m_unloadAction->setParameter(projectName);
  m_unloadActionContextMenu->setParameter(projectNameContextMenu);
  m_unloadOthersActionContextMenu->setParameter(projectNameContextMenu);
  m_closeProjectFilesActionFileMenu->setParameter(projectName);
  m_closeProjectFilesActionContextMenu->setParameter(projectNameContextMenu);

  // mode bar build action
  const auto buildAction = ActionManager::command(Constants::BUILD)->action();
  m_modeBarBuildAction->setAction(isBuilding ? ActionManager::command(Constants::CANCELBUILD)->action() : buildAction);
  m_modeBarBuildAction->setIcon(isBuilding ? Icons::CANCELBUILD_FLAT.icon() : buildAction->icon());

  const RunConfiguration *const runConfig = project && project->activeTarget() ? project->activeTarget()->activeRunConfiguration() : nullptr;

  // Normal actions
  m_buildAction->setParameter(projectName);
  m_buildProjectForAllConfigsAction->setParameter(projectName);
  if (runConfig)
    m_buildForRunConfigAction->setParameter(runConfig->displayName());

  m_buildAction->setEnabled(buildActionState.first);
  m_buildProjectForAllConfigsAction->setEnabled(buildActionState.first);
  m_rebuildAction->setEnabled(buildActionState.first);
  m_rebuildProjectForAllConfigsAction->setEnabled(buildActionState.first);
  m_cleanAction->setEnabled(buildActionState.first);
  m_cleanProjectForAllConfigsAction->setEnabled(buildActionState.first);

  // The last condition is there to prevent offering this action for custom run configurations.
  m_buildForRunConfigAction->setEnabled(buildActionState.first && runConfig && project->canBuildProducts() && !runConfig->buildTargetInfo().projectFilePath.isEmpty());

  m_buildAction->setToolTip(buildActionState.second);
  m_buildProjectForAllConfigsAction->setToolTip(buildActionState.second);
  m_rebuildAction->setToolTip(buildActionState.second);
  m_rebuildProjectForAllConfigsAction->setToolTip(buildActionState.second);
  m_cleanAction->setToolTip(buildActionState.second);
  m_cleanProjectForAllConfigsAction->setToolTip(buildActionState.second);

  // Context menu actions
  m_setStartupProjectAction->setParameter(projectNameContextMenu);
  m_setStartupProjectAction->setVisible(currentProject != project);

  const auto hasDependencies = SessionManager::projectOrder(currentProject).size() > 1;
  m_buildActionContextMenu->setVisible(hasDependencies);
  m_rebuildActionContextMenu->setVisible(hasDependencies);
  m_cleanActionContextMenu->setVisible(hasDependencies);

  m_buildActionContextMenu->setEnabled(buildActionContextState.first);
  m_rebuildActionContextMenu->setEnabled(buildActionContextState.first);
  m_cleanActionContextMenu->setEnabled(buildActionContextState.first);

  m_buildDependenciesActionContextMenu->setEnabled(buildActionContextState.first);
  m_rebuildDependenciesActionContextMenu->setEnabled(buildActionContextState.first);
  m_cleanDependenciesActionContextMenu->setEnabled(buildActionContextState.first);

  m_buildActionContextMenu->setToolTip(buildActionState.second);
  m_rebuildActionContextMenu->setToolTip(buildActionState.second);
  m_cleanActionContextMenu->setToolTip(buildActionState.second);

  // build project only
  m_buildProjectOnlyAction->setEnabled(buildActionState.first);
  m_rebuildProjectOnlyAction->setEnabled(buildActionState.first);
  m_cleanProjectOnlyAction->setEnabled(buildActionState.first);

  m_buildProjectOnlyAction->setToolTip(buildActionState.second);
  m_rebuildProjectOnlyAction->setToolTip(buildActionState.second);
  m_cleanProjectOnlyAction->setToolTip(buildActionState.second);

  // Session actions
  m_closeAllProjects->setEnabled(SessionManager::hasProjects());
  m_unloadAction->setVisible(SessionManager::projects().size() <= 1);
  m_unloadAction->setEnabled(SessionManager::projects().size() == 1);
  m_unloadActionContextMenu->setEnabled(SessionManager::hasProjects());
  m_unloadOthersActionContextMenu->setVisible(SessionManager::projects().size() >= 2);
  m_closeProjectFilesActionFileMenu->setVisible(SessionManager::projects().size() <= 1);
  m_closeProjectFilesActionFileMenu->setEnabled(SessionManager::projects().size() == 1);
  m_closeProjectFilesActionContextMenu->setEnabled(SessionManager::hasProjects());

  const auto aci = ActionManager::actionContainer(Constants::M_UNLOADPROJECTS);
  aci->menu()->menuAction()->setVisible(SessionManager::projects().size() > 1);

  m_buildSessionAction->setEnabled(buildSessionState.first);
  m_buildSessionForAllConfigsAction->setEnabled(buildSessionState.first);
  m_rebuildSessionAction->setEnabled(buildSessionState.first);
  m_rebuildSessionForAllConfigsAction->setEnabled(buildSessionState.first);
  m_cleanSessionAction->setEnabled(buildSessionState.first);
  m_cleanSessionForAllConfigsAction->setEnabled(buildSessionState.first);

  m_buildSessionAction->setToolTip(buildSessionState.second);
  m_buildSessionForAllConfigsAction->setToolTip(buildSessionState.second);
  m_rebuildSessionAction->setToolTip(buildSessionState.second);
  m_rebuildSessionForAllConfigsAction->setToolTip(buildSessionState.second);
  m_cleanSessionAction->setToolTip(buildSessionState.second);
  m_cleanSessionForAllConfigsAction->setToolTip(buildSessionState.second);

  m_cancelBuildAction->setEnabled(BuildManager::isBuilding());

  const auto hasProjects = SessionManager::hasProjects();
  m_projectSelectorAction->setEnabled(hasProjects);
  m_projectSelectorActionMenu->setEnabled(hasProjects);
  m_projectSelectorActionQuick->setEnabled(hasProjects);

  updateDeployActions();
  updateRunWithoutDeployMenu();
}

auto ProjectExplorerPlugin::saveModifiedFiles() -> bool
{
  const auto documentsToSave = DocumentManager::modifiedDocuments();
  if (!documentsToSave.isEmpty()) {
    if (dd->m_projectExplorerSettings.saveBeforeBuild) {
      auto cancelled = false;
      DocumentManager::saveModifiedDocumentsSilently(documentsToSave, &cancelled);
      if (cancelled)
        return false;
    } else {
      auto cancelled = false;
      auto alwaysSave = false;
      if (!DocumentManager::saveModifiedDocuments(documentsToSave, QString(), &cancelled, tr("Always save files before build"), &alwaysSave)) {
        if (cancelled)
          return false;
      }

      if (alwaysSave)
        dd->m_projectExplorerSettings.saveBeforeBuild = true;
    }
  }
  return true;
}

ProjectExplorerPluginPrivate::ProjectExplorerPluginPrivate() {}

auto ProjectExplorerPluginPrivate::extendFolderNavigationWidgetFactory() -> void
{
  const auto folderNavigationWidgetFactory = FolderNavigationWidgetFactory::instance();
  connect(folderNavigationWidgetFactory, &FolderNavigationWidgetFactory::aboutToShowContextMenu, this, [this](QMenu *menu, const FilePath &filePath, bool isDir) {
    if (isDir) {
      const auto actionOpenProjects = menu->addAction(ProjectExplorerPlugin::tr("Open Project in \"%1\"").arg(filePath.toUserOutput()));
      connect(actionOpenProjects, &QAction::triggered, this, [filePath] {
        openProjectsInDirectory(filePath);
      });
      if (projectsInDirectory(filePath).isEmpty())
        actionOpenProjects->setEnabled(false);
    } else if (ProjectExplorerPlugin::isProjectFile(filePath)) {
      const auto actionOpenAsProject = menu->addAction(tr("Open Project \"%1\"").arg(filePath.toUserOutput()));
      connect(actionOpenAsProject, &QAction::triggered, this, [filePath] {
        ProjectExplorerPlugin::openProject(filePath);
      });
    }
  });
  connect(folderNavigationWidgetFactory, &FolderNavigationWidgetFactory::fileRenamed, this, [](const FilePath &before, const FilePath &after) {
    const auto folderNodes = renamableFolderNodes(before, after);
    QVector<FolderNode*> failedNodes;
    for (const auto folder : folderNodes) {
      if (!folder->renameFile(before, after))
        failedNodes.append(folder);
    }
    if (!failedNodes.isEmpty()) {
      const auto projects = projectNames(failedNodes).join(", ");
      const auto errorMessage = ProjectExplorerPlugin::tr("The file \"%1\" was renamed to \"%2\", " "but the following projects could not be automatically changed: %3").arg(before.toUserOutput(), after.toUserOutput(), projects);
      QTimer::singleShot(0, ICore::instance(), [errorMessage] {
        QMessageBox::warning(ICore::dialogParent(), ProjectExplorerPlugin::tr("Project Editing Failed"), errorMessage);
      });
    }
  });
  connect(folderNavigationWidgetFactory, &FolderNavigationWidgetFactory::aboutToRemoveFile, this, [](const FilePath &filePath) {
    const auto folderNodes = removableFolderNodes(filePath);
    const auto failedNodes = filtered(folderNodes, [filePath](FolderNode *folder) {
      return folder->removeFiles({filePath}) != RemovedFilesFromProject::Ok;
    });
    if (!failedNodes.isEmpty()) {
      const auto projects = projectNames(failedNodes).join(", ");
      const auto errorMessage = tr("The following projects failed to automatically remove the file: %1").arg(projects);
      QTimer::singleShot(0, ICore::instance(), [errorMessage] {
        QMessageBox::warning(ICore::dialogParent(), tr("Project Editing Failed"), errorMessage);
      });
    }
  });
}

auto ProjectExplorerPluginPrivate::runProjectContextMenu() -> void
{
  const Node *node = ProjectTree::currentNode();
  const auto projectNode = node ? node->asProjectNode() : nullptr;
  if (projectNode == ProjectTree::currentProject()->rootProjectNode() || !projectNode) {
    ProjectExplorerPlugin::runProject(ProjectTree::currentProject(), Constants::NORMAL_RUN_MODE);
  } else {
    const auto act = qobject_cast<QAction*>(sender());
    if (!act)
      return;
    auto *rc = act->data().value<RunConfiguration*>();
    if (!rc)
      return;
    ProjectExplorerPlugin::runRunConfiguration(rc, Constants::NORMAL_RUN_MODE);
  }
}

static auto hasBuildSettings(const Project *pro) -> bool
{
  return anyOf(SessionManager::projectOrder(pro), [](const Project *project) {
    return project && project->activeTarget() && project->activeTarget()->activeBuildConfiguration();
  });
}

static auto subprojectEnabledState(const Project *pro) -> QPair<bool, QString>
{
  QPair<bool, QString> result;
  result.first = true;

  const auto &projects = SessionManager::projectOrder(pro);
  foreach(Project *project, projects) {
    if (project && project->activeTarget() && project->activeTarget()->activeBuildConfiguration() && !project->activeTarget()->activeBuildConfiguration()->isEnabled()) {
      result.first = false;
      result.second += QCoreApplication::translate("ProjectExplorerPluginPrivate", "Building \"%1\" is disabled: %2<br>").arg(project->displayName(), project->activeTarget()->activeBuildConfiguration()->disabledReason());
    }
  }

  return result;
}

auto ProjectExplorerPluginPrivate::buildSettingsEnabled(const Project *pro) -> QPair<bool, QString>
{
  QPair<bool, QString> result;
  result.first = true;
  if (!pro) {
    result.first = false;
    result.second = tr("No project loaded.");
  } else if (BuildManager::isBuilding(pro)) {
    result.first = false;
    result.second = tr("Currently building the active project.");
  } else if (pro->needsConfiguration()) {
    result.first = false;
    result.second = tr("The project %1 is not configured.").arg(pro->displayName());
  } else if (!hasBuildSettings(pro)) {
    result.first = false;
    result.second = tr("Project has no build settings.");
  } else {
    result = subprojectEnabledState(pro);
  }
  return result;
}

auto ProjectExplorerPluginPrivate::buildSettingsEnabledForSession() -> QPair<bool, QString>
{
  QPair<bool, QString> result;
  result.first = true;
  if (!SessionManager::hasProjects()) {
    result.first = false;
    result.second = tr("No project loaded.");
  } else if (BuildManager::isBuilding()) {
    result.first = false;
    result.second = tr("A build is in progress.");
  } else if (!hasBuildSettings(nullptr)) {
    result.first = false;
    result.second = tr("Project has no build settings.");
  } else {
    result = subprojectEnabledState(nullptr);
  }
  return result;
}

auto ProjectExplorerPlugin::coreAboutToClose() -> bool
{
  if (!m_instance)
    return true;
  if (BuildManager::isBuilding()) {
    QMessageBox box;
    auto closeAnyway = box.addButton(tr("Cancel Build && Close"), QMessageBox::AcceptRole);
    const auto cancelClose = box.addButton(tr("Do Not Close"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelClose);
    box.setWindowTitle(tr("Close %1?").arg(Core::Constants::IDE_DISPLAY_NAME));
    box.setText(tr("A project is currently being built."));
    box.setInformativeText(tr("Do you want to cancel the build process and close %1 anyway?").arg(Core::Constants::IDE_DISPLAY_NAME));
    box.exec();
    if (box.clickedButton() != closeAnyway)
      return false;
  }
  return dd->m_outputPane.aboutToClose();
}

auto ProjectExplorerPlugin::handleCommandLineArguments(const QStringList &arguments) -> void
{
  CustomWizard::setVerbose(arguments.count(QLatin1String("-customwizard-verbose")));
  JsonWizardFactory::setVerbose(arguments.count(QLatin1String("-customwizard-verbose")));

  const int kitForBinaryOptionIndex = arguments.indexOf("-ensure-kit-for-binary");
  if (kitForBinaryOptionIndex != -1) {
    if (kitForBinaryOptionIndex == arguments.count() - 1) {
      qWarning() << "The \"-ensure-kit-for-binary\" option requires a file path argument.";
    } else {
      const auto binary = FilePath::fromString(arguments.at(kitForBinaryOptionIndex + 1));
      if (binary.isEmpty() || !binary.exists())
        qWarning() << QString("No such file \"%1\".").arg(binary.toUserOutput());
      else
        KitManager::setBinaryForKit(binary);
    }
  }
}

static auto hasDeploySettings(Project *pro) -> bool
{
  return anyOf(SessionManager::projectOrder(pro), [](Project *project) {
    return project->activeTarget() && project->activeTarget()->activeDeployConfiguration();
  });
}

auto ProjectExplorerPlugin::runProject(Project *pro, Id mode, const bool forceSkipDeploy) -> void
{
  if (!pro)
    return;

  if (const auto target = pro->activeTarget())
    if (const auto rc = target->activeRunConfiguration())
      runRunConfiguration(rc, mode, forceSkipDeploy);
}

auto ProjectExplorerPlugin::runStartupProject(Id runMode, bool forceSkipDeploy) -> void
{
  runProject(SessionManager::startupProject(), runMode, forceSkipDeploy);
}

auto ProjectExplorerPlugin::runRunConfiguration(RunConfiguration *rc, Id runMode, const bool forceSkipDeploy) -> void
{
  if (!rc->isEnabled())
    return;
  const auto delay = [rc, runMode] {
    dd->m_runMode = runMode;
    dd->m_delayedRunConfiguration = rc;
    dd->m_shouldHaveRunConfiguration = true;
  };
  const auto buildStatus = forceSkipDeploy ? BuildManager::isBuilding(rc->project()) ? BuildForRunConfigStatus::Building : BuildForRunConfigStatus::NotBuilding : BuildManager::potentiallyBuildForRunConfig(rc);
  switch (buildStatus) {
  case BuildForRunConfigStatus::BuildFailed:
    return;
  case BuildForRunConfigStatus::Building: QTC_ASSERT(dd->m_runMode == Constants::NO_RUN_MODE, return);
    delay();
    break;
  case BuildForRunConfigStatus::NotBuilding:
    if (rc->isEnabled())
      dd->executeRunConfiguration(rc, runMode);
    else
      delay();
    break;
  }

  dd->doUpdateRunActions();
}

auto ProjectExplorerPlugin::runningRunControlProcesses() -> QList<QPair<Runnable, ProcessHandle>>
{
  QList<QPair<Runnable, ProcessHandle>> processes;
  foreach(RunControl *rc, allRunControls()) {
    if (rc->isRunning())
      processes << qMakePair(rc->runnable(), rc->applicationProcessHandle());
  }
  return processes;
}

auto ProjectExplorerPlugin::allRunControls() -> QList<RunControl*>
{
  return dd->m_outputPane.allRunControls();
}

auto ProjectExplorerPluginPrivate::projectAdded(Project *pro) -> void
{
  Q_UNUSED(pro)
  m_projectsMode.setEnabled(true);
}

auto ProjectExplorerPluginPrivate::projectRemoved(Project *pro) -> void
{
  Q_UNUSED(pro)
  m_projectsMode.setEnabled(SessionManager::hasProjects());
}

auto ProjectExplorerPluginPrivate::projectDisplayNameChanged(Project *pro) -> void
{
  addToRecentProjects(pro->projectFilePath().toString(), pro->displayName());
  updateActions();
}

auto ProjectExplorerPluginPrivate::updateDeployActions() -> void
{
  const auto project = SessionManager::startupProject();

  auto enableDeployActions = project && !BuildManager::isBuilding(project) && hasDeploySettings(project);
  const auto currentProject = ProjectTree::currentProject();
  auto enableDeployActionsContextMenu = currentProject && !BuildManager::isBuilding(currentProject) && hasDeploySettings(currentProject);

  if (m_projectExplorerSettings.buildBeforeDeploy != BuildBeforeRunMode::Off) {
    if (hasBuildSettings(project) && !buildSettingsEnabled(project).first)
      enableDeployActions = false;
    if (hasBuildSettings(currentProject) && !buildSettingsEnabled(currentProject).first)
      enableDeployActionsContextMenu = false;
  }

  const auto projectName = project ? project->displayName() : QString();
  const auto hasProjects = SessionManager::hasProjects();

  m_deployAction->setEnabled(enableDeployActions);

  m_deployActionContextMenu->setEnabled(enableDeployActionsContextMenu);

  m_deployProjectOnlyAction->setEnabled(enableDeployActions);

  auto enableDeploySessionAction = true;
  if (m_projectExplorerSettings.buildBeforeDeploy != BuildBeforeRunMode::Off) {
    auto hasDisabledBuildConfiguration = [](Project *project) {
      return project && project->activeTarget() && project->activeTarget()->activeBuildConfiguration() && !project->activeTarget()->activeBuildConfiguration()->isEnabled();
    };

    if (anyOf(SessionManager::projectOrder(nullptr), hasDisabledBuildConfiguration))
      enableDeploySessionAction = false;
  }
  if (!hasProjects || !hasDeploySettings(nullptr) || BuildManager::isBuilding())
    enableDeploySessionAction = false;
  m_deploySessionAction->setEnabled(enableDeploySessionAction);

  doUpdateRunActions();
}

auto ProjectExplorerPlugin::canRunStartupProject(Id runMode, QString *whyNot) -> bool
{
  const auto project = SessionManager::startupProject();
  if (!project) {
    if (whyNot)
      *whyNot = tr("No active project.");
    return false;
  }

  if (project->needsConfiguration()) {
    if (whyNot)
      *whyNot = tr("The project \"%1\" is not configured.").arg(project->displayName());
    return false;
  }

  const auto target = project->activeTarget();
  if (!target) {
    if (whyNot)
      *whyNot = tr("The project \"%1\" has no active kit.").arg(project->displayName());
    return false;
  }

  const auto activeRC = target->activeRunConfiguration();
  if (!activeRC) {
    if (whyNot)
      *whyNot = tr("The kit \"%1\" for the project \"%2\" has no active run configuration.").arg(target->displayName(), project->displayName());
    return false;
  }

  if (!activeRC->isEnabled()) {
    if (whyNot)
      *whyNot = activeRC->disabledReason();
    return false;
  }

  if (dd->m_projectExplorerSettings.buildBeforeDeploy != BuildBeforeRunMode::Off && dd->m_projectExplorerSettings.deployBeforeRun && !BuildManager::isBuilding(project) && hasBuildSettings(project)) {
    const auto buildState = dd->buildSettingsEnabled(project);
    if (!buildState.first) {
      if (whyNot)
        *whyNot = buildState.second;
      return false;
    }

    if (BuildManager::isBuilding()) {
      if (whyNot)
        *whyNot = tr("A build is still in progress.");
      return false;
    }
  }

  // shouldn't actually be shown to the user...
  if (!RunControl::canRun(runMode, DeviceTypeKitAspect::deviceTypeId(target->kit()), activeRC->id())) {
    if (whyNot)
      *whyNot = tr("Cannot run \"%1\".").arg(activeRC->displayName());
    return false;
  }

  if (dd->m_delayedRunConfiguration && dd->m_delayedRunConfiguration->project() == project) {
    if (whyNot)
      *whyNot = tr("A run action is already scheduled for the active project.");
    return false;
  }

  return true;
}

auto ProjectExplorerPluginPrivate::doUpdateRunActions() -> void
{
  QString whyNot;
  const auto state = ProjectExplorerPlugin::canRunStartupProject(Constants::NORMAL_RUN_MODE, &whyNot);
  m_runAction->setEnabled(state);
  m_runAction->setToolTip(whyNot);
  m_runWithoutDeployAction->setEnabled(state);

  emit m_instance->runActionsUpdated();
}

auto ProjectExplorerPluginPrivate::addToRecentProjects(const QString &fileName, const QString &displayName) -> void
{
  if (fileName.isEmpty())
    return;
  auto prettyFileName(QDir::toNativeSeparators(fileName));

  QList<QPair<QString, QString>>::iterator it;
  for (it = m_recentProjects.begin(); it != m_recentProjects.end();)
    if ((*it).first == prettyFileName)
      it = m_recentProjects.erase(it);
    else
      ++it;

  if (m_recentProjects.count() > m_maxRecentProjects)
    m_recentProjects.removeLast();
  m_recentProjects.prepend(qMakePair(prettyFileName, displayName));
  const QFileInfo fi(prettyFileName);
  m_lastOpenDirectory = fi.absolutePath();
  emit m_instance->recentProjectsChanged();
}

auto ProjectExplorerPluginPrivate::updateUnloadProjectMenu() -> void
{
  const auto aci = ActionManager::actionContainer(Constants::M_UNLOADPROJECTS);
  const auto menu = aci->menu();
  menu->clear();
  for (auto project : SessionManager::projects()) {
    const auto action = menu->addAction(tr("Close Project \"%1\"").arg(project->displayName()));
    connect(action, &QAction::triggered, [project] { ProjectExplorerPlugin::unloadProject(project); });
  }
}

auto ProjectExplorerPluginPrivate::updateRecentProjectMenu() -> void
{
  using StringPairListConstIterator = QList<QPair<QString, QString>>::const_iterator;
  const auto aci = ActionManager::actionContainer(Constants::M_RECENTPROJECTS);
  const auto menu = aci->menu();
  menu->clear();

  auto acceleratorKey = 1;
  const auto projects = recentProjects();
  //projects (ignore sessions, they used to be in this list)
  const auto end = projects.constEnd();
  for (auto it = projects.constBegin(); it != end; ++it, ++acceleratorKey) {
    const auto fileName = it->first;
    if (fileName.endsWith(QLatin1String(".qws")))
      continue;

    const auto actionText = ActionManager::withNumberAccelerator(withTildeHomePath(fileName), acceleratorKey);
    const auto action = menu->addAction(actionText);
    connect(action, &QAction::triggered, this, [this, fileName] {
      openRecentProject(fileName);
    });
  }
  const auto hasRecentProjects = !projects.empty();
  menu->setEnabled(hasRecentProjects);

  // add the Clear Menu item
  if (hasRecentProjects) {
    menu->addSeparator();
    const auto action = menu->addAction(QCoreApplication::translate("Core", Core::Constants::TR_CLEAR_MENU));
    connect(action, &QAction::triggered, this, &ProjectExplorerPluginPrivate::clearRecentProjects);
  }
  emit m_instance->recentProjectsChanged();
}

auto ProjectExplorerPluginPrivate::clearRecentProjects() -> void
{
  m_recentProjects.clear();
  updateWelcomePage();
}

auto ProjectExplorerPluginPrivate::openRecentProject(const QString &fileName) -> void
{
  if (!fileName.isEmpty()) {
    const auto result = ProjectExplorerPlugin::openProject(FilePath::fromUserInput(fileName));
    if (!result)
      ProjectExplorerPlugin::showOpenProjectError(result);
  }
}

auto ProjectExplorerPluginPrivate::removeFromRecentProjects(const QString &fileName, const QString &displayName) -> void
{
  QTC_ASSERT(!fileName.isEmpty() && !displayName.isEmpty(), return);
  QTC_CHECK(m_recentProjects.removeOne(QPair<QString, QString>(fileName, displayName)));
}

auto ProjectExplorerPluginPrivate::invalidateProject(Project *project) -> void
{
  disconnect(project, &Project::fileListChanged, m_instance, &ProjectExplorerPlugin::fileListChanged);
  updateActions();
}

auto ProjectExplorerPluginPrivate::updateContextMenuActions(Node *currentNode) -> void
{
  m_addExistingFilesAction->setEnabled(false);
  m_addExistingDirectoryAction->setEnabled(false);
  m_addNewFileAction->setEnabled(false);
  m_addNewSubprojectAction->setEnabled(false);
  m_addExistingProjectsAction->setEnabled(false);
  m_removeProjectAction->setEnabled(false);
  m_removeFileAction->setEnabled(false);
  m_duplicateFileAction->setEnabled(false);
  m_deleteFileAction->setEnabled(false);
  m_renameFileAction->setEnabled(false);
  m_diffFileAction->setEnabled(false);

  m_addExistingFilesAction->setVisible(true);
  m_addExistingDirectoryAction->setVisible(true);
  m_addNewFileAction->setVisible(true);
  m_addNewSubprojectAction->setVisible(true);
  m_addExistingProjectsAction->setVisible(true);
  m_removeProjectAction->setVisible(true);
  m_removeFileAction->setVisible(true);
  m_duplicateFileAction->setVisible(false);
  m_deleteFileAction->setVisible(true);
  m_runActionContextMenu->setVisible(false);
  m_diffFileAction->setVisible(DiffService::instance());

  m_openTerminalHere->setVisible(true);
  m_openTerminalHereBuildEnv->setVisible(false);
  m_openTerminalHereRunEnv->setVisible(false);

  m_showInGraphicalShell->setVisible(true);
  m_showFileSystemPane->setVisible(true);
  m_searchOnFileSystem->setVisible(true);

  const auto runMenu = ActionManager::actionContainer(Constants::RUNMENUCONTEXTMENU);
  runMenu->menu()->clear();
  runMenu->menu()->menuAction()->setVisible(false);

  if (currentNode && currentNode->managingProject()) {
    ProjectNode *pn;
    if (const ContainerNode *cn = currentNode->asContainerNode())
      pn = cn->rootProjectNode();
    else
      pn = const_cast<ProjectNode*>(currentNode->asProjectNode());

    const auto project = ProjectTree::currentProject();
    m_openTerminalHereBuildEnv->setVisible(bool(buildEnv(project)));
    m_openTerminalHereRunEnv->setVisible(canOpenTerminalWithRunEnv(project, pn));

    if (pn && project) {
      if (pn == project->rootProjectNode()) {
        m_runActionContextMenu->setVisible(true);
      } else {
        QList<RunConfiguration*> runConfigs;
        if (const auto t = project->activeTarget()) {
          const auto buildKey = pn->buildKey();
          for (const auto rc : t->runConfigurations()) {
            if (rc->buildKey() == buildKey)
              runConfigs.append(rc);
          }
        }
        if (runConfigs.count() == 1) {
          m_runActionContextMenu->setVisible(true);
          m_runActionContextMenu->setData(QVariant::fromValue(runConfigs.first()));
        } else if (runConfigs.count() > 1) {
          runMenu->menu()->menuAction()->setVisible(true);
          foreach(RunConfiguration *rc, runConfigs) {
            auto *act = new QAction(runMenu->menu());
            act->setData(QVariant::fromValue(rc));
            act->setText(tr("Run %1").arg(rc->displayName()));
            runMenu->menu()->addAction(act);
            connect(act, &QAction::triggered, this, &ProjectExplorerPluginPrivate::runProjectContextMenu);
          }
        }
      }
    }

    auto supports = [currentNode](ProjectAction action) {
      return currentNode->supportsAction(action, currentNode);
    };

    auto canEditProject = true;
    if (project && project->activeTarget()) {
      const BuildSystem *const bs = project->activeTarget()->buildSystem();
      if (bs->isParsing() || bs->isWaitingForParse())
        canEditProject = false;
    }
    if (currentNode->asFolderNode()) {
      // Also handles ProjectNode
      m_addNewFileAction->setEnabled(canEditProject && supports(AddNewFile) && !ICore::isNewItemDialogRunning());
      m_addNewSubprojectAction->setEnabled(canEditProject && currentNode->isProjectNodeType() && supports(AddSubProject) && !ICore::isNewItemDialogRunning());
      m_addExistingProjectsAction->setEnabled(canEditProject && currentNode->isProjectNodeType() && supports(AddExistingProject));
      m_removeProjectAction->setEnabled(canEditProject && currentNode->isProjectNodeType() && supports(RemoveSubProject));
      m_addExistingFilesAction->setEnabled(canEditProject && supports(AddExistingFile));
      m_addExistingDirectoryAction->setEnabled(canEditProject && supports(AddExistingDirectory));
      m_renameFileAction->setEnabled(canEditProject && supports(Rename));
    } else if (const auto fileNode = currentNode->asFileNode()) {
      // Enable and Show remove / delete in magic ways:
      // If both are disabled Show Remove
      // If both are enabled Show both (can't happen atm)
      // If only removeFile is enabled only Show it
      // If only deleteFile is enable only Show it
      const auto isTypeProject = fileNode->fileType() == FileType::Project;
      const auto enableRemove = canEditProject && !isTypeProject && supports(RemoveFile);
      m_removeFileAction->setEnabled(enableRemove);
      const auto enableDelete = canEditProject && !isTypeProject && supports(EraseFile);
      m_deleteFileAction->setEnabled(enableDelete);
      m_deleteFileAction->setVisible(enableDelete);

      m_removeFileAction->setVisible(!enableDelete || enableRemove);
      m_renameFileAction->setEnabled(canEditProject && !isTypeProject && supports(Rename));
      const auto currentNodeIsTextFile = isTextFile(currentNode->filePath());
      m_diffFileAction->setEnabled(DiffService::instance() && currentNodeIsTextFile && TextEditor::TextDocument::currentTextDocument());

      const auto canDuplicate = canEditProject && supports(AddNewFile) && currentNode->asFileNode()->fileType() != FileType::Project;
      m_duplicateFileAction->setVisible(canDuplicate);
      m_duplicateFileAction->setEnabled(canDuplicate);

      EditorManager::populateOpenWithMenu(m_openWithMenu, currentNode->filePath());
    }

    if (supports(HidePathActions)) {
      m_openTerminalHere->setVisible(false);
      m_showInGraphicalShell->setVisible(false);
      m_showFileSystemPane->setVisible(false);
      m_searchOnFileSystem->setVisible(false);
    }

    if (supports(HideFileActions)) {
      m_deleteFileAction->setVisible(false);
      m_removeFileAction->setVisible(false);
    }

    if (supports(HideFolderActions)) {
      m_addNewFileAction->setVisible(false);
      m_addNewSubprojectAction->setVisible(false);
      m_addExistingProjectsAction->setVisible(false);
      m_removeProjectAction->setVisible(false);
      m_addExistingFilesAction->setVisible(false);
      m_addExistingDirectoryAction->setVisible(false);
    }
  }
}

auto ProjectExplorerPluginPrivate::updateLocationSubMenus() -> void
{
  static QList<QAction*> actions;
  qDeleteAll(actions); // This will also remove these actions from the menus!
  actions.clear();

  const auto projectMenuContainer = ActionManager::actionContainer(Constants::PROJECT_OPEN_LOCATIONS_CONTEXT_MENU);
  const auto projectMenu = projectMenuContainer->menu();
  QTC_CHECK(projectMenu->actions().isEmpty());

  const auto folderMenuContainer = ActionManager::actionContainer(Constants::FOLDER_OPEN_LOCATIONS_CONTEXT_MENU);
  const auto folderMenu = folderMenuContainer->menu();
  QTC_CHECK(folderMenu->actions().isEmpty());

  const FolderNode *const fn = ProjectTree::currentNode() ? ProjectTree::currentNode()->asFolderNode() : nullptr;
  const auto locations = fn ? fn->locationInfo() : QVector<FolderNode::LocationInfo>();

  const auto isVisible = !locations.isEmpty();
  projectMenu->menuAction()->setVisible(isVisible);
  folderMenu->menuAction()->setVisible(isVisible);

  if (!isVisible)
    return;

  unsigned int lastPriority = 0;
  for (const auto &li : locations) {
    if (li.priority != lastPriority) {
      projectMenu->addSeparator();
      folderMenu->addSeparator();
      lastPriority = li.priority;
    }
    const auto line = li.line;
    const auto path = li.path;
    auto displayName = fn->filePath() == li.path ? li.displayName : tr("%1 in %2").arg(li.displayName).arg(li.path.toUserOutput());
    auto *action = new QAction(displayName, nullptr);
    connect(action, &QAction::triggered, this, [line, path]() {
      EditorManager::openEditorAt(Link(path, line), {}, EditorManager::AllowExternalEditor);
    });

    projectMenu->addAction(action);
    folderMenu->addAction(action);

    actions.append(action);
  }
}

auto ProjectExplorerPluginPrivate::addNewFile() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  const auto location = currentNode->directory();

  QVariantMap map;
  // store void pointer to avoid QVariant to use qobject_cast, which might core-dump when trying
  // to access meta data on an object that get deleted in the meantime:
  map.insert(QLatin1String(Constants::PREFERRED_PROJECT_NODE), QVariant::fromValue(static_cast<void*>(currentNode)));
  map.insert(Constants::PREFERRED_PROJECT_NODE_PATH, currentNode->filePath().toString());
  if (const auto p = ProjectTree::currentProject()) {
    const auto profileIds = transform(p->targets(), [](const Target *t) {
      return t->id().toString();
    });
    map.insert(QLatin1String(Constants::PROJECT_KIT_IDS), profileIds);
    map.insert(Constants::PROJECT_POINTER, QVariant::fromValue(static_cast<void*>(p)));
  }
  ICore::showNewItemDialog(ProjectExplorerPlugin::tr("New File", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), [](IWizardFactory *f) {
    return f->supportedProjectTypes().isEmpty();
  }), location, map);
}

auto ProjectExplorerPluginPrivate::addNewSubproject() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  const auto location = currentNode->directory();

  if (currentNode->isProjectNodeType() && currentNode->supportsAction(AddSubProject, currentNode)) {
    QVariantMap map;
    map.insert(QLatin1String(Constants::PREFERRED_PROJECT_NODE), QVariant::fromValue(currentNode));
    const auto project = ProjectTree::currentProject();
    Id projectType;
    if (project) {
      const auto profileIds = transform(ProjectTree::currentProject()->targets(), [](const Target *t) {
        return t->id().toString();
      });
      map.insert(QLatin1String(Constants::PROJECT_KIT_IDS), profileIds);
      projectType = project->id();
    }

    ICore::showNewItemDialog(tr("New Subproject", "Title of dialog"), filtered(IWizardFactory::allWizardFactories(), [projectType](IWizardFactory *f) {
      return projectType.isValid() ? f->supportedProjectTypes().contains(projectType) : !f->supportedProjectTypes().isEmpty();
    }), location, map);
  }
}

auto ProjectExplorerPluginPrivate::addExistingProjects() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  if (!currentNode)
    return;
  auto projectNode = currentNode->asProjectNode();
  if (!projectNode && currentNode->asContainerNode())
    projectNode = currentNode->asContainerNode()->rootProjectNode();
  QTC_ASSERT(projectNode, return);
  const auto dir = currentNode->directory();
  auto subProjectFilePaths = Utils::FileUtils::getOpenFilePaths(nullptr, tr("Choose Project File"), dir, projectNode->subProjectFileNamePatterns().join(";;"));
  if (!ProjectTree::hasNode(projectNode))
    return;
  const auto childNodes = projectNode->nodes();
  Utils::erase(subProjectFilePaths, [childNodes](const FilePath &filePath) {
    return anyOf(childNodes, [filePath](const Node *n) {
      return n->filePath() == filePath;
    });
  });
  if (subProjectFilePaths.empty())
    return;
  FilePaths failedProjects;
  FilePaths addedProjects;
  for (const auto &filePath : qAsConst(subProjectFilePaths)) {
    if (projectNode->addSubProject(filePath))
      addedProjects << filePath;
    else
      failedProjects << filePath;
  }
  if (!failedProjects.empty()) {
    const auto message = tr("The following subprojects could not be added to project " "\"%1\":").arg(projectNode->managingProject()->displayName());
    QMessageBox::warning(ICore::dialogParent(), tr("Adding Subproject Failed"), message + "\n  " + FilePath::formatFilePaths(failedProjects, "\n  "));
    return;
  }
  VcsManager::promptToAdd(dir, addedProjects);
}

auto ProjectExplorerPluginPrivate::handleAddExistingFiles() -> void
{
  const auto node = ProjectTree::currentNode();
  const auto folderNode = node ? node->asFolderNode() : nullptr;

  QTC_ASSERT(folderNode, return);

  const auto filePaths = Utils::FileUtils::getOpenFilePaths(nullptr, tr("Add Existing Files"), node->directory());
  if (filePaths.isEmpty())
    return;

  ProjectExplorerPlugin::addExistingFiles(folderNode, filePaths);
}

auto ProjectExplorerPluginPrivate::addExistingDirectory() -> void
{
  const auto node = ProjectTree::currentNode();
  const auto folderNode = node ? node->asFolderNode() : nullptr;

  QTC_ASSERT(folderNode, return);

  SelectableFilesDialogAddDirectory dialog(node->directory(), FilePaths(), ICore::dialogParent());
  dialog.setAddFileFilter({});

  if (dialog.exec() == QDialog::Accepted)
    ProjectExplorerPlugin::addExistingFiles(folderNode, dialog.selectedFiles());
}

auto ProjectExplorerPlugin::addExistingFiles(FolderNode *folderNode, const FilePaths &filePaths) -> void
{
  // can happen when project is not yet parsed or finished parsing while the dialog was open:
  if (!folderNode || !ProjectTree::hasNode(folderNode))
    return;

  const auto dir = folderNode->directory();
  auto fileNames = filePaths;
  FilePaths notAdded;
  folderNode->addFiles(fileNames, &notAdded);

  if (!notAdded.isEmpty()) {
    const QString message = tr("Could not add following files to project %1:").arg(folderNode->managingProject()->displayName()) + QLatin1Char('\n');
    QMessageBox::warning(ICore::dialogParent(), tr("Adding Files to Project Failed"), message + FilePath::formatFilePaths(notAdded, "\n"));
    fileNames = filtered(fileNames, [&notAdded](const FilePath &f) { return !notAdded.contains(f); });
  }

  VcsManager::promptToAdd(dir, fileNames);
}

auto ProjectExplorerPluginPrivate::removeProject() -> void
{
  const auto node = ProjectTree::currentNode();
  if (!node)
    return;
  const auto projectNode = node->managingProject();
  if (projectNode) {
    RemoveFileDialog removeFileDialog(node->filePath(), ICore::dialogParent());
    removeFileDialog.setDeleteFileVisible(false);
    if (removeFileDialog.exec() == QDialog::Accepted)
      projectNode->removeSubProject(node->filePath());
  }
}

auto ProjectExplorerPluginPrivate::openFile() -> void
{
  const Node *currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  EditorManager::openEditor(currentNode->filePath());
}

auto ProjectExplorerPluginPrivate::searchOnFileSystem() -> void
{
  const Node *currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  TextEditor::FindInFiles::findOnFileSystem(currentNode->path().toString());
}

auto ProjectExplorerPluginPrivate::showInGraphicalShell() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  Core::FileUtils::showInGraphicalShell(ICore::dialogParent(), currentNode->path());
}

auto ProjectExplorerPluginPrivate::showInFileSystemPane() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);
  Core::FileUtils::showInFileSystemView(currentNode->filePath());
}

auto ProjectExplorerPluginPrivate::openTerminalHere(const EnvironmentGetter &env) -> void
{
  const Node *currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);

  const auto environment = env(ProjectTree::projectForNode(currentNode));
  if (!environment)
    return;

  Core::FileUtils::openTerminal(currentNode->directory(), environment.value());
}

auto ProjectExplorerPluginPrivate::openTerminalHereWithRunEnv() -> void
{
  const Node *currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode, return);

  const Project *const project = ProjectTree::projectForNode(currentNode);
  QTC_ASSERT(project, return);
  const Target *const target = project->activeTarget();
  QTC_ASSERT(target, return);
  const auto runConfig = runConfigForNode(target, currentNode->asProjectNode());
  QTC_ASSERT(runConfig, return);

  const auto runnable = runConfig->runnable();
  auto device = runnable.device;
  if (!device)
    device = DeviceKitAspect::device(target->kit());
  QTC_ASSERT(device && device->canOpenTerminal(), return);
  const auto workingDir = device->type() == Constants::DESKTOP_DEVICE_TYPE ? currentNode->directory() : runnable.workingDirectory;
  device->openTerminal(runnable.environment, workingDir);
}

auto ProjectExplorerPluginPrivate::removeFile() -> void
{
  const Node *currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode && currentNode->asFileNode(), return);

  ProjectTree::CurrentNodeKeeper nodeKeeper;

  const auto filePath = currentNode->filePath();
  using NodeAndPath = QPair<const Node*, FilePath>;
  QList<NodeAndPath> filesToRemove{qMakePair(currentNode, currentNode->filePath())};
  QList<NodeAndPath> siblings;
  for (const Node *const n : ProjectTree::siblingsWithSameBaseName(currentNode))
    siblings << qMakePair(n, n->filePath());

  RemoveFileDialog removeFileDialog(filePath, ICore::dialogParent());
  if (removeFileDialog.exec() != QDialog::Accepted)
    return;

  const auto deleteFile = removeFileDialog.isDeleteFileChecked();

  if (!siblings.isEmpty()) {
    const auto reply = QMessageBox::question(ICore::dialogParent(), tr("Remove More Files?"), tr("Remove these files as well?\n    %1").arg(Utils::transform<QStringList>(siblings, [](const NodeAndPath &np) {
      return np.second.fileName();
    }).join("\n    ")));
    if (reply == QMessageBox::Yes)
      filesToRemove << siblings;
  }

  for (const auto &file : qAsConst(filesToRemove)) {
    // Nodes can become invalid if the project was re-parsed while the dialog was open
    if (!ProjectTree::hasNode(file.first)) {
      QMessageBox::warning(ICore::dialogParent(), tr("Removing File Failed"), tr("File \"%1\" was not removed, because the project has changed " "in the meantime.\nPlease try again.").arg(file.second.toUserOutput()));
      return;
    }

    // remove from project
    const auto folderNode = file.first->asFileNode()->parentFolderNode();
    QTC_ASSERT(folderNode, return);

    const auto &currentFilePath = file.second;
    const auto status = folderNode->removeFiles({currentFilePath});
    const auto success = status == RemovedFilesFromProject::Ok || (status == RemovedFilesFromProject::Wildcard && removeFileDialog.isDeleteFileChecked());
    if (!success) {
      TaskHub::addTask(BuildSystemTask(Task::Error, tr("Could not remove file \"%1\" from project \"%2\".").arg(currentFilePath.toUserOutput(), folderNode->managingProject()->displayName()), folderNode->managingProject()->filePath()));
    }
  }

  std::vector<std::unique_ptr<FileChangeBlocker>> changeGuards;
  FilePaths pathList;
  for (const auto &file : qAsConst(filesToRemove)) {
    pathList << file.second;
    changeGuards.emplace_back(std::make_unique<FileChangeBlocker>(file.second));
  }

  Core::FileUtils::removeFiles(pathList, deleteFile);
}

static auto canTryToRenameIncludeGuards(const Node *node) -> HandleIncludeGuards
{
  return node->asFileNode() && node->asFileNode()->fileType() == FileType::Header ? HandleIncludeGuards::Yes : HandleIncludeGuards::No;
}

auto ProjectExplorerPluginPrivate::duplicateFile() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode && currentNode->asFileNode(), return);

  ProjectTree::CurrentNodeKeeper nodeKeeper;

  const auto fileNode = currentNode->asFileNode();
  const auto filePath = currentNode->filePath().toString();
  const QFileInfo sourceFileInfo(filePath);
  const auto baseName = sourceFileInfo.baseName();

  auto newFileName = sourceFileInfo.fileName();
  const int copyTokenIndex = newFileName.lastIndexOf(baseName) + baseName.length();
  newFileName.insert(copyTokenIndex, tr("_copy"));

  bool okPressed;
  newFileName = QInputDialog::getText(ICore::dialogParent(), tr("Choose File Name"), tr("New file name:"), QLineEdit::Normal, newFileName, &okPressed);
  if (!okPressed)
    return;
  if (!ProjectTree::hasNode(currentNode))
    return;

  const QString newFilePath = sourceFileInfo.path() + '/' + newFileName;
  const auto folderNode = fileNode->parentFolderNode();
  QTC_ASSERT(folderNode, return);
  QFile sourceFile(filePath);
  if (!sourceFile.copy(newFilePath)) {
    QMessageBox::critical(ICore::dialogParent(), tr("Duplicating File Failed"), tr("Failed to copy file \"%1\" to \"%2\": %3.").arg(QDir::toNativeSeparators(filePath), QDir::toNativeSeparators(newFilePath), sourceFile.errorString()));
    return;
  }
  Core::FileUtils::updateHeaderFileGuardIfApplicable(currentNode->filePath(), FilePath::fromString(newFilePath), canTryToRenameIncludeGuards(currentNode));
  if (!folderNode->addFiles({FilePath::fromString(newFilePath)})) {
    QMessageBox::critical(ICore::dialogParent(), tr("Duplicating File Failed"), tr("Failed to add new file \"%1\" to the project.").arg(QDir::toNativeSeparators(newFilePath)));
  }
}

auto ProjectExplorerPluginPrivate::deleteFile() -> void
{
  const auto currentNode = ProjectTree::currentNode();
  QTC_ASSERT(currentNode && currentNode->asFileNode(), return);

  ProjectTree::CurrentNodeKeeper nodeKeeper;

  const auto fileNode = currentNode->asFileNode();

  auto filePath = currentNode->filePath();
  const auto button = QMessageBox::question(ICore::dialogParent(), tr("Delete File"), tr("Delete %1 from file system?").arg(filePath.toUserOutput()), QMessageBox::Yes | QMessageBox::No);
  if (button != QMessageBox::Yes)
    return;

  const auto folderNode = fileNode->parentFolderNode();
  QTC_ASSERT(folderNode, return);

  folderNode->deleteFiles({filePath});

  FileChangeBlocker changeGuard(currentNode->filePath());
  if (const auto vc = VcsManager::findVersionControlForDirectory(filePath.absolutePath()))
    vc->vcsDelete(filePath);

  if (filePath.exists()) {
    if (!filePath.removeFile())
      QMessageBox::warning(ICore::dialogParent(), tr("Deleting File Failed"), tr("Could not delete file %1.").arg(filePath.toUserOutput()));
  }
}

auto ProjectExplorerPluginPrivate::handleRenameFile() -> void
{
  auto focusWidget = QApplication::focusWidget();
  while (focusWidget) {
    const auto treeWidget = qobject_cast<ProjectTreeWidget*>(focusWidget);
    if (treeWidget) {
      treeWidget->editCurrentItem();
      return;
    }
    focusWidget = focusWidget->parentWidget();
  }
}

auto ProjectExplorerPlugin::renameFile(Node *node, const QString &newFileName) -> void
{
  const auto oldFilePath = node->filePath().absoluteFilePath();
  const auto folderNode = node->parentFolderNode();
  QTC_ASSERT(folderNode, return);
  const auto projectFileName = folderNode->managingProject()->filePath().fileName();

  const auto newFilePath = FilePath::fromString(newFileName);

  if (oldFilePath == newFilePath)
    return;

  const auto handleGuards = canTryToRenameIncludeGuards(node);
  if (!folderNode->canRenameFile(oldFilePath, newFilePath)) {
    QTimer::singleShot(0, [oldFilePath, newFilePath, projectFileName, handleGuards] {
      const int res = QMessageBox::question(ICore::dialogParent(), tr("Project Editing Failed"), tr("The project file %1 cannot be automatically changed.\n\n" "Rename %2 to %3 anyway?").arg(projectFileName).arg(oldFilePath.toUserOutput()).arg(newFilePath.toUserOutput()));
      if (res == QMessageBox::Yes) {
        QTC_CHECK(Core::FileUtils::renameFile(oldFilePath, newFilePath, handleGuards));
      }
    });
    return;
  }

  if (Core::FileUtils::renameFile(oldFilePath, newFilePath, handleGuards)) {
    // Tell the project plugin about rename
    if (!folderNode->renameFile(oldFilePath, newFilePath)) {
      const auto renameFileError = tr("The file %1 was renamed to %2, but the project " "file %3 could not be automatically changed.").arg(oldFilePath.toUserOutput()).arg(newFilePath.toUserOutput()).arg(projectFileName);

      QTimer::singleShot(0, [renameFileError]() {
        QMessageBox::warning(ICore::dialogParent(), tr("Project Editing Failed"), renameFileError);
      });
    }
  } else {
    const auto renameFileError = tr("The file %1 could not be renamed %2.").arg(oldFilePath.toUserOutput()).arg(newFilePath.toUserOutput());

    QTimer::singleShot(0, [renameFileError]() {
      QMessageBox::warning(ICore::dialogParent(), tr("Cannot Rename File"), renameFileError);
    });
  }
}

auto ProjectExplorerPluginPrivate::handleSetStartupProject() -> void
{
  setStartupProject(ProjectTree::currentProject());
}

auto ProjectExplorerPluginPrivate::updateSessionMenu() -> void
{
  m_sessionMenu->clear();
  dd->m_sessionMenu->addAction(dd->m_sessionManagerAction);
  dd->m_sessionMenu->addSeparator();
  auto *ag = new QActionGroup(m_sessionMenu);
  connect(ag, &QActionGroup::triggered, this, &ProjectExplorerPluginPrivate::setSession);
  const auto activeSession = SessionManager::activeSession();

  const auto sessions = SessionManager::sessions();
  for (auto i = 0; i < sessions.size(); ++i) {
    const auto &session = sessions[i];

    const auto actionText = ActionManager::withNumberAccelerator(quoteAmpersands(session), i + 1);
    const auto act = ag->addAction(actionText);
    act->setData(session);
    act->setCheckable(true);
    if (session == activeSession)
      act->setChecked(true);
  }
  m_sessionMenu->addActions(ag->actions());
  m_sessionMenu->setEnabled(true);
}

auto ProjectExplorerPluginPrivate::setSession(QAction *action) -> void
{
  const auto session = action->data().toString();
  if (session != SessionManager::activeSession())
    SessionManager::loadSession(session);
}

auto ProjectExplorerPlugin::setProjectExplorerSettings(const ProjectExplorerSettings &pes) -> void
{
  QTC_ASSERT(dd->m_projectExplorerSettings.environmentId == pes.environmentId, return);

  if (dd->m_projectExplorerSettings == pes)
    return;
  dd->m_projectExplorerSettings = pes;
  emit m_instance->settingsChanged();
}

auto ProjectExplorerPlugin::projectExplorerSettings() -> const ProjectExplorerSettings&
{
  return dd->m_projectExplorerSettings;
}

auto ProjectExplorerPlugin::setAppOutputSettings(const AppOutputSettings &settings) -> void
{
  dd->m_outputPane.setSettings(settings);
}

auto ProjectExplorerPlugin::appOutputSettings() -> const AppOutputSettings&
{
  return dd->m_outputPane.settings();
}

auto ProjectExplorerPlugin::buildPropertiesSettings() -> BuildPropertiesSettings&
{
  return dd->m_buildPropertiesSettings;
}

auto ProjectExplorerPlugin::showQtSettings() -> void
{
  dd->m_buildPropertiesSettings.showQtSettings.setValue(true);
}

auto ProjectExplorerPlugin::setCustomParsers(const QList<CustomParserSettings> &settings) -> void
{
  if (dd->m_customParsers != settings) {
    dd->m_customParsers = settings;
    emit m_instance->customParsersChanged();
  }
}

auto ProjectExplorerPlugin::addCustomParser(const CustomParserSettings &settings) -> void
{
  QTC_ASSERT(settings.id.isValid(), return);
  QTC_ASSERT(!contains(dd->m_customParsers, [&settings](const CustomParserSettings &s) { return s.id == settings.id; }), return);

  dd->m_customParsers << settings;
  emit m_instance->customParsersChanged();
}

auto ProjectExplorerPlugin::removeCustomParser(Id id) -> void
{
  Utils::erase(dd->m_customParsers, [id](const CustomParserSettings &s) {
    return s.id == id;
  });
  emit m_instance->customParsersChanged();
}

auto ProjectExplorerPlugin::customParsers() -> const QList<CustomParserSettings>
{
  return dd->m_customParsers;
}

auto ProjectExplorerPlugin::projectFilePatterns() -> QStringList
{
  QStringList patterns;
  for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
    auto mt = mimeTypeForName(it.key());
    if (mt.isValid())
      patterns.append(mt.globPatterns());
  }
  return patterns;
}

auto ProjectExplorerPlugin::isProjectFile(const FilePath &filePath) -> bool
{
  const auto mt = mimeTypeForFile(filePath);
  for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
    if (mt.inherits(it.key()))
      return true;
  }
  return false;
}

auto ProjectExplorerPlugin::openOpenProjectDialog() -> void
{
  const auto path = DocumentManager::useProjectsDirectory() ? DocumentManager::projectsDirectory() : FilePath();
  const auto files = DocumentManager::getOpenFileNames(dd->m_projectFilterString, path);
  if (!files.isEmpty())
    ICore::openFiles(files, ICore::SwitchMode);
}

/*!
    Returns the current build directory template.

    \sa setBuildDirectoryTemplate
*/
auto ProjectExplorerPlugin::buildDirectoryTemplate() -> QString
{
  return dd->m_buildPropertiesSettings.buildDirectoryTemplate.value();
}

auto ProjectExplorerPlugin::defaultBuildDirectoryTemplate() -> QString
{
  return dd->m_buildPropertiesSettings.defaultBuildDirectoryTemplate();
}

auto ProjectExplorerPlugin::updateActions() -> void
{
  dd->updateActions();
}

auto ProjectExplorerPlugin::activateProjectPanel(Id panelId) -> void
{
  ModeManager::activateMode(Constants::MODE_SESSION);
  dd->m_proWindow->activateProjectPanel(panelId);
}

auto ProjectExplorerPlugin::clearRecentProjects() -> void
{
  dd->clearRecentProjects();
}

auto ProjectExplorerPlugin::removeFromRecentProjects(const QString &fileName, const QString &displayName) -> void
{
  dd->removeFromRecentProjects(fileName, displayName);
}

auto ProjectExplorerPlugin::updateRunActions() -> void
{
  dd->doUpdateRunActions();
}

auto ProjectExplorerPlugin::buildSystemOutput() -> OutputWindow*
{
  return dd->m_proWindow->buildSystemOutput();
}

auto ProjectExplorerPlugin::recentProjects() -> QList<QPair<QString, QString>>
{
  return dd->recentProjects();
}

auto ProjectManager::registerProjectCreator(const QString &mimeType, const std::function<Project *(const FilePath &)> &creator) -> void
{
  dd->m_projectCreators[mimeType] = creator;
}

auto ProjectManager::openProject(const MimeType &mt, const FilePath &fileName) -> Project*
{
  if (mt.isValid()) {
    for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
      if (mt.matchesName(it.key()))
        return it.value()(fileName);
    }
  }
  return nullptr;
}

auto ProjectManager::canOpenProjectForMimeType(const MimeType &mt) -> bool
{
  if (mt.isValid()) {
    for (auto it = dd->m_projectCreators.cbegin(); it != dd->m_projectCreators.cend(); ++it) {
      if (mt.matchesName(it.key()))
        return true;
    }
  }
  return false;
}

AllProjectFilesFilter::AllProjectFilesFilter() : DirectoryFilter("Files in All Project Directories")
{
  setDisplayName(id().toString());
  // shared with "Files in Any Project":
  setDefaultShortcutString("a");
  setDefaultIncludedByDefault(false); // but not included in default
  setFilters({});
  setIsCustomFilter(false);
  setDescription(ProjectExplorerPluginPrivate::tr("Matches all files from all project directories. Append \"+<number>\" or " "\":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
}

const char kDirectoriesKey[] = "directories";
const char kFilesKey[] = "files";

auto AllProjectFilesFilter::saveState(QJsonObject &object) const -> void
{
  DirectoryFilter::saveState(object);
  // do not save the directories, they are automatically managed
  object.remove(kDirectoriesKey);
  object.remove(kFilesKey);
}

auto AllProjectFilesFilter::restoreState(const QJsonObject &object) -> void
{
  // do not restore the directories (from saved settings from Qt Creator <= 5,
  // they are automatically managed
  auto withoutDirectories = object;
  withoutDirectories.remove(kDirectoriesKey);
  withoutDirectories.remove(kFilesKey);
  DirectoryFilter::restoreState(withoutDirectories);
}

} // namespace ProjectExplorer
