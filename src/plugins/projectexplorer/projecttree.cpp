// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projecttree.hpp"

#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "projectnodes.hpp"
#include "projecttreewidget.hpp"
#include "session.hpp"
#include "target.hpp"

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/documentmanager.hpp>
#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>
#include <core/modemanager.hpp>
#include <core/navigationwidget.hpp>
#include <core/vcsmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/infobar.hpp>
#include <utils/qtcassert.hpp>

#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>

namespace {
constexpr char EXTERNAL_FILE_WARNING[] = "ExternalFile";
}

using namespace Utils;

namespace ProjectExplorer {

using namespace Internal;

ProjectTree *ProjectTree::s_instance = nullptr;

ProjectTree::ProjectTree(QObject *parent) : QObject(parent)
{
  s_instance = this;

  connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, this, &ProjectTree::update);

  connect(qApp, &QApplication::focusChanged, this, &ProjectTree::update);

  connect(SessionManager::instance(), &SessionManager::projectAdded, this, &ProjectTree::sessionAndTreeChanged);
  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, &ProjectTree::sessionAndTreeChanged);
  connect(SessionManager::instance(), &SessionManager::startupProjectChanged, this, &ProjectTree::sessionChanged);
  connect(this, &ProjectTree::subtreeChanged, this, &ProjectTree::treeChanged);
}

ProjectTree::~ProjectTree()
{
  QTC_ASSERT(s_instance == this, return);
  s_instance = nullptr;
}

auto ProjectTree::aboutToShutDown() -> void
{
  disconnect(qApp, &QApplication::focusChanged, s_instance, &ProjectTree::update);
  s_instance->setCurrent(nullptr, nullptr);
  qDeleteAll(s_instance->m_projectTreeWidgets);
  QTC_CHECK(s_instance->m_projectTreeWidgets.isEmpty());
}

auto ProjectTree::instance() -> ProjectTree*
{
  return s_instance;
}

auto ProjectTree::currentProject() -> Project*
{
  return s_instance->m_currentProject;
}

auto ProjectTree::currentTarget() -> Target*
{
  const auto p = currentProject();
  return p ? p->activeTarget() : nullptr;
}

auto ProjectTree::currentBuildSystem() -> BuildSystem*
{
  const auto t = currentTarget();
  return t ? t->buildSystem() : nullptr;
}

auto ProjectTree::currentNode() -> Node*
{
  s_instance->update();
  return s_instance->m_currentNode;
}

auto ProjectTree::currentFilePath() -> FilePath
{
  const auto node = currentNode();
  return node ? node->filePath() : FilePath();
}

auto ProjectTree::registerWidget(ProjectTreeWidget *widget) -> void
{
  s_instance->m_projectTreeWidgets.append(widget);
  if (hasFocus(widget))
    s_instance->updateFromProjectTreeWidget(widget);
}

auto ProjectTree::unregisterWidget(ProjectTreeWidget *widget) -> void
{
  s_instance->m_projectTreeWidgets.removeOne(widget);
  if (hasFocus(widget))
    s_instance->updateFromDocumentManager();
}

auto ProjectTree::nodeChanged(ProjectTreeWidget *widget) -> void
{
  if (hasFocus(widget))
    s_instance->updateFromProjectTreeWidget(widget);
}

auto ProjectTree::update() -> void
{
  auto focus = m_focusForContextMenu;
  if (!focus)
    focus = currentWidget();

  if (focus)
    updateFromProjectTreeWidget(focus);
  else
    updateFromDocumentManager();
}

auto ProjectTree::updateFromProjectTreeWidget(ProjectTreeWidget *widget) -> void
{
  const auto currentNode = widget->currentNode();
  const auto project = projectForNode(currentNode);

  if (!project)
    updateFromNode(nullptr); // Project was removed!
  else
    setCurrent(currentNode, project);
}

auto ProjectTree::updateFromDocumentManager() -> void
{
  if (const auto document = Core::EditorManager::currentDocument()) {
    const auto fileName = document->filePath();
    updateFromNode(ProjectTreeWidget::nodeForFile(fileName));
  } else {
    updateFromNode(nullptr);
  }
}

auto ProjectTree::updateFromNode(Node *node) -> void
{
  Project *project;
  if (node)
    project = projectForNode(node);
  else
    project = SessionManager::startupProject();

  setCurrent(node, project);
  foreach(ProjectTreeWidget *widget, m_projectTreeWidgets)
    widget->sync(node);
}

