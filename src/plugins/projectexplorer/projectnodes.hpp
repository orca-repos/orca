// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <QFutureInterface>
#include <QIcon>
#include <QStringList>

#include <utils/fileutils.hpp>
#include <utils/id.hpp>
#include <utils/optional.hpp>
#include <utils/variant.hpp>

#include <functional>

namespace Utils {
class MimeType;
}

namespace ProjectExplorer {

class BuildSystem;
class Project;

// File types common for qt projects
enum class FileType : quint16 {
  Unknown = 0,
  Header,
  Source,
  Form,
  StateChart,
  Resource,
  QML,
  Project,
  FileTypeSize
};

enum class ProductType {
  App,
  Lib,
  Other,
  None
};

enum ProjectAction {
  // Special value to indicate that the actions are handled by the parent
  InheritedFromParent,
  AddSubProject,
  AddExistingProject,
  RemoveSubProject,
  // Let's the user select to which project file
  // the file is added
  AddNewFile,
  AddExistingFile,
  // Add files, which match user defined filters,
  // from an existing directory and its subdirectories
  AddExistingDirectory,
  // Removes a file from the project, optionally also
  // delete it on disc
  RemoveFile,
  // Deletes a file from the file system, informs the project
  // that a file was deleted
  // DeleteFile is a define on windows...
  EraseFile,
  Rename,
  // hides actions that use the path(): Open containing folder, open terminal here and Find in Directory
  HidePathActions,
  HideFileActions,
  HideFolderActions,
};

enum class RemovedFilesFromProject {
  Ok,
  Wildcard,
  Error
};

class FileNode;
class FolderNode;
class ProjectNode;
class ContainerNode;

class PROJECTEXPLORER_EXPORT DirectoryIcon {
public:
  explicit DirectoryIcon(const QString &overlay);

  auto icon() const -> QIcon; // only safe in UI thread

private:
  QString m_overlay;
  static QHash<QString, QIcon> m_cache;
};

using IconCreator = std::function<QIcon()>;

// Documentation inside.
class PROJECTEXPLORER_EXPORT Node {
public:
  enum PriorityLevel {
    DefaultPriority = 0,
    DefaultFilePriority = 100000,
    DefaultFolderPriority = 200000,
    DefaultVirtualFolderPriority = 300000,
    DefaultProjectPriority = 400000,
    DefaultProjectFilePriority = 500000
  };

  virtual ~Node();

  virtual auto isFolderNodeType() const -> bool { return false; }
  virtual auto isProjectNodeType() const -> bool { return false; }
  virtual auto isVirtualFolderType() const -> bool { return false; }
  auto priority() const -> int;
  auto parentProjectNode() const -> ProjectNode*; // parent project, will be nullptr for the top-level project
  auto parentFolderNode() const -> FolderNode*;   // parent folder or project
  auto managingProject() -> ProjectNode*; // project managing this node.
  // result is the container's rootProject node if this is a project container node
  // (i.e. possibly null)
  // or node if node is a top-level ProjectNode directly below a container
  // or node->parentProjectNode() for all other cases.
  auto managingProject() const -> const ProjectNode*; // see above.
  auto getProject() const -> Project*;
  auto filePath() const -> const Utils::FilePath&; // file system path
  auto line() const -> int;
  virtual auto displayName() const -> QString;
  virtual auto tooltip() const -> QString;
  auto isEnabled() const -> bool;
  auto listInProject() const -> bool;
  auto isGenerated() const -> bool;
  virtual auto supportsAction(ProjectAction action, const Node *node) const -> bool;
  auto setEnabled(bool enabled) -> void;
  auto setAbsoluteFilePathAndLine(const Utils::FilePath &filePath, int line) -> void;
  virtual auto asFileNode() -> FileNode* { return nullptr; }
  virtual auto asFileNode() const -> const FileNode* { return nullptr; }
  virtual auto asFolderNode() -> FolderNode* { return nullptr; }
  virtual auto asFolderNode() const -> const FolderNode* { return nullptr; }
  virtual auto asProjectNode() -> ProjectNode* { return nullptr; }
  virtual auto asProjectNode() const -> const ProjectNode* { return nullptr; }
  virtual auto asContainerNode() -> ContainerNode* { return nullptr; }
  virtual auto asContainerNode() const -> const ContainerNode* { return nullptr; }
  virtual auto buildKey() const -> QString { return QString(); }
  static auto sortByPath(const Node *a, const Node *b) -> bool;
  auto setParentFolderNode(FolderNode *parentFolder) -> void;
  auto setListInProject(bool l) -> void;
  auto setIsGenerated(bool g) -> void;
  auto setPriority(int priority) -> void;
  auto setLine(int line) -> void;
  static auto fileTypeForMimeType(const Utils::MimeType &mt) -> FileType;
  static auto fileTypeForFileName(const Utils::FilePath &file) -> FileType;
  auto path() const -> Utils::FilePath { return pathOrDirectory(false); }
  auto directory() const -> Utils::FilePath { return pathOrDirectory(true); }

protected:
  Node();
  Node(const Node &other) = delete;

