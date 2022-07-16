// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectmodels.hpp"

#include "buildsystem.hpp"
#include "project.hpp"
#include "projectnodes.hpp"
#include "projectexplorer.hpp"
#include "projecttree.hpp"
#include "session.hpp"
#include "target.hpp"

#include <app/app_version.hpp>
#include <core/core-document-manager.hpp>
#include <core/core-file-icon-provider.hpp>
#include <core/core-interface.hpp>
#include <core/core-version-control-interface.hpp>
#include <core/core-vcs-manager.hpp>
#include <utils/utilsicons.hpp>
#include <utils/algorithm.hpp>
#include <utils/dropsupport.hpp>
#include <utils/pathchooser.hpp>
#include <utils/stringutils.hpp>
#include <utils/theme/theme.hpp>

#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QLoggingCategory>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <functional>
#include <iterator>
#include <tuple>
#include <vector>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

/// An output iterator whose assignment operator appends a clone of the operand to the list of
/// children of the WrapperNode passed to the constructor.
class Appender {
public:
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = void;
  using pointer = void;
  using reference = void;

  explicit Appender(WrapperNode *parent) : m_parent(parent) {}

  auto operator=(const WrapperNode *node) -> Appender&
  {
    if (node)
      m_parent->appendClone(*node);
    return *this;
  }

  auto operator*() -> Appender& { return *this; }
  auto operator++() -> Appender& { return *this; }
  auto operator++(int) -> Appender& { return *this; }

private:
  WrapperNode *m_parent;
};

auto compareNodes(const Node *n1, const Node *n2) -> bool
{
  if (n1->priority() > n2->priority())
    return true;
  if (n1->priority() < n2->priority())
    return false;

  const auto displayNameResult = caseFriendlyCompare(n1->displayName(), n2->displayName());
  if (displayNameResult != 0)
    return displayNameResult < 0;

  const auto filePathResult = caseFriendlyCompare(n1->filePath().toString(), n2->filePath().toString());
  return filePathResult < 0;
}

static auto sortWrapperNodes(const WrapperNode *w1, const WrapperNode *w2) -> bool
{
  return compareNodes(w1->m_node, w2->m_node);
}

/// Appends to `dest` clones of children of `first` and `second`, removing duplicates (recursively).
///
/// \param first, second
///   Nodes with children sorted by sortWrapperNodes.
/// \param dest
///   Node to which to append clones of children of `first` and `second`, with duplicates removed.
static auto appendMergedChildren(const WrapperNode *first, const WrapperNode *second, WrapperNode *dest) -> void
{
  setUnionMerge(first->begin(), first->end(), second->begin(), second->end(), Appender(dest), [dest](const WrapperNode *childOfFirst, const WrapperNode *childOfSecond) -> const WrapperNode* {
    if (childOfSecond->hasChildren()) {
      if (childOfFirst->hasChildren()) {
        const auto mergeResult = new WrapperNode(childOfFirst->m_node);
        dest->appendChild(mergeResult);
        appendMergedChildren(childOfFirst, childOfSecond, mergeResult);
        // mergeResult has already been appended to the parent's list of
        // children -- there's no need for the Appender to do it again.
        // That's why we return a null pointer.
        return nullptr;
      } else {
        return childOfSecond;
      }
    } else {
      return childOfFirst;
    }
  }, sortWrapperNodes);
}

/// Given a node `parent` with children sorted by the criteria defined in sortWrapperNodes(), merge
/// any children that are equal according to those criteria.
static auto mergeDuplicates(WrapperNode *parent) -> void
{
  // We assume all descendants of 'parent' are sorted
  auto childIndex = 0;
  while (childIndex + 1 < parent->childCount()) {
    const WrapperNode *child = parent->childAt(childIndex);
    const WrapperNode *nextChild = parent->childAt(childIndex + 1);
    Q_ASSERT_X(!sortWrapperNodes(nextChild, child), __func__, "Children are not sorted");
    if (!sortWrapperNodes(child, nextChild)) {
      // child and nextChild must have the same priorities, display names and folder paths.
      // Replace them by a single node 'mergeResult` containing the union of their children.
      const auto mergeResult = new WrapperNode(child->m_node);
      parent->insertChild(childIndex, mergeResult);
      appendMergedChildren(child, nextChild, mergeResult);
      // Now we can remove the original children
      parent->removeChildAt(childIndex + 2);
      parent->removeChildAt(childIndex + 1);
    } else {
      ++childIndex;
    }
  }
}

