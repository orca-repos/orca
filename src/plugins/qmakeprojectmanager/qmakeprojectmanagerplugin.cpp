// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakeprojectmanagerplugin.hpp"

#include "addlibrarywizard.hpp"
#include "profileeditor.hpp"
#include "qmakenodes.hpp"
#include "qmakesettings.hpp"
#include "qmakestep.hpp"
#include "qmakemakestep.hpp"
#include "qmakebuildconfiguration.hpp"
#include "wizards/subdirsprojectwizard.hpp"
#include "customwidgetwizard/customwidgetwizard.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakeproject.hpp"
#include "externaleditors.hpp"
#include "qmakekitinformation.hpp"
#include "profilehighlighter.hpp"

#include <core/core-interface.hpp>
#include <core/core-constants.hpp>
#include <core/core-action-manager.hpp>
#include <core/core-action-container.hpp>
#include <core/core-command.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>

#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/projectnodes.hpp>
#include <projectexplorer/projectmanager.hpp>
#include <projectexplorer/projecttree.hpp>
#include <projectexplorer/runcontrol.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorericons.hpp>

#include <texteditor/texteditor.hpp>
#include <texteditor/texteditoractionhandler.hpp>
#include <texteditor/texteditorconstants.hpp>

#include <utils/hostosinfo.hpp>
#include <utils/parameteraction.hpp>
#include <utils/utilsicons.hpp>

#ifdef WITH_TESTS
#    include <QTest>
#endif

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

class QmakeProjectManagerPluginPrivate : public QObject
{
public:
    ~QmakeProjectManagerPluginPrivate() override;

    auto projectChanged() -> void;
    auto activeTargetChanged() -> void;
    auto updateActions() -> void;
    auto updateRunQMakeAction() -> void;
    auto updateContextActions(Node *node) -> void;
    auto buildStateChanged(Project *pro) -> void;
    auto updateBuildFileAction() -> void;
    auto disableBuildFileMenus() -> void;
    auto enableBuildFileMenus(const FilePath &file) -> void;

    Orca::Plugin::Core::Context projectContext;

    CustomWizardMetaFactory<CustomQmakeProjectWizard>
        qmakeProjectWizard{"qmakeproject", IWizardFactory::ProjectWizard};

    QMakeStepFactory qmakeStepFactory;
    QmakeMakeStepFactory makeStepFactory;

    QmakeBuildConfigurationFactory buildConfigFactory;

    ProFileEditorFactory profileEditorFactory;

    QmakeSettingsPage settingsPage;

    ExternalQtEditor *m_designerEditor{ExternalQtEditor::createDesignerEditor()};
    ExternalQtEditor *m_linguistEditor{ExternalQtEditor::createLinguistEditor()};

    QmakeProject *m_previousStartupProject = nullptr;
    Target *m_previousTarget = nullptr;

    QAction *m_runQMakeAction = nullptr;
    QAction *m_runQMakeActionContextMenu = nullptr;
    ParameterAction *m_buildSubProjectContextMenu = nullptr;
    QAction *m_subProjectRebuildSeparator = nullptr;
    QAction *m_rebuildSubProjectContextMenu = nullptr;
    QAction *m_cleanSubProjectContextMenu = nullptr;
    QAction *m_buildFileContextMenu = nullptr;
    ParameterAction *m_buildSubProjectAction = nullptr;
    QAction *m_rebuildSubProjectAction = nullptr;
    QAction *m_cleanSubProjectAction = nullptr;
    ParameterAction *m_buildFileAction = nullptr;
    QAction *m_addLibraryAction = nullptr;
    QAction *m_addLibraryActionContextMenu = nullptr;

    QmakeKitAspect qmakeKitAspect;

    auto addLibrary() -> void;
    auto addLibraryContextMenu() -> void;
    auto runQMake() -> void;
    auto runQMakeContextMenu() -> void;

    auto buildSubDirContextMenu() -> void { handleSubDirContextMenu(QmakeBuildSystem::BUILD, false); }
    auto rebuildSubDirContextMenu() -> void { handleSubDirContextMenu(QmakeBuildSystem::REBUILD, false); }
    auto cleanSubDirContextMenu() -> void { handleSubDirContextMenu(QmakeBuildSystem::CLEAN, false); }
    auto buildFileContextMenu() -> void { handleSubDirContextMenu(QmakeBuildSystem::BUILD, true); }
    auto buildFile() -> void;

