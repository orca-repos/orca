// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectnodes.hpp"

#include "buildconfiguration.hpp"
#include "buildsystem.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "projecttree.hpp"
#include "target.hpp"

#include <core/core-file-icon-provider.hpp>
#include <core/core-interface.hpp>
#include <core/core-version-control-interface.hpp>
#include <core/core-vcs-manager.hpp>

#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/mimetypes/mimetype.hpp>
#include <utils/pointeralgorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/utilsicons.hpp>

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStyle>
#include <QThread>
#include <QTimer>

#include <memory>

using namespace Utils;

namespace ProjectExplorer {

QHash<QString, QIcon> DirectoryIcon::m_cache;

static auto recursiveFindOrCreateFolderNode(FolderNode *folder, const FilePath &directory, const FilePath &overrideBaseDir, const FolderNode::FolderNodeFactory &factory) -> FolderNode*
{
  auto path = overrideBaseDir.isEmpty() ? folder->filePath() : overrideBaseDir;

  FilePath directoryWithoutPrefix;
  auto isRelative = false;

  if (path.isEmpty() || path.toDir().isRoot()) {
    directoryWithoutPrefix = directory;
    isRelative = false;
  } else {
    if (directory.isChildOf(path) || directory == path) {
      isRelative = true;
      directoryWithoutPrefix = directory.relativeChildPath(path);
    } else {
      isRelative = false;
      path.clear();
      directoryWithoutPrefix = directory;
    }
  }
  auto parts = directoryWithoutPrefix.toString().split('/', Qt::SkipEmptyParts);
  if (!HostOsInfo::isWindowsHost() && !isRelative && !parts.isEmpty())
    parts[0].prepend('/');

  auto parent = folder;
  foreach(const QString &part, parts) {
    path = path.pathAppended(part);
    // Find folder in subFolders
    auto next = parent->folderNode(path);
    if (!next) {
      // No FolderNode yet, so create it
      auto tmp = factory(path);
      tmp->setDisplayName(part);
      next = tmp.get();
      parent->addNode(std::move(tmp));
    }
    parent = next;
  }
  return parent;
}

/*!
  \class ProjectExplorer::Node

  \brief The Node class is the base class of all nodes in the node hierarchy.

  The nodes are arranged in a tree where leaves are FileNodes and non-leaves are FolderNodes
  A Project is a special Folder that manages the files and normal folders underneath it.

  The Watcher emits signals for structural changes in the hierarchy.
  A Visitor can be used to traverse all Projects and other Folders.

  \sa ProjectExplorer::FileNode, ProjectExplorer::FolderNode, ProjectExplorer::ProjectNode
  \sa ProjectExplorer::NodesWatcher
*/

Node::Node() = default;

auto Node::setPriority(int p) -> void
{
  m_priority = p;
}

auto Node::setFilePath(const FilePath &filePath) -> void
{
  m_filePath = filePath;
}

auto Node::setLine(int line) -> void
{
  m_line = line;
}

auto Node::setListInProject(bool l) -> void
{
  if (l)
    m_flags = static_cast<NodeFlag>(m_flags | FlagListInProject);
  else
    m_flags = static_cast<NodeFlag>(m_flags & ~FlagListInProject);
}

auto Node::setIsGenerated(bool g) -> void
{
  if (g)
    m_flags = static_cast<NodeFlag>(m_flags | FlagIsGenerated);
  else
    m_flags = static_cast<NodeFlag>(m_flags & ~FlagIsGenerated);
}

auto Node::setAbsoluteFilePathAndLine(const FilePath &path, int line) -> void
{
  if (m_filePath == path && m_line == line)
    return;

  m_filePath = path;
  m_line = line;
}

Node::~Node() = default;

auto Node::priority() const -> int
{
  return m_priority;
}

/*!
  Returns \c true if the Node should be listed as part of the projects file list.
  */
auto Node::listInProject() const -> bool
{
  return (m_flags & FlagListInProject) == FlagListInProject;
}

/*!
  The project that owns and manages the node. It is the first project in the list
  of ancestors.
  */
auto Node::parentProjectNode() const -> ProjectNode*
{
  if (!m_parentFolderNode)
    return nullptr;
  const auto pn = m_parentFolderNode->asProjectNode();
  if (pn)
    return pn;
  return m_parentFolderNode->parentProjectNode();
}

/*!
  The parent in the node hierarchy.
  */
auto Node::parentFolderNode() const -> FolderNode*
{
  return m_parentFolderNode;
}

auto Node::managingProject() -> ProjectNode*
{
  if (asContainerNode())
    return asContainerNode()->rootProjectNode();
  QTC_ASSERT(m_parentFolderNode, return nullptr);
  const auto pn = parentProjectNode();
  return pn ? pn : asProjectNode(); // projects manage themselves...
}

auto Node::managingProject() const -> const ProjectNode*
{
  return const_cast<Node*>(this)->managingProject();
}

auto Node::getProject() const -> Project*
{
  if (const auto cn = asContainerNode())
    return cn->project();
  if (!m_parentFolderNode)
    return nullptr;
  return m_parentFolderNode->getProject();
}

/*!
  The path of the file or folder in the filesystem the node represents.
  */
auto Node::filePath() const -> const FilePath&
{
  return m_filePath;
}

auto Node::line() const -> int
{
  return m_line;
}

auto Node::displayName() const -> QString
{
  return filePath().fileName();
}

auto Node::tooltip() const -> QString
{
  return filePath().toUserOutput();
}

auto Node::isEnabled() const -> bool
{
  if ((m_flags & FlagIsEnabled) == 0)
    return false;
  const auto parent = parentFolderNode();
  return parent ? parent->isEnabled() : true;
}

auto FileNode::icon() const -> QIcon
{
  if (hasError())
    return Icons::WARNING.icon();
  if (m_icon.isNull())
    m_icon = Orca::Plugin::Core::icon(filePath());
  return m_icon;
}

auto FileNode::setIcon(const QIcon icon) -> void
{
  m_icon = icon;
}

auto FileNode::hasError() const -> bool
{
  return m_hasError;
}

auto FileNode::setHasError(bool error) -> void
{
  m_hasError = error;
}

auto FileNode::setHasError(bool error) const -> void
{
  m_hasError = error;
}

/*!
  Returns \c true if the file is automatically generated by a compile step.
  */
auto Node::isGenerated() const -> bool
{
  return (m_flags & FlagIsGenerated) == FlagIsGenerated;
}

auto Node::supportsAction(ProjectAction, const Node *) const -> bool
{
  return false;
}

auto Node::setEnabled(bool enabled) -> void
{
  if (enabled)
    m_flags = static_cast<NodeFlag>(m_flags | FlagIsEnabled);
  else
    m_flags = static_cast<NodeFlag>(m_flags & ~FlagIsEnabled);
}

auto Node::sortByPath(const Node *a, const Node *b) -> bool
{
  return a->filePath() < b->filePath();
}

auto Node::setParentFolderNode(FolderNode *parentFolder) -> void
{
  m_parentFolderNode = parentFolder;
}

auto Node::fileTypeForMimeType(const MimeType &mt) -> FileType
{
  auto type = FileType::Source;
  if (mt.isValid()) {
    const auto mtName = mt.name();
    if (mtName == Constants::C_HEADER_MIMETYPE || mtName == Constants::CPP_HEADER_MIMETYPE)
      type = FileType::Header;
    else if (mtName == Constants::FORM_MIMETYPE)
      type = FileType::Form;
    else if (mtName == Constants::RESOURCE_MIMETYPE)
      type = FileType::Resource;
    else if (mtName == Constants::SCXML_MIMETYPE)
      type = FileType::StateChart;
    else if (mtName == Constants::QML_MIMETYPE || mtName == Constants::QMLUI_MIMETYPE)
      type = FileType::QML;
  } else {
    type = FileType::Unknown;
  }
  return type;
}

auto Node::fileTypeForFileName(const FilePath &file) -> FileType
{
  return fileTypeForMimeType(mimeTypeForFile(file, MimeMatchMode::MatchExtension));
}

auto Node::pathOrDirectory(bool dir) const -> FilePath
{
  FilePath location;
  const auto folder = asFolderNode();
  if (isVirtualFolderType() && folder) {
    // Virtual Folder case
    // If there are files directly below or no subfolders, take the folder path
    if (!folder->fileNodes().isEmpty() || folder->folderNodes().isEmpty()) {
      location = m_filePath;
    } else {
      // Otherwise we figure out a commonPath from the subfolders
      QStringList list;
      foreach(FolderNode *f, folder->folderNodes())
        list << f->filePath().toString() + QLatin1Char('/');
      location = FilePath::fromString(commonPath(list));
    }

    QTC_CHECK(!location.needsDevice());
    auto fi = location.toFileInfo();
    while ((!fi.exists() || !fi.isDir()) && !fi.isRoot())
      fi.setFile(fi.absolutePath());
    location = FilePath::fromString(fi.absoluteFilePath());
  } else if (!m_filePath.isEmpty()) {
    QTC_CHECK(!m_filePath.needsDevice());
    auto fi = m_filePath.toFileInfo();
    // remove any /suffixes, which e.g. ResourceNode uses
    // Note this could be removed again by making path() a true path again
    // That requires changes in both the VirtualFolderNode and ResourceNode
    while (!fi.exists() && !fi.isRoot())
      fi.setFile(fi.absolutePath());

    if (dir)
      location = FilePath::fromString(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath());
    else
      location = FilePath::fromString(fi.absoluteFilePath());
  }
  return location;
}

/*!
  \class ProjectExplorer::FileNode

  \brief The FileNode class is an in-memory presentation of a file.

  All file nodes are leaf nodes.

  \sa ProjectExplorer::FolderNode, ProjectExplorer::ProjectNode
*/

FileNode::FileNode(const FilePath &filePath, const FileType fileType) : m_fileType(fileType)
{
  setFilePath(filePath);
  setListInProject(true);
  if (fileType == FileType::Project)
    setPriority(DefaultProjectFilePriority);
  else
    setPriority(DefaultFilePriority);
}

auto FileNode::clone() const -> FileNode*
{
  const auto fn = new FileNode(filePath(), fileType());
  fn->setLine(line());
  fn->setIsGenerated(isGenerated());
  fn->setEnabled(isEnabled());
  fn->setPriority(priority());
  fn->setListInProject(listInProject());
  return fn;
}

auto FileNode::fileType() const -> FileType
{
  return m_fileType;
}

auto FileNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  if (action == InheritedFromParent)
    return true;
  const auto parentFolder = parentFolderNode();
  return parentFolder && parentFolder->supportsAction(action, node);
}