auto WrapperNode::appendClone(const WrapperNode &node) -> void
{
  const auto clone = new WrapperNode(node.m_node);
  appendChild(clone);
  for (const WrapperNode *child : node)
    clone->appendClone(*child);
}

FlatModel::FlatModel(QObject *parent) : TreeModel<WrapperNode, WrapperNode>(new WrapperNode(nullptr), parent)
{
  const auto tree = ProjectTree::instance();
  connect(tree, &ProjectTree::subtreeChanged, this, &FlatModel::updateSubtree);

  const auto sm = SessionManager::instance();
  connect(sm, &SessionManager::projectRemoved, this, &FlatModel::handleProjectRemoved);
  connect(sm, &SessionManager::aboutToLoadSession, this, &FlatModel::loadExpandData);
  connect(sm, &SessionManager::aboutToSaveSession, this, &FlatModel::saveExpandData);
  connect(sm, &SessionManager::projectAdded, this, &FlatModel::handleProjectAdded);
  connect(sm, &SessionManager::startupProjectChanged, this, [this] { emit layoutChanged(); });

  for (const auto project : SessionManager::projects())
    handleProjectAdded(project);
}

auto FlatModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const Node *const node = nodeForIndex(index);
  if (!node)
    return QVariant();

  const auto folderNode = node->asFolderNode();
  const auto containerNode = node->asContainerNode();
  const Project *const project = containerNode ? containerNode->project() : nullptr;
  const Target *const target = project ? project->activeTarget() : nullptr;
  const BuildSystem *const bs = target ? target->buildSystem() : nullptr;

  switch (role) {
  case Qt::DisplayRole:
    return node->displayName();
  case Qt::EditRole:
    return node->filePath().fileName();
  case Qt::ToolTipRole: {
    auto tooltip = node->tooltip();
    if (project) {
      if (target) {
        const auto projectIssues = toHtml(project->projectIssues(project->activeTarget()->kit()));
        if (!projectIssues.isEmpty())
          tooltip += "<p>" + projectIssues;
      } else {
        tooltip += "<p>" + tr("No kits are enabled for this project. " "Enable kits in the \"Projects\" mode.");
      }
    }
    return tooltip;
  }
  case Qt::DecorationRole: {
    if (!folderNode)
      return node->asFileNode()->icon();
    if (!project)
      return folderNode->icon();
    static auto warnIcon = Icons::WARNING.icon();
    static auto emptyIcon = Icons::EMPTY16.icon();
    if (project->needsConfiguration())
      return warnIcon;
    if (bs && bs->isParsing())
      return emptyIcon;
    if (!target || !project->projectIssues(target->kit()).isEmpty())
      return warnIcon;
    return containerNode->rootProjectNode() ? containerNode->rootProjectNode()->icon() : folderNode->icon();
  }
  case Qt::FontRole: {
    QFont font;
    if (project == SessionManager::startupProject())
      font.setBold(true);
    return font;
  }
  case Qt::ForegroundRole:
    return node->isEnabled() ? QVariant() : Utils::orcaTheme()->color(Theme::TextColorDisabled);
  case Project::FilePathRole:
    return node->filePath().toString();
  case Project::isParsingRole:
    return project && bs ? bs->isParsing() && !project->needsConfiguration() : false;
  }

  return QVariant();
}

auto FlatModel::flags(const QModelIndex &index) const -> Qt::ItemFlags
{
  if (!index.isValid())
    return {};
  // We claim that everything is editable
  // That's slightly wrong
  // We control the only view, and that one does the checks
  auto f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
  if (const auto node = nodeForIndex(index)) {
    if (!node->asProjectNode()) {
      // either folder or file node
      if (node->supportsAction(Rename, node))
        f = f | Qt::ItemIsEditable;
    } else if (node->supportsAction(AddExistingFile, node)) {
      f |= Qt::ItemIsDropEnabled;
    }
  }
  return f;
}