    auto handleSubDirContextMenu(QmakeBuildSystem::Action action, bool isFileBuild) -> void;
    auto addLibraryImpl(const FilePath &filePath, TextEditor::BaseTextEditor *editor) -> void;
    auto runQMakeImpl(Project *p, ProjectExplorer::Node *node) -> void;
};

QmakeProjectManagerPlugin::~QmakeProjectManagerPlugin()
{
    delete d;
}

auto QmakeProjectManagerPlugin::initialize(const QStringList &arguments, QString *errorMessage) -> bool
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)
    const Context projectContext(QmakeProjectManager::Constants::QMAKEPROJECT_ID);
    Context projectTreeContext(ProjectExplorer::Constants::C_PROJECT_TREE);

    d = new QmakeProjectManagerPluginPrivate;

    //create and register objects
    ProjectManager::registerProjectType<QmakeProject>(QmakeProjectManager::Constants::PROFILE_MIMETYPE);

    IWizardFactory::registerFactoryCreator([] {
        return QList<IWizardFactory *> {
            new SubdirsProjectWizard,
            new CustomWidgetWizard
        };
    });

    //menus
    auto mbuild =
            ActionManager::actionContainer(ProjectExplorer::Constants::M_BUILDPROJECT);
    auto mproject =
            ActionManager::actionContainer(ProjectExplorer::Constants::M_PROJECTCONTEXT);
    auto msubproject =
            ActionManager::actionContainer(ProjectExplorer::Constants::M_SUBPROJECTCONTEXT);
    auto mfile =
            ActionManager::actionContainer(ProjectExplorer::Constants::M_FILECONTEXT);

    //register actions
    Command *command = nullptr;

    d->m_buildSubProjectContextMenu = new ParameterAction(tr("Build"), tr("Build \"%1\""),
                                                              ParameterAction::AlwaysEnabled/*handled manually*/,
                                                              this);
    command = ActionManager::registerAction(d->m_buildSubProjectContextMenu, Constants::BUILDSUBDIRCONTEXTMENU, projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->m_buildSubProjectContextMenu->text());
    msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
    connect(d->m_buildSubProjectContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::buildSubDirContextMenu);

    d->m_runQMakeActionContextMenu = new QAction(tr("Run qmake"), this);
    command = ActionManager::registerAction(d->m_runQMakeActionContextMenu, Constants::RUNQMAKECONTEXTMENU, projectContext);
    command->setAttribute(Command::CA_Hide);
    mproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
    msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
    connect(d->m_runQMakeActionContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::runQMakeContextMenu);

    command = msubproject->addSeparator(projectContext, ProjectExplorer::Constants::G_PROJECT_BUILD,
                                        &d->m_subProjectRebuildSeparator);
    command->setAttribute(Command::CA_Hide);

    d->m_rebuildSubProjectContextMenu = new QAction(tr("Rebuild"), this);
    command = ActionManager::registerAction(
                d->m_rebuildSubProjectContextMenu, Constants::REBUILDSUBDIRCONTEXTMENU, projectContext);
    command->setAttribute(Command::CA_Hide);
    msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
    connect(d->m_rebuildSubProjectContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::rebuildSubDirContextMenu);

    d->m_cleanSubProjectContextMenu = new QAction(tr("Clean"), this);
    command = ActionManager::registerAction(
                d->m_cleanSubProjectContextMenu, Constants::CLEANSUBDIRCONTEXTMENU, projectContext);
    command->setAttribute(Command::CA_Hide);
    msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);
    connect(d->m_cleanSubProjectContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::cleanSubDirContextMenu);

    d->m_buildFileContextMenu = new QAction(tr("Build"), this);
    command = ActionManager::registerAction(d->m_buildFileContextMenu, Constants::BUILDFILECONTEXTMENU, projectContext);
    command->setAttribute(Command::CA_Hide);
    mfile->addAction(command, ProjectExplorer::Constants::G_FILE_OTHER);
    connect(d->m_buildFileContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::buildFileContextMenu);

    d->m_buildSubProjectAction = new ParameterAction(tr("Build &Subproject"), tr("Build &Subproject \"%1\""),
                                                         ParameterAction::AlwaysEnabled, this);
    command = ActionManager::registerAction(d->m_buildSubProjectAction, Constants::BUILDSUBDIR, projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->m_buildSubProjectAction->text());
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_SUBPROJECT);
    connect(d->m_buildSubProjectAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::buildSubDirContextMenu);

    d->m_runQMakeAction = new QAction(tr("Run qmake"), this);
    const Context globalcontext(Orca::Plugin::Core::C_GLOBAL);
    command = ActionManager::registerAction(d->m_runQMakeAction, Constants::RUNQMAKE, globalcontext);
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_BUILD);
    connect(d->m_runQMakeAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::runQMake);

    d->m_rebuildSubProjectAction = new QAction(ProjectExplorer::Icons::REBUILD.icon(), tr("Rebuild"), this);
    d->m_rebuildSubProjectAction->setWhatsThis(tr("Rebuild Subproject"));
    command = ActionManager::registerAction(d->m_rebuildSubProjectAction, Constants::REBUILDSUBDIR, projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->m_rebuildSubProjectAction->whatsThis());
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_SUBPROJECT);
    connect(d->m_rebuildSubProjectAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::rebuildSubDirContextMenu);

    d->m_cleanSubProjectAction = new QAction(Utils::Icons::CLEAN.icon(),tr("Clean"), this);
    d->m_cleanSubProjectAction->setWhatsThis(tr("Clean Subproject"));
    command = ActionManager::registerAction(d->m_cleanSubProjectAction, Constants::CLEANSUBDIR, projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->m_cleanSubProjectAction->whatsThis());
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_SUBPROJECT);
    connect(d->m_cleanSubProjectAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::cleanSubDirContextMenu);

    d->m_buildFileAction = new ParameterAction(tr("Build File"), tr("Build File \"%1\""),
                                                   ParameterAction::AlwaysEnabled, this);
    command = ActionManager::registerAction(d->m_buildFileAction, Constants::BUILDFILE, projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->m_buildFileAction->text());
    command->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+B")));
    mbuild->addAction(command, ProjectExplorer::Constants::G_BUILD_FILE);
    connect(d->m_buildFileAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::buildFile);

    connect(BuildManager::instance(), &BuildManager::buildStateChanged,
            d, &QmakeProjectManagerPluginPrivate::buildStateChanged);
    connect(SessionManager::instance(), &SessionManager::startupProjectChanged,
            d, &QmakeProjectManagerPluginPrivate::projectChanged);
    connect(ProjectTree::instance(), &ProjectTree::currentProjectChanged,
            d, &QmakeProjectManagerPluginPrivate::projectChanged);

    connect(ProjectTree::instance(), &ProjectTree::currentNodeChanged,
            d, &QmakeProjectManagerPluginPrivate::updateContextActions);

    auto contextMenu = ActionManager::createMenu(QmakeProjectManager::Constants::M_CONTEXT);

    auto proFileEditorContext = Context(QmakeProjectManager::Constants::PROFILE_EDITOR_ID);

    command = ActionManager::command(TextEditor::Constants::JUMP_TO_FILE_UNDER_CURSOR);
    contextMenu->addAction(command);

    d->m_addLibraryAction = new QAction(tr("Add Library..."), this);
    command = ActionManager::registerAction(d->m_addLibraryAction,
        Constants::ADDLIBRARY, proFileEditorContext);
    connect(d->m_addLibraryAction, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::addLibrary);
    contextMenu->addAction(command);

    d->m_addLibraryActionContextMenu = new QAction(tr("Add Library..."), this);
    command = ActionManager::registerAction(d->m_addLibraryActionContextMenu,
        Constants::ADDLIBRARY, projectTreeContext);
    connect(d->m_addLibraryActionContextMenu, &QAction::triggered,
            d, &QmakeProjectManagerPluginPrivate::addLibraryContextMenu);
    mproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_FILES);
    msubproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_FILES);

    contextMenu->addSeparator(proFileEditorContext);

    command = ActionManager::command(TextEditor::Constants::UN_COMMENT_SELECTION);
    contextMenu->addAction(command);

    connect(EditorManager::instance(), &EditorManager::currentEditorChanged,
            d, &QmakeProjectManagerPluginPrivate::updateBuildFileAction);

    d->updateActions();

    return true;
}