  auto operator=(const Node &other) -> bool = delete;
  auto setFilePath(const Utils::FilePath &filePath) -> void;

private:
  auto pathOrDirectory(bool dir) const -> Utils::FilePath;

  FolderNode *m_parentFolderNode = nullptr;
  Utils::FilePath m_filePath;
  int m_line = -1;
  int m_priority = DefaultPriority;

  enum NodeFlag : quint16 {
    FlagNone = 0,
    FlagIsEnabled = 1 << 0,
    FlagIsGenerated = 1 << 1,
    FlagListInProject = 1 << 2,
  };

  NodeFlag m_flags = FlagIsEnabled;
};

class PROJECTEXPLORER_EXPORT FileNode : public Node {
public:
  FileNode(const Utils::FilePath &filePath, const FileType fileType);

  auto clone() const -> FileNode*;
  auto fileType() const -> FileType;
  auto asFileNode() -> FileNode* final { return this; }
  auto asFileNode() const -> const FileNode* final { return this; }
  auto supportsAction(ProjectAction action, const Node *node) const -> bool override;
  auto displayName() const -> QString override;
  auto hasError() const -> bool;
  auto setHasError(const bool error) -> void;
  auto setHasError(const bool error) const -> void;
  auto icon() const -> QIcon;
  auto setIcon(const QIcon icon) -> void;

private:
  FileType m_fileType;
  mutable QIcon m_icon;
  mutable bool m_hasError = false;
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT FolderNode : public Node {
public:
  explicit FolderNode(const Utils::FilePath &folderPath);

  auto displayName() const -> QString override;
  // only safe from UI thread
  auto icon() const -> QIcon;
  auto isFolderNodeType() const -> bool override { return true; }
  auto findNode(const std::function<bool(Node *)> &filter) -> Node*;
  auto findNodes(const std::function<bool(Node *)> &filter) -> QList<Node*>;
  auto forEachNode(const std::function<void(FileNode *)> &fileTask, const std::function<void(FolderNode *)> &folderTask = {}, const std::function<bool(const FolderNode *)> &folderFilterTask = {}) const -> void;
  auto forEachGenericNode(const std::function<void(Node *)> &genericTask) const -> void;
  auto forEachProjectNode(const std::function<void(const ProjectNode *)> &genericTask) const -> void;
  auto findProjectNode(const std::function<bool(const ProjectNode *)> &predicate) -> ProjectNode*;
  auto nodes() const -> const QList<Node*>;
  auto fileNodes() const -> QList<FileNode*>;
  auto fileNode(const Utils::FilePath &file) const -> FileNode*;
  auto folderNodes() const -> QList<FolderNode*>;
  auto folderNode(const Utils::FilePath &directory) const -> FolderNode*;
  using FolderNodeFactory = std::function<std::unique_ptr<FolderNode>(const Utils::FilePath &)>;
  auto addNestedNodes(std::vector<std::unique_ptr<FileNode>> &&files, const Utils::FilePath &overrideBaseDir = Utils::FilePath(), const FolderNodeFactory &factory = [](const Utils::FilePath &fn) { return std::make_unique<FolderNode>(fn); }) -> void;
  auto addNestedNode(std::unique_ptr<FileNode> &&fileNode, const Utils::FilePath &overrideBaseDir = Utils::FilePath(), const FolderNodeFactory &factory = [](const Utils::FilePath &fn) { return std::make_unique<FolderNode>(fn); }) -> void;
  auto compress() -> void;
  // takes ownership of newNode.
  // Will delete newNode if oldNode is not a child of this node.
  auto replaceSubtree(Node *oldNode, std::unique_ptr<Node> &&newNode) -> bool;
  auto setDisplayName(const QString &name) -> void;
  // you have to make sure the QIcon is created in the UI thread if you are calling setIcon(QIcon)
  auto setIcon(const QIcon &icon) -> void;
  auto setIcon(const DirectoryIcon &directoryIcon) -> void;
  auto setIcon(const QString &path) -> void;
  auto setIcon(const IconCreator &iconCreator) -> void;

  class LocationInfo {
  public:
    LocationInfo() = default;
    LocationInfo(const QString &dn, const Utils::FilePath &p, const int l = 0, const unsigned int prio = 0) : path(p), line(l), priority(prio), displayName(dn) {}

    Utils::FilePath path;
    int line = -1;
    unsigned int priority = 0;
    QString displayName;
  };

  auto setLocationInfo(const QVector<LocationInfo> &info) -> void;
  auto locationInfo() const -> const QVector<LocationInfo>;
  auto addFileFilter() const -> QString;
  auto setAddFileFilter(const QString &filter) -> void { m_addFileFilter = filter; }
  auto supportsAction(ProjectAction action, const Node *node) const -> bool override;

  virtual auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded = nullptr) -> bool;
  virtual auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved = nullptr) -> RemovedFilesFromProject;
  virtual auto deleteFiles(const Utils::FilePaths &filePaths) -> bool;
  virtual auto canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  virtual auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  virtual auto addDependencies(const QStringList &dependencies) -> bool;