auto FlatModel::setData(const QModelIndex &index, const QVariant &value, int role) -> bool
{
  if (!index.isValid())
    return false;
  if (role != Qt::EditRole)
    return false;

  auto node = nodeForIndex(index);
  QTC_ASSERT(node, return false);

  std::vector<std::tuple<Node*, FilePath, FilePath>> toRename;
  const auto orgFilePath = node->filePath();
  const auto newFilePath = orgFilePath.parentDir().pathAppended(value.toString());
  const auto orgFileInfo = orgFilePath.toFileInfo();
  toRename.emplace_back(std::make_tuple(node, orgFilePath, newFilePath));

  // The base name of the file was changed. Go look for other files with the same base name
  // and offer to rename them as well.
  if (orgFilePath != newFilePath && orgFilePath.suffix() == newFilePath.suffix()) {
    const auto candidateNodes = ProjectTree::siblingsWithSameBaseName(node);
    if (!candidateNodes.isEmpty()) {
      auto fileNames = transform<QStringList>(candidateNodes, [](const Node *n) {
        return n->filePath().fileName();
      });
      fileNames.removeDuplicates();
      const auto reply = QMessageBox::question(ICore::dialogParent(), tr("Rename More Files?"), tr("Would you like to rename these files as well?\n    %1").arg(fileNames.join("\n    ")), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
      switch (reply) {
      case QMessageBox::Yes:
        for (const auto n : candidateNodes) {
          QString targetFilePath = orgFileInfo.absolutePath() + '/' + newFilePath.completeBaseName();
          const auto suffix = n->filePath().suffix();
          if (!suffix.isEmpty())
            targetFilePath.append('.').append(suffix);
          toRename.emplace_back(std::make_tuple(n, n->filePath(), FilePath::fromString(targetFilePath)));
        }
        break;
      case QMessageBox::Cancel:
        return false;
      default:
        break;
      }
    }
  }

  for (const auto &f : toRename) {
    ProjectExplorerPlugin::renameFile(std::get<0>(f), std::get<2>(f).toString());
    emit renamed(std::get<1>(f), std::get<2>(f));
  }
  return true;
}

static auto compareProjectNames(const WrapperNode *lhs, const WrapperNode *rhs) -> bool
{
  const auto p1 = lhs->m_node;
  const auto p2 = rhs->m_node;
  const auto displayNameResult = caseFriendlyCompare(p1->displayName(), p2->displayName());
  if (displayNameResult != 0)
    return displayNameResult < 0;
  return p1 < p2; // sort by pointer value
}

auto FlatModel::addOrRebuildProjectModel(Project *project) -> void
{
  auto container = nodeForProject(project);
  if (container) {
    container->removeChildren();
    project->containerNode()->removeAllChildren();
  } else {
    container = new WrapperNode(project->containerNode());
    rootItem()->insertOrderedChild(container, &compareProjectNames);
  }

  QSet<Node*> seen;

  if (const auto projectNode = project->rootProjectNode()) {
    addFolderNode(container, projectNode, &seen);
    if (m_trimEmptyDirectories)
      trimEmptyDirectories(container);
  }

  if (project->needsInitialExpansion())
    m_toExpand.insert(expandDataForNode(container->m_node));

  if (container->childCount() == 0) {
    auto projectFileNode = std::make_unique<FileNode>(project->projectFilePath(), FileType::Project);
    seen.insert(projectFileNode.get());
    container->appendChild(new WrapperNode(projectFileNode.get()));
    project->containerNode()->addNestedNode(std::move(projectFileNode));
  }

  container->sortChildren(&sortWrapperNodes);

  container->forAllChildren([this](WrapperNode *node) {
    if (node->m_node) {
      const auto path = node->m_node->filePath().toString();
      const auto displayName = node->m_node->displayName();
      const ExpandData ed(path, displayName);
      if (m_toExpand.contains(ed)) emit requestExpansion(node->index());
    } else {
      emit requestExpansion(node->index());
    }
  });

  const auto path = container->m_node->filePath().toString();
  const auto displayName = container->m_node->displayName();
  const ExpandData ed(path, displayName);
  if (m_toExpand.contains(ed)) emit requestExpansion(container->index());
}

auto FlatModel::parsingStateChanged(Project *project) -> void
{
  const WrapperNode *const node = nodeForProject(project);
  const auto nodeIdx = indexForNode(node->m_node);
  emit dataChanged(nodeIdx, nodeIdx);
}

auto FlatModel::updateSubtree(FolderNode *node) -> void
{
  // FIXME: This is still excessive, should be limited to the affected subtree.
  while (const auto parent = node->parentFolderNode())
    node = parent;
  if (const auto container = node->asContainerNode())
    addOrRebuildProjectModel(container->project());
}

auto FlatModel::rebuildModel() -> void
{
  const auto projects = SessionManager::projects();
  for (const auto project : projects)
    addOrRebuildProjectModel(project);
}

auto FlatModel::onCollapsed(const QModelIndex &idx) -> void
{
  m_toExpand.remove(expandDataForNode(nodeForIndex(idx)));
}

auto FlatModel::onExpanded(const QModelIndex &idx) -> void
{
  m_toExpand.insert(expandDataForNode(nodeForIndex(idx)));
}

auto FlatModel::expandDataForNode(const Node *node) const -> ExpandData
{
  QTC_ASSERT(node, return ExpandData());
  const auto path = node->filePath().toString();
  const auto displayName = node->displayName();
  return ExpandData(path, displayName);
}

auto FlatModel::handleProjectAdded(Project *project) -> void
{
  QTC_ASSERT(project, return);

  connect(project, &Project::anyParsingStarted, this, [this, project]() {
    if (nodeForProject(project))
      parsingStateChanged(project);
  });
  connect(project, &Project::anyParsingFinished, this, [this, project]() {
    if (nodeForProject(project))
      parsingStateChanged(project);
    emit ProjectTree::instance()->nodeActionsChanged();
  });
  addOrRebuildProjectModel(project);
}

auto FlatModel::handleProjectRemoved(Project *project) -> void
{
  destroyItem(nodeForProject(project));
}

auto FlatModel::nodeForProject(const Project *project) const -> WrapperNode*
{
  QTC_ASSERT(project, return nullptr);
  auto containerNode = project->containerNode();
  QTC_ASSERT(containerNode, return nullptr);
  return rootItem()->findFirstLevelChild([containerNode](WrapperNode *node) {
    return node->m_node == containerNode;
  });
}

auto FlatModel::loadExpandData() -> void
{
  const auto data = SessionManager::value("ProjectTree.ExpandData").value<QList<QVariant>>();
  m_toExpand = Utils::transform<QSet>(data, &ExpandData::fromSettings);
  m_toExpand.remove(ExpandData());
}

auto FlatModel::saveExpandData() -> void
{
  // TODO if there are multiple ProjectTreeWidgets, the last one saves the data
  const auto data = Utils::transform<QList>(m_toExpand, &ExpandData::toSettings);
  SessionManager::setValue(QLatin1String("ProjectTree.ExpandData"), data);
}

auto FlatModel::addFolderNode(WrapperNode *parent, FolderNode *folderNode, QSet<Node*> *seen) -> void
{
  auto hasHiddenSourcesOrHeaders = false;

  for (const auto node : folderNode->nodes()) {
    if (m_filterGeneratedFiles && node->isGenerated())
      continue;
    if (m_filterDisabledFiles && !node->isEnabled())
      continue;
    if (const auto subFolderNode = node->asFolderNode()) {
      auto isHidden = m_filterProjects && !subFolderNode->showInSimpleTree();
      if (m_hideSourceGroups) {
        if (subFolderNode->isVirtualFolderType()) {
          const auto vnode = static_cast<VirtualFolderNode*>(subFolderNode);
          if (vnode->isSourcesOrHeaders()) {
            isHidden = true;
            hasHiddenSourcesOrHeaders = true;
          }
        }
      }
      if (!isHidden && !seen->contains(subFolderNode)) {
        seen->insert(subFolderNode);
        const auto node = new WrapperNode(subFolderNode);
        parent->appendChild(node);
        addFolderNode(node, subFolderNode, seen);
        node->sortChildren(&sortWrapperNodes);
      } else {
        addFolderNode(parent, subFolderNode, seen);
      }
    } else if (const auto fileNode = node->asFileNode()) {
      if (!seen->contains(fileNode)) {
        seen->insert(fileNode);
        parent->appendChild(new WrapperNode(fileNode));
      }
    }
  }

  if (hasHiddenSourcesOrHeaders) {
    parent->sortChildren(&sortWrapperNodes);
    mergeDuplicates(parent);
  }
}

auto FlatModel::trimEmptyDirectories(WrapperNode *parent) -> bool
{
  const FolderNode *fn = parent->m_node->asFolderNode();
  if (!fn)
    return false;

  for (auto i = parent->childCount() - 1; i >= 0; --i) {
    if (trimEmptyDirectories(parent->childAt(i)))
      parent->removeChildAt(i);
  }
  return parent->childCount() == 0 && !fn->showWhenEmpty();
}

auto FlatModel::supportedDragActions() const -> Qt::DropActions
{
  return Qt::CopyAction;
}

auto FlatModel::mimeTypes() const -> QStringList
{
  return DropSupport::mimeTypesForFilePaths();
}

auto FlatModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  const auto data = new DropMimeData;
  for (const auto &index : indexes) {
    if (auto node = nodeForIndex(index)) {
      if (node->asFileNode())
        data->addFile(node->filePath());
      data->addValue(QVariant::fromValue(node));
    }
  }
  return data;
}