auto FileNode::displayName() const -> QString
{
  const auto l = line();
  if (l < 0)
    return Node::displayName();
  return Node::displayName() + ':' + QString::number(l);
}

/*!
  \class ProjectExplorer::FolderNode

  In-memory presentation of a folder. Note that the node itself + all children (files and folders) are "managed" by the owning project.

  \sa ProjectExplorer::FileNode, ProjectExplorer::ProjectNode
*/
FolderNode::FolderNode(const FilePath &folderPath)
{
  setFilePath(folderPath);
  setPriority(DefaultFolderPriority);
  setListInProject(false);
  setIsGenerated(false);
  m_displayName = folderPath.toUserOutput();
}

/*!
    Contains the display name that should be used in a view.
    \sa setFolderName()
 */

auto FolderNode::displayName() const -> QString
{
  return m_displayName;
}

/*!
    Contains the icon that should be used in a view. Default is the directory icon
    (QStyle::S_PDirIcon). Calling this method is only safe in the UI thread.

    \sa setIcon()
 */
auto FolderNode::icon() const -> QIcon
{
  QTC_CHECK(QThread::currentThread() == QCoreApplication::instance()->thread());

  // Instantiating the Icon provider is expensive.
  if (const auto strPtr = Utils::get_if<QString>(&m_icon)) {
    m_icon = QIcon(*strPtr);
  } else if (const auto directoryIconPtr = Utils::get_if<DirectoryIcon>(&m_icon)) {
    m_icon = directoryIconPtr->icon();
  } else if (const auto creatorPtr = Utils::get_if<IconCreator>(&m_icon)) {
    m_icon = (*creatorPtr)();
  } else {
    const auto iconPtr = Utils::get_if<QIcon>(&m_icon);
    if (!iconPtr || iconPtr->isNull())
      m_icon = Orca::Plugin::Core::icon(QFileIconProvider::Folder);
  }
  return Utils::get<QIcon>(m_icon);
}

