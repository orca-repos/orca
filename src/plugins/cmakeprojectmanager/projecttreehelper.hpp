// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprojectnodes.hpp"

#include <utils/fileutils.hpp>

#include <memory>

namespace CMakeProjectManager {
namespace Internal {

auto createCMakeVFolder(const Utils::FilePath &basePath, int priority, const QString &displayName) -> std::unique_ptr<ProjectExplorer::FolderNode>;
auto addCMakeVFolder(ProjectExplorer::FolderNode *base, const Utils::FilePath &basePath, int priority, const QString &displayName, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&files) -> void;
auto removeKnownNodes(const QSet<Utils::FilePath> &knownFiles, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&files) -> std::vector<std::unique_ptr<ProjectExplorer::FileNode>>&&;
auto addCMakeInputs(ProjectExplorer::FolderNode *root, const Utils::FilePath &sourceDir, const Utils::FilePath &buildDir, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&sourceInputs, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&buildInputs, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&rootInputs) -> void;
auto addCMakeLists(CMakeProjectNode *root, std::vector<std::unique_ptr<ProjectExplorer::FileNode>> &&cmakeLists) -> QHash<Utils::FilePath, ProjectExplorer::ProjectNode*>;
auto createProjectNode(const QHash<Utils::FilePath, ProjectExplorer::ProjectNode*> &cmakeListsNodes, const Utils::FilePath &dir, const QString &displayName) -> void;
auto createTargetNode(const QHash<Utils::FilePath, ProjectExplorer::ProjectNode*> &cmakeListsNodes, const Utils::FilePath &dir, const QString &displayName) -> CMakeTargetNode*;
auto addFileSystemNodes(ProjectExplorer::ProjectNode *root, const std::shared_ptr<ProjectExplorer::FolderNode> &folderNode) -> void;

} // namespace Internal
} // namespace CMakeProjectManager