auto ProjectTree::setCurrent(Node *node, Project *project) -> void
{
  const auto changedProject = project != m_currentProject;
  if (changedProject) {
    if (m_currentProject) {
      disconnect(m_currentProject, &Project::projectLanguagesUpdated, this, &ProjectTree::updateContext);
    }

    m_currentProject = project;

    if (m_currentProject) {
      connect(m_currentProject, &Project::projectLanguagesUpdated, this, &ProjectTree::updateContext);
    }
  }

  if (const auto document = Core::EditorManager::currentDocument()) {
    if (node) {
      disconnect(document, &Core::IDocument::changed, this, &ProjectTree::updateExternalFileWarning);
      document->infoBar()->removeInfo(EXTERNAL_FILE_WARNING);
    } else {
      connect(document, &Core::IDocument::changed, this, &ProjectTree::updateExternalFileWarning, Qt::UniqueConnection);
    }
  }

  if (node != m_currentNode) {
    m_currentNode = node;
    emit currentNodeChanged(node);
  }

  if (changedProject) {
    emit currentProjectChanged(m_currentProject);
    sessionChanged();
    updateContext();
  }
}

auto ProjectTree::sessionChanged() -> void
{
  if (m_currentProject) {
    Core::DocumentManager::setDefaultLocationForNewFiles(m_currentProject->projectDirectory());
  } else if (const auto project = SessionManager::startupProject()) {
    Core::DocumentManager::setDefaultLocationForNewFiles(project->projectDirectory());
    updateFromNode(nullptr); // Make startup project current if there is no other current
  } else {
    Core::DocumentManager::setDefaultLocationForNewFiles({});
  }
  update();
}

auto ProjectTree::updateContext() -> void
{
  Core::Context oldContext;
  oldContext.add(m_lastProjectContext);

  Core::Context newContext;
  if (m_currentProject) {
    newContext.add(m_currentProject->projectContext());
    newContext.add(m_currentProject->projectLanguages());

    m_lastProjectContext = newContext;
  } else {
    m_lastProjectContext = Core::Context();
  }

  Core::ICore::updateAdditionalContexts(oldContext, newContext);
}

auto ProjectTree::emitSubtreeChanged(FolderNode *node) -> void
{
  if (hasNode(node)) emit s_instance->subtreeChanged(node);
}

auto ProjectTree::sessionAndTreeChanged() -> void
{
  sessionChanged();
  emit treeChanged();
}

auto ProjectTree::expandCurrentNodeRecursively() -> void
{
  if (const auto w = currentWidget())
    w->expandCurrentNodeRecursively();
}

auto ProjectTree::collapseAll() -> void
{
  if (const auto w = currentWidget())
    w->collapseAll();
}

auto ProjectTree::expandAll() -> void
{
  if (const auto w = currentWidget())
    w->expandAll();
}

auto ProjectTree::changeProjectRootDirectory() -> void
{
  if (m_currentProject)
    m_currentProject->changeRootProjectDirectory();
}

auto ProjectTree::updateExternalFileWarning() -> void
{
  const auto document = qobject_cast<Core::IDocument*>(sender());
  if (!document || document->filePath().isEmpty())
    return;
  const auto infoBar = document->infoBar();
  const Id externalFileId(EXTERNAL_FILE_WARNING);
  if (!document->isModified()) {
    infoBar->removeInfo(externalFileId);
    return;
  }
  if (!infoBar->canInfoBeAdded(externalFileId))
    return;
  const auto fileName = document->filePath();
  const auto projects = SessionManager::projects();
  if (projects.isEmpty())
    return;
  for (const auto project : projects) {
    auto projectDir = project->projectDirectory();
    if (projectDir.isEmpty())
      continue;
    if (fileName.isChildOf(projectDir))
      return;
    // External file. Test if it under the same VCS
    QString topLevel;
    if (Core::VcsManager::findVersionControlForDirectory(projectDir, &topLevel) && fileName.isChildOf(FilePath::fromString(topLevel))) {
      return;
    }
  }
  infoBar->addInfo(InfoBarEntry(externalFileId, tr("<b>Warning:</b> This file is outside the project directory."), InfoBarEntry::GlobalSuppression::Enabled));
}

auto ProjectTree::hasFocus(ProjectTreeWidget *widget) -> bool
{
  return widget && ((widget->focusWidget() && widget->focusWidget()->hasFocus()) || s_instance->m_focusForContextMenu == widget);
}

auto ProjectTree::currentWidget() const -> ProjectTreeWidget*
{
  return findOrDefault(m_projectTreeWidgets, &ProjectTree::hasFocus);
}