auto FolderNode::findNode(const std::function<bool(Node *)> &filter) -> Node*
{
  if (filter(this))
    return this;

  for (const auto &n : m_nodes) {
    if (n->asFileNode() && filter(n.get())) {
      return n.get();
    } else if (const auto folder = n->asFolderNode()) {
      const auto result = folder->findNode(filter);
      if (result)
        return result;
    }
  }
  return nullptr;
}

auto FolderNode::findNodes(const std::function<bool(Node *)> &filter) -> QList<Node*>
{
  QList<Node*> result;
  if (filter(this))
    result.append(this);
  for (const auto &n : m_nodes) {
    if (n->asFileNode() && filter(n.get()))
      result.append(n.get());
    else if (const auto folder = n->asFolderNode())
      result.append(folder->findNodes(filter));
  }
  return result;
}

auto FolderNode::forEachNode(const std::function<void(FileNode *)> &fileTask, const std::function<void(FolderNode *)> &folderTask, const std::function<bool(const FolderNode *)> &folderFilterTask) const -> void
{
  if (folderFilterTask) {
    if (!folderFilterTask(this))
      return;
  }
  if (fileTask) {
    for (const auto &n : m_nodes) {
      if (const auto fn = n->asFileNode())
        fileTask(fn);
    }
  }
  for (const auto &n : m_nodes) {
    if (const auto fn = n->asFolderNode()) {
      if (folderTask)
        folderTask(fn);
      fn->forEachNode(fileTask, folderTask, folderFilterTask);
    }
  }
}