QmakeProjectManagerPluginPrivate::~QmakeProjectManagerPluginPrivate()
{
    delete m_designerEditor;
    delete m_linguistEditor;
}

auto QmakeProjectManagerPluginPrivate::projectChanged() -> void
{
    if (m_previousStartupProject)
        disconnect(m_previousStartupProject, &Project::activeTargetChanged,
                   this, &QmakeProjectManagerPluginPrivate::activeTargetChanged);

    if (ProjectTree::currentProject())
        m_previousStartupProject = qobject_cast<QmakeProject *>(ProjectTree::currentProject());
    else
        m_previousStartupProject = qobject_cast<QmakeProject *>(SessionManager::startupProject());

    if (m_previousStartupProject) {
        connect(m_previousStartupProject, &Project::activeTargetChanged,
                this, &QmakeProjectManagerPluginPrivate::activeTargetChanged);
    }

    activeTargetChanged();
}

static auto buildableFileProFile(Node *node) -> QmakeProFileNode*
{
    if (node) {
        auto subPriFileNode = dynamic_cast<QmakePriFileNode *>(node);
        if (!subPriFileNode)
            subPriFileNode = dynamic_cast<QmakePriFileNode *>(node->parentProjectNode());
        if (subPriFileNode)
            return subPriFileNode->proFileNode();
    }
    return nullptr;
}