  class AddNewInformation {
  public:
    AddNewInformation(const QString &name, int p) : displayName(name), priority(p) { }
    QString displayName;
    int priority;
  };

  virtual auto addNewInformation(const Utils::FilePaths &files, Node *context) const -> AddNewInformation;

  // determines if node will be shown in the flat view, by default folder and projects aren't shown
  virtual auto showInSimpleTree() const -> bool;

  // determines if node will always be shown when hiding empty directories
  auto showWhenEmpty() const -> bool;
  auto setShowWhenEmpty(bool showWhenEmpty) -> void;
  auto addNode(std::unique_ptr<Node> &&node) -> void;
  auto isEmpty() const -> bool;
  auto asFolderNode() -> FolderNode* override { return this; }
  auto asFolderNode() const -> const FolderNode* override { return this; }

protected:
  virtual auto handleSubTreeChanged(FolderNode *node) -> void;

  std::vector<std::unique_ptr<Node>> m_nodes;
  QVector<LocationInfo> m_locations;

private:
  auto takeNode(Node *node) -> std::unique_ptr<Node>;

  QString m_displayName;
  QString m_addFileFilter;
  mutable Utils::variant<QIcon, DirectoryIcon, QString, IconCreator> m_icon;
  bool m_showWhenEmpty = false;
};

class PROJECTEXPLORER_EXPORT VirtualFolderNode : public FolderNode {
public:
  explicit VirtualFolderNode(const Utils::FilePath &folderPath);