auto FolderNode::forEachGenericNode(const std::function<void(Node *)> &genericTask) const -> void
{
  for (const auto &n : m_nodes) {
    genericTask(n.get());
    if (const auto fn = n->asFolderNode())
      fn->forEachGenericNode(genericTask);
  }
}

auto FolderNode::forEachProjectNode(const std::function<void(const ProjectNode *)> &task) const -> void
{
  if (const auto projectNode = asProjectNode())
    task(projectNode);

  for (const auto &n : m_nodes) {
    if (const auto fn = n->asFolderNode())
      fn->forEachProjectNode(task);
  }
}

auto FolderNode::findProjectNode(const std::function<bool(const ProjectNode *)> &predicate) -> ProjectNode*
{
  if (const auto projectNode = asProjectNode()) {
    if (predicate(projectNode))
      return projectNode;
  }

  for (const auto &n : m_nodes) {
    if (const auto fn = n->asFolderNode()) {
      if (const auto pn = fn->findProjectNode(predicate))
        return pn;
    }
  }
  return nullptr;
}

auto FolderNode::nodes() const -> const QList<Node*>
{
  return Utils::toRawPointer<QList>(m_nodes);
}

auto FolderNode::fileNodes() const -> QList<FileNode*>
{
  QList<FileNode*> result;
  for (const auto &n : m_nodes) {
    if (const auto fn = n->asFileNode())
      result.append(fn);
  }
  return result;
}

auto FolderNode::fileNode(const FilePath &file) const -> FileNode*
{
  return static_cast<FileNode*>(findOrDefault(m_nodes, [&file](const std::unique_ptr<Node> &n) {
    const FileNode *fn = n->asFileNode();
    return fn && fn->filePath() == file;
  }));
}

auto FolderNode::folderNodes() const -> QList<FolderNode*>
{
  QList<FolderNode*> result;
  for (const auto &n : m_nodes) {
    if (const auto fn = n->asFolderNode())
      result.append(fn);
  }
  return result;
}

auto FolderNode::folderNode(const FilePath &directory) const -> FolderNode*
{
  const auto node = findOrDefault(m_nodes, [directory](const std::unique_ptr<Node> &n) {
    const auto fn = n->asFolderNode();
    return fn && fn->filePath() == directory;
  });
  return static_cast<FolderNode*>(node);
}

auto FolderNode::addNestedNode(std::unique_ptr<FileNode> &&fileNode, const FilePath &overrideBaseDir, const FolderNodeFactory &factory) -> void
{
  const auto folder = recursiveFindOrCreateFolderNode(this, fileNode->filePath().parentDir(), overrideBaseDir, factory);
  folder->addNode(std::move(fileNode));
}