auto QmakeProjectManagerPluginPrivate::addLibrary() -> void
{
    if (auto editor = qobject_cast<BaseTextEditor *>(Orca::Plugin::Core::EditorManager::currentEditor()))
        addLibraryImpl(editor->document()->filePath(), editor);
}

auto QmakeProjectManagerPluginPrivate::addLibraryContextMenu() -> void
{
    FilePath projectPath;

    auto node = ProjectTree::currentNode();
    if (auto cn = node->asContainerNode())
        projectPath = cn->project()->projectFilePath();
    else if (dynamic_cast<QmakeProFileNode *>(node))
        projectPath = node->filePath();

    addLibraryImpl(projectPath, nullptr);
}

auto QmakeProjectManagerPluginPrivate::addLibraryImpl(const FilePath &filePath, BaseTextEditor *editor) -> void
{
    if (filePath.isEmpty())
        return;

    Internal::AddLibraryWizard wizard(filePath, Orca::Plugin::Core::ICore::dialogParent());
    if (wizard.exec() != QDialog::Accepted)
        return;

    if (!editor)
        editor = qobject_cast<BaseTextEditor *>(Orca::Plugin::Core::EditorManager::openEditor(filePath,
            Constants::PROFILE_EDITOR_ID, Orca::Plugin::Core::EditorManager::DoNotMakeVisible));
    if (!editor)
        return;

    const auto endOfDoc = editor->position(EndOfDocPosition);
    editor->setCursorPosition(endOfDoc);
    auto snippet = wizard.snippet();

    // add extra \n in case the last line is not empty
    int line, column;
    editor->convertPosition(endOfDoc, &line, &column);
    const auto positionInBlock = column - 1;
    if (!editor->textAt(endOfDoc - positionInBlock, positionInBlock).simplified().isEmpty())
        snippet = QLatin1Char('\n') + snippet;

    editor->insert(snippet);
}

auto QmakeProjectManagerPluginPrivate::runQMake() -> void
{
    runQMakeImpl(SessionManager::startupProject(), nullptr);
}

auto QmakeProjectManagerPluginPrivate::runQMakeContextMenu() -> void
{
    runQMakeImpl(ProjectTree::currentProject(), ProjectTree::currentNode());
}