auto ProjectTree::showContextMenu(ProjectTreeWidget *focus, const QPoint &globalPos, Node *node) -> void
{
  QMenu *contextMenu = nullptr;
  emit s_instance->aboutToShowContextMenu(node);

  if (!node) {
    contextMenu = Core::ActionManager::actionContainer(Constants::M_SESSIONCONTEXT)->menu();
  } else if (node->isProjectNodeType()) {
    if ((node->parentFolderNode() && node->parentFolderNode()->asContainerNode()) || node->asContainerNode())
      contextMenu = Core::ActionManager::actionContainer(Constants::M_PROJECTCONTEXT)->menu();
    else
      contextMenu = Core::ActionManager::actionContainer(Constants::M_SUBPROJECTCONTEXT)->menu();
  } else if (node->isVirtualFolderType() || node->isFolderNodeType()) {
    contextMenu = Core::ActionManager::actionContainer(Constants::M_FOLDERCONTEXT)->menu();
  } else if (node->asFileNode()) {
    contextMenu = Core::ActionManager::actionContainer(Constants::M_FILECONTEXT)->menu();
  }

  if (contextMenu && !contextMenu->actions().isEmpty()) {
    s_instance->m_focusForContextMenu = focus;
    contextMenu->popup(globalPos);
    connect(contextMenu, &QMenu::aboutToHide, s_instance, &ProjectTree::hideContextMenu, Qt::ConnectionType(Qt::UniqueConnection | Qt::QueuedConnection));
  }
}

auto ProjectTree::highlightProject(Project *project, const QString &message) -> void
{
  Core::ModeManager::activateMode(Core::Constants::MODE_EDIT);

  // Shows and focusses a project tree
  const auto widget = Core::NavigationWidget::activateSubWidget(Constants::PROJECTTREE_ID, Core::Side::Left);

  if (auto *projectTreeWidget = qobject_cast<ProjectTreeWidget*>(widget))
    projectTreeWidget->showMessage(project->rootProjectNode(), message);
}

/*!
    Registers the function \a treeChange to be run on a (sub tree of the)
    project tree when it is created. The function must be thread-safe, and
    applying the function on the same tree a second time must be a no-op.
*/
auto ProjectTree::registerTreeManager(const TreeManagerFunction &treeChange) -> void
{
  if (treeChange)
    s_instance->m_treeManagers.append(treeChange);
}

auto ProjectTree::applyTreeManager(FolderNode *folder, ConstructionPhase phase) -> void
{
  if (!folder)
    return;

  for (auto &f : s_instance->m_treeManagers)
    f(folder, phase);
}

auto ProjectTree::hasNode(const Node *node) -> bool
{
  return contains(SessionManager::projects(), [node](const Project *p) {
    if (!p)
      return false;
    if (p->containerNode() == node)
      return true;
    // When parsing fails we have a living container node but no rootProjectNode.
    const auto pn = p->rootProjectNode();
    if (!pn)
      return false;
    return pn->findNode([node](const Node *n) { return n == node; }) != nullptr;
  });
}

auto ProjectTree::forEachNode(const std::function<void(Node *)> &task) -> void
{
  const auto projects = SessionManager::projects();
  for (const auto project : projects) {
    if (const auto projectNode = project->rootProjectNode()) {
      task(projectNode);
      projectNode->forEachGenericNode(task);
    }
  }
}

auto ProjectTree::projectForNode(const Node *node) -> Project*
{
  if (!node)
    return nullptr;

  auto folder = node->asFolderNode();
  if (!folder)
    folder = node->parentFolderNode();

  while (folder && folder->parentFolderNode())
    folder = folder->parentFolderNode();

  return findOrDefault(SessionManager::projects(), [folder](const Project *pro) {
    return pro->containerNode() == folder;
  });
}

auto ProjectTree::nodeForFile(const FilePath &fileName) -> Node*
{
  Node *node = nullptr;
  for (const Project *project : SessionManager::projects()) {
    project->nodeForFilePath(fileName, [&](const Node *n) {
      if (!node || (!node->asFileNode() && n->asFileNode()))
        node = const_cast<Node*>(n);
      return false;
    });
    // early return:
    if (node && node->asFileNode())
      return node;
  }
  return node;
}

auto ProjectTree::siblingsWithSameBaseName(const Node *fileNode) -> const QList<Node*>
{
  auto productNode = fileNode->parentProjectNode();
  while (productNode && !productNode->isProduct())
    productNode = productNode->parentProjectNode();
  if (!productNode)
    return {};
  const auto fi = fileNode->filePath().toFileInfo();
  const auto filter = [&fi](const Node *n) {
    return n->asFileNode() && n->filePath().toFileInfo().dir() == fi.dir() && n->filePath().completeBaseName() == fi.completeBaseName() && n->filePath().toString() != fi.filePath();
  };
  return productNode->findNodes(filter);
}

auto ProjectTree::hideContextMenu() -> void
{
  if (m_keepCurrentNodeRequests == 0)
    m_focusForContextMenu = nullptr;
}

ProjectTree::CurrentNodeKeeper::CurrentNodeKeeper() : m_active(instance()->m_focusForContextMenu)
{
  if (m_active)
    ++instance()->m_keepCurrentNodeRequests;
}

ProjectTree::CurrentNodeKeeper::~CurrentNodeKeeper()
{
  if (m_active && --instance()->m_keepCurrentNodeRequests == 0) {
    instance()->m_focusForContextMenu = nullptr;
    instance()->update();
  }
}

} // namespace ProjectExplorer