auto FolderNode::addNestedNodes(std::vector<std::unique_ptr<FileNode>> &&files, const FilePath &overrideBaseDir, const FolderNodeFactory &factory) -> void
{
  using DirWithNodes = std::pair<FilePath, std::vector<std::unique_ptr<FileNode>>>;
  std::vector<DirWithNodes> fileNodesPerDir;
  for (auto &f : files) {
    const auto parentDir = f->filePath().parentDir();
    const auto it = std::lower_bound(fileNodesPerDir.begin(), fileNodesPerDir.end(), parentDir, [](const DirWithNodes &nad, const FilePath &dir) { return nad.first < dir; });
    if (it != fileNodesPerDir.end() && it->first == parentDir) {
      it->second.emplace_back(std::move(f));
    } else {
      DirWithNodes dirWithNodes;
      dirWithNodes.first = parentDir;
      dirWithNodes.second.emplace_back(std::move(f));
      fileNodesPerDir.insert(it, std::move(dirWithNodes));
    }
  }

  for (auto &dirWithNodes : fileNodesPerDir) {
    const auto folderNode = recursiveFindOrCreateFolderNode(this, dirWithNodes.first, overrideBaseDir, factory);
    for (auto &f : dirWithNodes.second)
      folderNode->addNode(std::move(f));
  }
}

// "Compress" a tree of foldernodes such that foldernodes with exactly one foldernode as a child
// are merged into one. This e.g. turns a sequence of FolderNodes "foo" "bar" "baz" into one
// FolderNode named "foo/bar/baz", saving a lot of clicks in the Project View to get to the actual
// files.
auto FolderNode::compress() -> void
{
  if (const auto subFolder = m_nodes.size() == 1 ? m_nodes.at(0)->asFolderNode() : nullptr) {
    const auto sameType = (isFolderNodeType() && subFolder->isFolderNodeType()) || (isProjectNodeType() && subFolder->isProjectNodeType()) || (isVirtualFolderType() && subFolder->isVirtualFolderType());
    if (!sameType)
      return;

    // Only one subfolder: Compress!
    setDisplayName(QDir::toNativeSeparators(displayName() + "/" + subFolder->displayName()));
    for (const auto n : subFolder->nodes()) {
      auto toMove = subFolder->takeNode(n);
      toMove->setParentFolderNode(nullptr);
      addNode(std::move(toMove));
    }
    setAbsoluteFilePathAndLine(subFolder->filePath(), -1);

    takeNode(subFolder);

    compress();
  } else {
    for (const auto fn : folderNodes())
      fn->compress();
  }
}

auto FolderNode::replaceSubtree(Node *oldNode, std::unique_ptr<Node> &&newNode) -> bool
{
  std::unique_ptr<Node> keepAlive;
  if (!oldNode) {
    addNode(std::move(newNode)); // Happens e.g. when a project is registered
  } else {
    const auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [oldNode](const std::unique_ptr<Node> &n) {
      return oldNode == n.get();
    });
    QTC_ASSERT(it != m_nodes.end(), return false);
    if (newNode) {
      newNode->setParentFolderNode(this);
      keepAlive = std::move(*it);
      *it = std::move(newNode);
    } else {
      keepAlive = takeNode(oldNode); // Happens e.g. when project is shutting down
    }
  }
  handleSubTreeChanged(this);
  return true;
}

auto FolderNode::setDisplayName(const QString &name) -> void
{
  if (m_displayName == name)
    return;
  m_displayName = name;
}

/*!
    Sets the \a icon for this node. Note that creating QIcon instances is only safe in the UI thread.
*/
auto FolderNode::setIcon(const QIcon &icon) -> void
{
  m_icon = icon;
}

/*!
    Sets the \a directoryIcon that is used to create the icon for this node on demand.
*/
auto FolderNode::setIcon(const DirectoryIcon &directoryIcon) -> void
{
  m_icon = directoryIcon;
}

/*!
    Sets the \a path that is used to create the icon for this node on demand.
*/
auto FolderNode::setIcon(const QString &path) -> void
{
  m_icon = path;
}

/*!
    Sets the \a iconCreator function that is used to create the icon for this node on demand.
*/
auto FolderNode::setIcon(const IconCreator &iconCreator) -> void
{
  m_icon = iconCreator;
}

auto FolderNode::setLocationInfo(const QVector<LocationInfo> &info) -> void
{
  m_locations = info;
  sort(m_locations, &LocationInfo::priority);
}

auto FolderNode::locationInfo() const -> const QVector<LocationInfo>
{
  return m_locations;
}

auto FolderNode::addFileFilter() const -> QString
{
  if (!m_addFileFilter.isNull())
    return m_addFileFilter;

  const auto fn = parentFolderNode();
  return fn ? fn->addFileFilter() : QString();
}