auto FlatModel::canDropMimeData(const QMimeData *data, Qt::DropAction, int, int, const QModelIndex &) const -> bool
{
  // For now, we support only drops of Qt Creator file nodes.
  const auto *const dropData = dynamic_cast<const DropMimeData*>(data);
  if (!dropData)
    return false;
  QTC_ASSERT(!dropData->values().empty(), return false);
  return dropData->files().size() == dropData->values().size();
}

enum class DropAction {
  Copy,
  CopyWithFiles,
  Move,
  MoveWithFiles
};

class DropFileDialog : public QDialog {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::FlatModel)
public:
  DropFileDialog(const FilePath &defaultTargetDir) : m_buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel)), m_buttonGroup(new QButtonGroup(this))
  {
    setWindowTitle(tr("Choose Drop Action"));
    const auto offerFileIo = !defaultTargetDir.isEmpty();
    auto *const layout = new QVBoxLayout(this);
    const QString idename(Orca::Plugin::Core::IDE_DISPLAY_NAME);
    layout->addWidget(new QLabel(tr("You just dragged some files from one project node to " "another.\nWhat should %1 do now?").arg(idename), this));
    auto *const copyButton = new QRadioButton(this);
    m_buttonGroup->addButton(copyButton, int(DropAction::Copy));
    layout->addWidget(copyButton);
    auto *const moveButton = new QRadioButton(this);
    m_buttonGroup->addButton(moveButton, int(DropAction::Move));
    layout->addWidget(moveButton);
    if (offerFileIo) {
      copyButton->setText(tr("Copy Only File References"));
      moveButton->setText(tr("Move Only File References"));
      auto *const copyWithFilesButton = new QRadioButton(tr("Copy file references and files"), this);
      m_buttonGroup->addButton(copyWithFilesButton, int(DropAction::CopyWithFiles));
      layout->addWidget(copyWithFilesButton);
      auto *const moveWithFilesButton = new QRadioButton(tr("Move file references and files"), this);
      m_buttonGroup->addButton(moveWithFilesButton, int(DropAction::MoveWithFiles));
      layout->addWidget(moveWithFilesButton);
      moveWithFilesButton->setChecked(true);
      auto *const targetDirLayout = new QHBoxLayout;
      layout->addLayout(targetDirLayout);
      targetDirLayout->addWidget(new QLabel(tr("Target directory:"), this));
      m_targetDirChooser = new PathChooser(this);
      m_targetDirChooser->setExpectedKind(PathChooser::ExistingDirectory);
      m_targetDirChooser->setFilePath(defaultTargetDir);
      connect(m_targetDirChooser, &PathChooser::validChanged, this, [this](bool valid) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
      });
      targetDirLayout->addWidget(m_targetDirChooser);
      connect(m_buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, [this] {
        switch (dropAction()) {
        case DropAction::CopyWithFiles:
        case DropAction::MoveWithFiles:
          m_targetDirChooser->setEnabled(true);
          m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_targetDirChooser->isValid());
          break;
        case DropAction::Copy:
        case DropAction::Move:
          m_targetDirChooser->setEnabled(false);
          m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
          break;
        }
      });
    } else {
      copyButton->setText(tr("Copy File References"));
      moveButton->setText(tr("Move File References"));
      moveButton->setChecked(true);
    }
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttonBox);
  }

  auto dropAction() const -> DropAction { return static_cast<DropAction>(m_buttonGroup->checkedId()); }

  auto targetDir() const -> FilePath
  {
    return m_targetDirChooser ? m_targetDirChooser->filePath() : FilePath();
  }