auto QmakeProjectManagerPluginPrivate::runQMakeImpl(Project *p, Node *node) -> void
{
    if (!ProjectExplorerPlugin::saveModifiedFiles())
        return;
    auto *qmakeProject = qobject_cast<QmakeProject *>(p);
    QTC_ASSERT(qmakeProject, return);

    if (!qmakeProject->activeTarget() || !qmakeProject->activeTarget()->activeBuildConfiguration())
        return;

    auto *bc = static_cast<QmakeBuildConfiguration *>(qmakeProject->activeTarget()->activeBuildConfiguration());
    auto qs = bc->qmakeStep();
    if (!qs)
        return;

    //found qmakeStep, now use it
    qs->setForced(true);

    if (node && node != qmakeProject->rootProjectNode())
        if (auto *profile = dynamic_cast<QmakeProFileNode *>(node))
            bc->setSubNodeBuild(profile);

    BuildManager::appendStep(qs, QmakeProjectManagerPlugin::tr("QMake"));
    bc->setSubNodeBuild(nullptr);
}

auto QmakeProjectManagerPluginPrivate::buildFile() -> void
{
  auto currentDocument = Orca::Plugin::Core::EditorManager::currentDocument();
    if (!currentDocument)
        return;

    const auto file = currentDocument->filePath();
  auto n = ProjectTree::nodeForFile(file);
  auto node  = n ? n->asFileNode() : nullptr;
    if (!node)
        return;
  auto project = SessionManager::projectForFile(file);
    if (!project)
        return;
  auto target = project->activeTarget();
    if (!target)
        return;

    if (auto bs = qobject_cast<QmakeBuildSystem *>(target->buildSystem()))
        bs->buildHelper(QmakeBuildSystem::BUILD, true, buildableFileProFile(node), node);
}

auto QmakeProjectManagerPluginPrivate::handleSubDirContextMenu(QmakeBuildSystem::Action action, bool isFileBuild) -> void
{
  auto node = ProjectTree::currentNode();

  auto subProjectNode = buildableFileProFile(node);
  auto fileNode = node ? node->asFileNode() : nullptr;
  auto buildFilePossible = subProjectNode && fileNode && fileNode->fileType() == FileType::Source;
  auto buildableFileNode = buildFilePossible ? fileNode : nullptr;

    if (auto bs = qobject_cast<QmakeBuildSystem *>(ProjectTree::currentBuildSystem()))
        bs->buildHelper(action, isFileBuild, subProjectNode, buildableFileNode);
}

auto QmakeProjectManagerPluginPrivate::activeTargetChanged() -> void
{
    if (m_previousTarget)
        disconnect(m_previousTarget, &Target::activeBuildConfigurationChanged,
                   this, &QmakeProjectManagerPluginPrivate::updateRunQMakeAction);

    m_previousTarget = m_previousStartupProject ? m_previousStartupProject->activeTarget() : nullptr;

    if (m_previousTarget) {
        connect(m_previousTarget, &Target::activeBuildConfigurationChanged,
                this, &QmakeProjectManagerPluginPrivate::updateRunQMakeAction);
        connect(m_previousTarget, &Target::parsingFinished,
                this, &QmakeProjectManagerPluginPrivate::updateActions);
    }

    updateRunQMakeAction();
}

auto QmakeProjectManagerPluginPrivate::updateActions() -> void
{
    updateRunQMakeAction();
    updateContextActions(ProjectTree::currentNode());
}

auto QmakeProjectManagerPluginPrivate::updateRunQMakeAction() -> void
{
  auto enable = true;
    if (BuildManager::isBuilding(m_previousStartupProject))
        enable = false;
    auto pro = qobject_cast<QmakeProject *>(m_previousStartupProject);
    m_runQMakeAction->setVisible(pro);
    if (!pro
            || !pro->rootProjectNode()
            || !pro->activeTarget()
            || !pro->activeTarget()->activeBuildConfiguration())
        enable = false;

    m_runQMakeAction->setEnabled(enable);
}