auto FolderNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  if (action == InheritedFromParent)
    return true;
  const auto parentFolder = parentFolderNode();
  return parentFolder && parentFolder->supportsAction(action, node);
}

auto FolderNode::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  const auto pn = managingProject();
  if (pn)
    return pn->addFiles(filePaths, notAdded);
  return false;
}

auto FolderNode::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  if (const auto pn = managingProject())
    return pn->removeFiles(filePaths, notRemoved);
  return RemovedFilesFromProject::Error;
}

auto FolderNode::deleteFiles(const FilePaths &filePaths) -> bool
{
  const auto pn = managingProject();
  if (pn)
    return pn->deleteFiles(filePaths);
  return false;
}

auto FolderNode::canRenameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  const auto pn = managingProject();
  if (pn)
    return pn->canRenameFile(oldFilePath, newFilePath);
  return false;
}

auto FolderNode::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  const auto pn = managingProject();
  if (pn)
    return pn->renameFile(oldFilePath, newFilePath);
  return false;
}

auto FolderNode::addDependencies(const QStringList &dependencies) -> bool
{
  if (const auto pn = managingProject())
    return pn->addDependencies(dependencies);
  return false;
}

auto FolderNode::addNewInformation(const FilePaths &files, Node *context) const -> AddNewInformation
{
  Q_UNUSED(files)
  return AddNewInformation(displayName(), context == this ? 120 : 100);
}

/*!
  Adds a node specified by \a node to the internal list of nodes.
*/

auto FolderNode::addNode(std::unique_ptr<Node> &&node) -> void
{
  QTC_ASSERT(node, return);
  QTC_ASSERT(!node->parentFolderNode(), qDebug("Node has already a parent folder"));
  node->setParentFolderNode(this);
  m_nodes.emplace_back(std::move(node));
}

/*!
  Return a node specified by \a node from the internal list.
*/

auto FolderNode::takeNode(Node *node) -> std::unique_ptr<Node>
{
  return takeOrDefault(m_nodes, node);
}

auto FolderNode::showInSimpleTree() const -> bool
{
  return false;
}

auto FolderNode::showWhenEmpty() const -> bool
{
  return m_showWhenEmpty;
}

/*!
  \class ProjectExplorer::VirtualFolderNode

  In-memory presentation of a virtual folder.
  Note that the node itself + all children (files and folders) are "managed" by the owning project.
  A virtual folder does not correspond to a actual folder on the file system. See for example the
  sources, headers and forms folder the QmakeProjectManager creates
  VirtualFolderNodes are always sorted before FolderNodes and are sorted according to their priority.

  \sa ProjectExplorer::FileNode, ProjectExplorer::ProjectNode
*/
VirtualFolderNode::VirtualFolderNode(const FilePath &folderPath) : FolderNode(folderPath) {}

/*!
  \class ProjectExplorer::ProjectNode

  \brief The ProjectNode class is an in-memory presentation of a Project.

  A concrete subclass must implement the persistent data.

  \sa ProjectExplorer::FileNode, ProjectExplorer::FolderNode
*/

/*!
  Creates an uninitialized project node object.
  */
ProjectNode::ProjectNode(const FilePath &projectFilePath) : FolderNode(projectFilePath)
{
  setPriority(DefaultProjectPriority);
  setListInProject(true);
  setDisplayName(projectFilePath.fileName());
}

auto ProjectNode::canAddSubProject(const FilePath &proFilePath) const -> bool
{
  Q_UNUSED(proFilePath)
  return false;
}

auto ProjectNode::addSubProject(const FilePath &proFilePath) -> bool
{
  Q_UNUSED(proFilePath)
  return false;
}

auto ProjectNode::subProjectFileNamePatterns() const -> QStringList
{
  return QStringList();
}

auto ProjectNode::removeSubProject(const FilePath &proFilePath) -> bool
{
  Q_UNUSED(proFilePath)
  return false;
}

auto ProjectNode::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  if (const auto bs = buildSystem())
    return bs->addFiles(this, filePaths, notAdded);
  return false;
}

auto ProjectNode::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  if (const auto bs = buildSystem())
    return bs->removeFiles(this, filePaths, notRemoved);
  return RemovedFilesFromProject::Error;
}