private:
  PathChooser *m_targetDirChooser = nullptr;
  QDialogButtonBox *const m_buttonBox;
  QButtonGroup *const m_buttonGroup;
};

auto FlatModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) -> bool
{
  Q_UNUSED(action)

  const auto *const dropData = dynamic_cast<const DropMimeData*>(data);
  QTC_ASSERT(dropData, return false);

  auto fileNodes = transform<QList<const Node*>>(dropData->values(), [](const QVariant &v) { return v.value<Node*>(); });
  QTC_ASSERT(!fileNodes.empty(), return true);

  // The drag operation does not block event handling, so it's possible that the project
  // was reparsed and the nodes in the drop data are now invalid. If that happens for any node,
  // we chicken out and abort the entire operation.
  // Note: In theory, it might be possible that the memory was reused in such an unlucky
  //       way that the pointers refer to different project nodes now, but...
  if (!allOf(fileNodes, [](const Node *n) { return ProjectTree::hasNode(n); }))
    return true;

  // We handle only proper file nodes, i.e. no project or folder nodes and no "pseudo"
  // file nodes that represent the project file.
  fileNodes = filtered(fileNodes, [](const Node *n) {
    return n->asFileNode() && n->asFileNode()->fileType() != FileType::Project;
  });
  if (fileNodes.empty())
    return true;

  // We can handle more than one file being dropped, as long as they have the same parent node.
  const auto sourceProjectNode = fileNodes.first()->parentProjectNode();
  QTC_ASSERT(sourceProjectNode, return true);
  if (anyOf(fileNodes, [sourceProjectNode](const Node *n) {
    return n->parentProjectNode() != sourceProjectNode;
  })) {
    return true;
  }
  auto targetNode = nodeForIndex(index(row, column, parent));
  if (!targetNode)
    targetNode = nodeForIndex(parent);
  QTC_ASSERT(targetNode, return true);
  auto targetProjectNode = targetNode->asProjectNode();
  if (!targetProjectNode)
    targetProjectNode = targetNode->parentProjectNode();
  QTC_ASSERT(targetProjectNode, return true);
  if (sourceProjectNode == targetProjectNode)
    return true;

  // Node weirdness: Sometimes the "file path" is a directory, sometimes it's a file...
  const auto dirForProjectNode = [](const ProjectNode *pNode) {
    const auto dir = pNode->filePath();
    if (dir.isDir())
      return dir;
    return FilePath::fromString(dir.toFileInfo().path());
  };
  auto targetDir = dirForProjectNode(targetProjectNode);

  // Ask the user what to do now: Copy or add? With or without file transfer?
  DropFileDialog dlg(targetDir == dirForProjectNode(sourceProjectNode) ? FilePath() : targetDir);
  if (dlg.exec() != QDialog::Accepted)
    return true;
  if (!dlg.targetDir().isEmpty())
    targetDir = dlg.targetDir();

  // Check the nodes again.
  if (!allOf(fileNodes, [](const Node *n) { return ProjectTree::hasNode(n); }))
    return true;

  // Some helper functions for the file operations.
  const auto targetFilePath = [&targetDir](const FilePath &sourceFilePath) {
    return targetDir.pathAppended(sourceFilePath.fileName());
  };

  struct VcsInfo {
    IVersionControl *vcs = nullptr;
    QString repoDir;

    auto operator==(const VcsInfo &other) const -> bool
    {
      return vcs == other.vcs && repoDir == other.repoDir;
    }
  };
  QHash<QString, VcsInfo> vcsHash;
  const auto vcsInfoForFile = [&vcsHash](const QString &filePath) {
    const auto dir = QFileInfo(filePath).path();
    const auto it = vcsHash.constFind(dir);
    if (it != vcsHash.constEnd())
      return it.value();
    VcsInfo vcsInfo;
    vcsInfo.vcs = VcsManager::findVersionControlForDirectory(FilePath::fromString(dir), &vcsInfo.repoDir);
    vcsHash.insert(dir, vcsInfo);
    return vcsInfo;
  };

  // Now do the actual work.
  const auto sourceFiles = transform(fileNodes, [](const Node *n) { return n->filePath(); });
  FilePaths failedRemoveFromProject;
  FilePaths failedAddToProject;
  FilePaths failedCopyOrMove;
  FilePaths failedDelete;
  FilePaths failedVcsOp;
  switch (dlg.dropAction()) {
  case DropAction::CopyWithFiles: {
    FilePaths filesToAdd;
    const auto vcs = VcsManager::findVersionControlForDirectory(targetDir);
    const auto addToVcs = vcs && vcs->supportsOperation(IVersionControl::AddOperation);
    for (const auto &sourceFile : sourceFiles) {
      const auto targetFile = targetFilePath(sourceFile);
      if (sourceFile.copyFile(targetFile)) {
        filesToAdd << targetFile;
        if (addToVcs && !vcs->vcsAdd(targetFile))
          failedVcsOp << targetFile;
      } else {
        failedCopyOrMove << sourceFile;
      }
    }
    targetProjectNode->addFiles(filesToAdd, &failedAddToProject);
    break;
  }
  case DropAction::Copy:
    targetProjectNode->addFiles(sourceFiles, &failedAddToProject);
    break;
  case DropAction::MoveWithFiles: {
    FilePaths filesToAdd;
    FilePaths filesToRemove;
    const auto targetVcs = vcsInfoForFile(targetDir.toString());
    const auto vcsAddPossible = targetVcs.vcs && targetVcs.vcs->supportsOperation(IVersionControl::AddOperation);
    for (const auto &sourceFile : sourceFiles) {
      const auto targetFile = targetFilePath(sourceFile);
      const auto sourceVcs = vcsInfoForFile(sourceFile.toString());
      if (sourceVcs.vcs && targetVcs.vcs && sourceVcs == targetVcs && sourceVcs.vcs->supportsOperation(IVersionControl::MoveOperation)) {
        if (sourceVcs.vcs->vcsMove(sourceFile, targetFile)) {
          filesToAdd << targetFile;
          filesToRemove << sourceFile;
        } else {
          failedCopyOrMove << sourceFile;
        }
        continue;
      }
      if (!sourceFile.copyFile(targetFile)) {
        failedCopyOrMove << sourceFile;
        continue;
      }
      filesToAdd << targetFile;
      filesToRemove << sourceFile;
      FileChangeBlocker changeGuard(sourceFile);
      if (sourceVcs.vcs && sourceVcs.vcs->supportsOperation(IVersionControl::DeleteOperation) && !sourceVcs.vcs->vcsDelete(sourceFile)) {
        failedVcsOp << sourceFile;
      }
      if (sourceFile.exists() && !sourceFile.removeFile())
        failedDelete << sourceFile;
      if (vcsAddPossible && !targetVcs.vcs->vcsAdd(targetFile))
        failedVcsOp << targetFile;
    }
    const auto result = sourceProjectNode->removeFiles(filesToRemove, &failedRemoveFromProject);
    if (result == RemovedFilesFromProject::Wildcard)
      failedRemoveFromProject.clear();
    targetProjectNode->addFiles(filesToAdd, &failedAddToProject);
    break;
  }
  case DropAction::Move:
    sourceProjectNode->removeFiles(sourceFiles, &failedRemoveFromProject);
    targetProjectNode->addFiles(sourceFiles, &failedAddToProject);
    break;
  }

  // Summary for the user in case anything went wrong.
  const auto makeUserFileList = [](const FilePaths &files) {
    return FilePath::formatFilePaths(files, "\n  ");
  };
  if (!failedAddToProject.empty() || !failedRemoveFromProject.empty() || !failedCopyOrMove.empty() || !failedDelete.empty() || !failedVcsOp.empty()) {
    auto message = tr("Not all operations finished successfully.");
    if (!failedCopyOrMove.empty()) {
      message.append('\n').append(tr("The following files could not be copied or moved:")).append("\n  ").append(makeUserFileList(failedCopyOrMove));
    }
    if (!failedRemoveFromProject.empty()) {
      message.append('\n').append(tr("The following files could not be removed from the " "project file:")).append("\n  ").append(makeUserFileList(failedRemoveFromProject));
    }
    if (!failedAddToProject.empty()) {
      message.append('\n').append(tr("The following files could not be added to the " "project file:")).append("\n  ").append(makeUserFileList(failedAddToProject));
    }
    if (!failedDelete.empty()) {
      message.append('\n').append(tr("The following files could not be deleted:")).append("\n  ").append(makeUserFileList(failedDelete));
    }
    if (!failedVcsOp.empty()) {
      message.append('\n').append(tr("A version control operation failed for the following " "files. Please check your repository.")).append("\n  ").append(makeUserFileList(failedVcsOp));
    }
    QMessageBox::warning(ICore::dialogParent(), tr("Failure Updating Project"), message);
  }

  return true;
}