  auto isFolderNodeType() const -> bool override { return false; }
  auto isVirtualFolderType() const -> bool override { return true; }
  auto isSourcesOrHeaders() const -> bool { return m_isSourcesOrHeaders; }
  auto setIsSourcesOrHeaders(bool on) -> void { m_isSourcesOrHeaders = on; }

private:
  bool m_isSourcesOrHeaders = false; // "Sources" or "Headers"
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT ProjectNode : public FolderNode {
public:
  explicit ProjectNode(const Utils::FilePath &projectFilePath);

  virtual auto canAddSubProject(const Utils::FilePath &proFilePath) const -> bool;
  virtual auto addSubProject(const Utils::FilePath &proFile) -> bool;
  virtual auto subProjectFileNamePatterns() const -> QStringList;
  virtual auto removeSubProject(const Utils::FilePath &proFilePath) -> bool;

  virtual auto visibleAfterAddFileAction() const -> Utils::optional<Utils::FilePath>
  {
    return Utils::nullopt;
  }

  auto isFolderNodeType() const -> bool override { return false; }
  auto isProjectNodeType() const -> bool override { return true; }
  auto showInSimpleTree() const -> bool override { return true; }
  auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded = nullptr) -> bool final;
  auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved = nullptr) -> RemovedFilesFromProject final;
  auto deleteFiles(const Utils::FilePaths &filePaths) -> bool final;
  auto canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool final;
  auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool final;
  auto addDependencies(const QStringList &dependencies) -> bool final;
  auto supportsAction(ProjectAction action, const Node *node) const -> bool final;
  // by default returns false
  virtual auto deploysFolder(const QString &folder) const -> bool;
  auto projectNode(const Utils::FilePath &file) const -> ProjectNode*;
  auto asProjectNode() -> ProjectNode* final { return this; }
  auto asProjectNode() const -> const ProjectNode* final { return this; }
  virtual auto targetApplications() const -> QStringList { return {}; }
  virtual auto parseInProgress() const -> bool { return false; }
  virtual auto validParse() const -> bool { return false; }
  virtual auto data(Utils::Id role) const -> QVariant;
  virtual auto setData(Utils::Id role, const QVariant &value) const -> bool;
  auto isProduct() const -> bool { return m_productType != ProductType::None; }
  auto productType() const -> ProductType { return m_productType; }
  // TODO: Currently used only for "Build for current run config" functionality, but we should
  //       probably use it to centralize the node-specific "Build" functionality that
  //       currently each project manager plugin adds to the context menu by itself.
  //       The function should then move up to the Node class, so it can also serve the
  //       "build single file" case.
  virtual auto build() -> void {}
  auto setFallbackData(Utils::Id key, const QVariant &value) -> void;

protected:
  auto setProductType(ProductType type) -> void { m_productType = type; }

  QString m_target;

private:
  auto buildSystem() const -> BuildSystem*;

  QHash<Utils::Id, QVariant> m_fallbackData; // Used in data(), unless overridden.
  ProductType m_productType = ProductType::None;
};

class PROJECTEXPLORER_EXPORT ContainerNode : public FolderNode {
public:
  ContainerNode(Project *project);

  auto displayName() const -> QString final;
  auto supportsAction(ProjectAction action, const Node *node) const -> bool final;
  auto isFolderNodeType() const -> bool override { return false; }
  auto isProjectNodeType() const -> bool override { return true; }
  auto asContainerNode() -> ContainerNode* final { return this; }
  auto asContainerNode() const -> const ContainerNode* final { return this; }
  auto rootProjectNode() const -> ProjectNode*;
  auto project() const -> Project* { return m_project; }
  auto removeAllChildren() -> void;

private:
  auto handleSubTreeChanged(FolderNode *node) -> void final;

  Project *m_project;
};

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Node *)
Q_DECLARE_METATYPE(ProjectExplorer::FolderNode *)