auto ProjectNode::deleteFiles(const FilePaths &filePaths) -> bool
{
  if (const auto bs = buildSystem())
    return bs->deleteFiles(this, filePaths);
  return false;
}

auto ProjectNode::canRenameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (const auto bs = buildSystem())
    return bs->canRenameFile(this, oldFilePath, newFilePath);
  return true;
}

auto ProjectNode::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (const auto bs = buildSystem())
    return bs->renameFile(this, oldFilePath, newFilePath);
  return false;
}

auto ProjectNode::addDependencies(const QStringList &dependencies) -> bool
{
  if (const auto bs = buildSystem())
    return bs->addDependencies(this, dependencies);
  return false;
}

auto ProjectNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  if (const auto bs = buildSystem())
    return bs->supportsAction(const_cast<ProjectNode*>(this), action, node);
  return false;
}

auto ProjectNode::deploysFolder(const QString &folder) const -> bool
{
  Q_UNUSED(folder)
  return false;
}

auto ProjectNode::projectNode(const FilePath &file) const -> ProjectNode*
{
  for (const auto &n : m_nodes) {
    if (const auto pnode = n->asProjectNode())
      if (pnode->filePath() == file)
        return pnode;
  }
  return nullptr;
}

auto ProjectNode::data(Id role) const -> QVariant
{
  return m_fallbackData.value(role);
}

auto ProjectNode::setData(Id role, const QVariant &value) const -> bool
{
  Q_UNUSED(role)
  Q_UNUSED(value)
  return false;
}

auto ProjectNode::setFallbackData(Id key, const QVariant &value) -> void
{
  m_fallbackData.insert(key, value);
}

auto ProjectNode::buildSystem() const -> BuildSystem*
{
  const auto p = getProject();
  const auto t = p ? p->activeTarget() : nullptr;
  return t ? t->buildSystem() : nullptr;
}

auto FolderNode::isEmpty() const -> bool
{
  return m_nodes.size() == 0;
}

auto FolderNode::handleSubTreeChanged(FolderNode *node) -> void
{
  if (const auto parent = parentFolderNode())
    parent->handleSubTreeChanged(node);
}

auto FolderNode::setShowWhenEmpty(bool showWhenEmpty) -> void
{
  m_showWhenEmpty = showWhenEmpty;
}

ContainerNode::ContainerNode(Project *project) : FolderNode(project->projectDirectory()), m_project(project) {}

auto ContainerNode::displayName() const -> QString
{
  auto name = m_project->displayName();

  const auto fp = m_project->projectFilePath();
  const auto dir = fp.isDir() ? fp.absoluteFilePath() : fp.absolutePath();
  if (const auto vc = Orca::Plugin::Core::VcsManager::findVersionControlForDirectory(dir)) {
    const auto vcsTopic = vc->vcsTopic(dir);
    if (!vcsTopic.isEmpty())
      name += " [" + vcsTopic + ']';
  }

  return name;
}

auto ContainerNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  const Node *rootNode = m_project->rootProjectNode();
  return rootNode && rootNode->supportsAction(action, node);
}

auto ContainerNode::rootProjectNode() const -> ProjectNode*
{
  return m_project->rootProjectNode();
}

auto ContainerNode::removeAllChildren() -> void
{
  m_nodes.clear();
}

auto ContainerNode::handleSubTreeChanged(FolderNode *node) -> void
{
  m_project->handleSubTreeChanged(node);
}

/*!
    \class ProjectExplorer::DirectoryIcon

    The DirectoryIcon class represents a directory icon with an overlay.

    The QIcon is created on demand and globally cached, so other DirectoryIcon
    instances with the same overlay share the same QIcon instance.
*/

/*!
    Creates a DirectoryIcon for the specified \a overlay.
*/
DirectoryIcon::DirectoryIcon(const QString &overlay) : m_overlay(overlay) {}

/*!
    Returns the icon for this DirectoryIcon. Calling this method is only safe in the UI thread.
*/
auto DirectoryIcon::icon() const -> QIcon
{
  QTC_CHECK(QThread::currentThread() == QCoreApplication::instance()->thread());
  const auto it = m_cache.find(m_overlay);
  if (it != m_cache.end())
    return it.value();
  const auto icon = Orca::Plugin::Core::directoryIcon(m_overlay);
  m_cache.insert(m_overlay, icon);
  return icon;
}

} // namespace ProjectExplorer