auto QmakeProjectManagerPluginPrivate::updateContextActions(Node *node) -> void
{
  auto project = ProjectTree::currentProject();

    const ContainerNode *containerNode = node ? node->asContainerNode() : nullptr;
    const auto *proFileNode = dynamic_cast<const QmakeProFileNode *>(containerNode ? containerNode->rootProjectNode() : node);

    m_addLibraryActionContextMenu->setEnabled(proFileNode);
    auto *qmakeProject = qobject_cast<QmakeProject *>(project);
    QmakeProFileNode *subProjectNode = nullptr;
    disableBuildFileMenus();
    if (node) {
        auto subPriFileNode = dynamic_cast<const QmakePriFileNode *>(node);
        if (!subPriFileNode)
            subPriFileNode = dynamic_cast<QmakePriFileNode *>(node->parentProjectNode());
        subProjectNode = subPriFileNode ? subPriFileNode->proFileNode() : nullptr;

        if (const FileNode *fileNode = node->asFileNode())
            enableBuildFileMenus(fileNode->filePath());
    }

  auto subProjectActionsVisible = false;
    if (qmakeProject && subProjectNode) {
        if (auto rootNode = qmakeProject->rootProjectNode())
            subProjectActionsVisible = subProjectNode != rootNode;
    }

    QString subProjectName;
    if (subProjectActionsVisible)
        subProjectName = subProjectNode->displayName();

    m_buildSubProjectAction->setParameter(subProjectName);
    m_buildSubProjectContextMenu->setParameter(proFileNode ? proFileNode->displayName() : QString());

    auto buildConfiguration = (qmakeProject && qmakeProject->activeTarget()) ?
                static_cast<QmakeBuildConfiguration *>(qmakeProject->activeTarget()->activeBuildConfiguration()) : nullptr;
  auto isProjectNode = qmakeProject && proFileNode && buildConfiguration;
  auto isBuilding = BuildManager::isBuilding(project);
  auto enabled = subProjectActionsVisible && !isBuilding;

    m_buildSubProjectAction->setVisible(subProjectActionsVisible);
    m_rebuildSubProjectAction->setVisible(subProjectActionsVisible);
    m_cleanSubProjectAction->setVisible(subProjectActionsVisible);
    m_buildSubProjectContextMenu->setVisible(subProjectActionsVisible && isProjectNode);
    m_subProjectRebuildSeparator->setVisible(subProjectActionsVisible && isProjectNode);
    m_rebuildSubProjectContextMenu->setVisible(subProjectActionsVisible && isProjectNode);
    m_cleanSubProjectContextMenu->setVisible(subProjectActionsVisible && isProjectNode);

    m_buildSubProjectAction->setEnabled(enabled);
    m_rebuildSubProjectAction->setEnabled(enabled);
    m_cleanSubProjectAction->setEnabled(enabled);
    m_buildSubProjectContextMenu->setEnabled(enabled && isProjectNode);
    m_rebuildSubProjectContextMenu->setEnabled(enabled && isProjectNode);
    m_cleanSubProjectContextMenu->setEnabled(enabled && isProjectNode);
    m_runQMakeActionContextMenu->setEnabled(isProjectNode && !isBuilding
                                            && buildConfiguration->qmakeStep());
}

auto QmakeProjectManagerPluginPrivate::buildStateChanged(Project *pro) -> void
{
    if (pro == ProjectTree::currentProject()) {
        updateRunQMakeAction();
        updateContextActions(ProjectTree::currentNode());
        updateBuildFileAction();
    }
}

auto QmakeProjectManagerPluginPrivate::updateBuildFileAction() -> void
{
    disableBuildFileMenus();
    if (auto currentDocument = EditorManager::currentDocument())
        enableBuildFileMenus(currentDocument->filePath());
}

auto QmakeProjectManagerPluginPrivate::disableBuildFileMenus() -> void
{
    m_buildFileAction->setVisible(false);
    m_buildFileAction->setEnabled(false);
    m_buildFileAction->setParameter(QString());
    m_buildFileContextMenu->setEnabled(false);
}

auto QmakeProjectManagerPluginPrivate::enableBuildFileMenus(const FilePath &file) -> void
{
  auto visible = false;
  auto enabled = false;

    if (auto node = ProjectTree::nodeForFile(file)) {
        if (auto project = SessionManager::projectForFile(file)) {
            if (const FileNode *fileNode = node->asFileNode()) {
                const auto type = fileNode->fileType();
                visible = qobject_cast<QmakeProject *>(project)
                        && dynamic_cast<QmakePriFileNode *>(node->parentProjectNode())
                        && (type == FileType::Source || type == FileType::Header);

                enabled = !BuildManager::isBuilding(project);
                m_buildFileAction->setParameter(file.fileName());
            }
        }
    }
    m_buildFileAction->setVisible(visible);
    m_buildFileAction->setEnabled(enabled);
    m_buildFileContextMenu->setEnabled(visible && enabled);
}

} // Internal
} // QmakeProjectManager
