// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "resource_global.hpp"
#include <projectexplorer/projectnodes.hpp>

namespace ResourceEditor {
namespace Internal {
class ResourceFileWatcher;
}

class RESOURCE_EXPORT ResourceTopLevelNode : public ProjectExplorer::FolderNode {
public:
  ResourceTopLevelNode(const Utils::FilePath &filePath, const Utils::FilePath &basePath, const QString &contents = {});
  ~ResourceTopLevelNode() override;

  auto setupWatcherIfNeeded() -> void;
  auto addInternalNodes() -> void;
  auto supportsAction(ProjectExplorer::ProjectAction action, const Node *node) const -> bool override;
  auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded) -> bool override;
  auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved) -> ProjectExplorer::RemovedFilesFromProject override;
  auto addPrefix(const QString &prefix, const QString &lang) -> bool;
  auto removePrefix(const QString &prefix, const QString &lang) -> bool;
  auto addNewInformation(const Utils::FilePaths &files, Node *context) const -> AddNewInformation override;
  auto showInSimpleTree() const -> bool override;
  auto removeNonExistingFiles() -> bool;
  auto contents() const -> QString { return m_contents; }

private:
  Internal::ResourceFileWatcher *m_document = nullptr;
  QString m_contents;
};

class RESOURCE_EXPORT ResourceFolderNode : public ProjectExplorer::FolderNode {
public:
  ResourceFolderNode(const QString &prefix, const QString &lang, ResourceTopLevelNode *parent);
  ~ResourceFolderNode() override;

  auto supportsAction(ProjectExplorer::ProjectAction action, const Node *node) const -> bool override;
  auto displayName() const -> QString override;
  auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded) -> bool override;
  auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved) -> ProjectExplorer::RemovedFilesFromProject override;
  auto canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool override;
  auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool override;
  auto renamePrefix(const QString &prefix, const QString &lang) -> bool;
  auto addNewInformation(const Utils::FilePaths &files, Node *context) const -> AddNewInformation override;
  auto prefix() const -> QString;
  auto lang() const -> QString;
  auto resourceNode() const -> ResourceTopLevelNode*;

private:
  ResourceTopLevelNode *m_topLevelNode;
  QString m_prefix;
  QString m_lang;
};

class RESOURCE_EXPORT ResourceFileNode : public ProjectExplorer::FileNode {
public:
  ResourceFileNode(const Utils::FilePath &filePath, const QString &qrcPath, const QString &displayName);

  auto displayName() const -> QString override;
  auto qrcPath() const -> QString;
  auto supportsAction(ProjectExplorer::ProjectAction action, const Node *node) const -> bool override;

private:
  QString m_qrcPath;
  QString m_displayName;
};

} // namespace ResourceEditor