auto FlatModel::wrapperForNode(const Node *node) const -> WrapperNode*
{
  return findNonRootItem([node](WrapperNode *item) {
    return item->m_node == node;
  });
}

auto FlatModel::indexForNode(const Node *node) const -> QModelIndex
{
  const auto wrapper = wrapperForNode(node);
  return wrapper ? indexForItem(wrapper) : QModelIndex();
}

auto FlatModel::setProjectFilterEnabled(bool filter) -> void
{
  if (filter == m_filterProjects)
    return;
  m_filterProjects = filter;
  rebuildModel();
}

auto FlatModel::setGeneratedFilesFilterEnabled(bool filter) -> void
{
  if (filter == m_filterGeneratedFiles)
    return;
  m_filterGeneratedFiles = filter;
  rebuildModel();
}

auto FlatModel::setDisabledFilesFilterEnabled(bool filter) -> void
{
  if (filter == m_filterDisabledFiles)
    return;
  m_filterDisabledFiles = filter;
  rebuildModel();
}

auto FlatModel::setTrimEmptyDirectories(bool filter) -> void
{
  if (filter == m_trimEmptyDirectories)
    return;
  m_trimEmptyDirectories = filter;
  rebuildModel();
}

auto FlatModel::setHideSourceGroups(bool filter) -> void
{
  if (filter == m_hideSourceGroups)
    return;
  m_hideSourceGroups = filter;
  rebuildModel();
}

auto FlatModel::projectFilterEnabled() -> bool
{
  return m_filterProjects;
}

auto FlatModel::generatedFilesFilterEnabled() -> bool
{
  return m_filterGeneratedFiles;
}

auto FlatModel::trimEmptyDirectoriesEnabled() -> bool
{
  return m_trimEmptyDirectories;
}

auto FlatModel::nodeForIndex(const QModelIndex &index) const -> Node*
{
  const auto flatNode = itemForIndex(index);
  return flatNode ? flatNode->m_node : nullptr;
}

auto FlatModel::logger() -> const QLoggingCategory&
{
  static QLoggingCategory logger("qtc.projectexplorer.flatmodel", QtWarningMsg);
  return logger;
}

} // namespace Internal
} // namespace ProjectExplorer

