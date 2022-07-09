// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourcenode.hpp"
#include "resourceeditorconstants.hpp"
#include "qrceditor/resourcefile_p.hpp"

#include <core/documentmanager.hpp>
#include <core/fileiconprovider.hpp>

#include <qmljstools/qmljstoolsconstants.h>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/threadutils.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDebug>

#include <limits>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

using namespace ResourceEditor::Internal;

namespace ResourceEditor {
namespace Internal {

class ResourceFileWatcher : public IDocument {
public:
  ResourceFileWatcher(ResourceTopLevelNode *node) : IDocument(nullptr), m_node(node)
  {
    setId("ResourceNodeWatcher");
    setMimeType(ResourceEditor::Constants::C_RESOURCE_MIMETYPE);
    setFilePath(node->filePath());
  }

  auto reloadBehavior(ChangeTrigger, ChangeType) const -> ReloadBehavior final
  {
    return BehaviorSilent;
  }

  auto reload(QString *, ReloadFlag, ChangeType type) -> bool final
  {
    Q_UNUSED(type)
    auto parent = m_node->parentFolderNode();
    QTC_ASSERT(parent, return false);
    parent->replaceSubtree(m_node, std::make_unique<ResourceTopLevelNode>(m_node->filePath(), parent->filePath(), m_node->contents()));
    return true;
  }

private:
  ResourceTopLevelNode *m_node;
};

class PrefixFolderLang {
public:
  PrefixFolderLang(const QString &prefix, const QString &folder, const QString &lang) : m_prefix(prefix), m_folder(folder), m_lang(lang) {}

  auto operator<(const PrefixFolderLang &other) const -> bool
  {
    if (m_prefix != other.m_prefix)
      return m_prefix < other.m_prefix;
    if (m_folder != other.m_folder)
      return m_folder < other.m_folder;
    if (m_lang != other.m_lang)
      return m_lang < other.m_lang;
    return false;
  }

private:
  QString m_prefix;
  QString m_folder;
  QString m_lang;
};

static auto getPriorityFromContextNode(const ProjectExplorer::Node *resourceNode, const ProjectExplorer::Node *contextNode) -> int
{
  if (contextNode == resourceNode)
    return std::numeric_limits<int>::max();
  for (auto n = contextNode; n; n = n->parentFolderNode()) {
    if (n == resourceNode)
      return std::numeric_limits<int>::max() - 1;
  }
  return -1;
}

static auto hasPriority(const FilePaths &files) -> bool
{
  if (files.isEmpty())
    return false;
  auto type = Utils::mimeTypeForFile(files.at(0)).name();
  if (type.startsWith(QLatin1String("image/")) || type == QLatin1String(QmlJSTools::Constants::QML_MIMETYPE) || type == QLatin1String(QmlJSTools::Constants::QMLUI_MIMETYPE) || type == QLatin1String(QmlJSTools::Constants::JS_MIMETYPE))
    return true;
  return false;
}

static auto addFilesToResource(const FilePath &resourceFile, const FilePaths &filePaths, FilePaths *notAdded, const QString &prefix, const QString &lang) -> bool
{
  if (notAdded)
    *notAdded = filePaths;

  ResourceFile file(resourceFile);
  if (file.load() != IDocument::OpenResult::Success)
    return false;

  auto index = file.indexOfPrefix(prefix, lang);
  if (index == -1)
    index = file.addPrefix(prefix, lang);

  if (notAdded)
    notAdded->clear();
  for (const auto &path : filePaths) {
    if (file.contains(index, path.toString())) {
      if (notAdded)
        *notAdded << path;
    } else {
      file.addFile(index, path.toString());
    }
  }

  file.save();

  return true;
}

class SimpleResourceFolderNode : public FolderNode {
  friend class ResourceEditor::ResourceTopLevelNode;
public:
  SimpleResourceFolderNode(const QString &afolderName, const QString &displayName, const QString &prefix, const QString &lang, FilePath absolutePath, ResourceTopLevelNode *topLevel, ResourceFolderNode *prefixNode);

  auto supportsAction(ProjectAction, const Node *node) const -> bool final;
  auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded) -> bool final;
  auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved) -> RemovedFilesFromProject final;
  auto canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool override;
  auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool final;

  auto prefix() const -> QString { return m_prefix; }
  auto resourceNode() const -> ResourceTopLevelNode* { return m_topLevelNode; }
  auto prefixNode() const -> ResourceFolderNode* { return m_prefixNode; }

private:
  QString m_folderName;
  QString m_prefix;
  QString m_lang;
  ResourceTopLevelNode *m_topLevelNode;
  ResourceFolderNode *m_prefixNode;
};

SimpleResourceFolderNode::SimpleResourceFolderNode(const QString &afolderName, const QString &displayName, const QString &prefix, const QString &lang, FilePath absolutePath, ResourceTopLevelNode *topLevel, ResourceFolderNode *prefixNode) : FolderNode(absolutePath), m_folderName(afolderName), m_prefix(prefix), m_lang(lang), m_topLevelNode(topLevel), m_prefixNode(prefixNode)
{
  setDisplayName(displayName);
}

auto SimpleResourceFolderNode::supportsAction(ProjectAction action, const Node *) const -> bool
{
  return action == AddNewFile || action == AddExistingFile || action == AddExistingDirectory || action == RemoveFile || action == Rename // Note: only works for the filename, works akwardly for relative file paths
    || action == InheritedFromParent;                                                                                                    // Do not add to list of projects when adding new file
}

auto SimpleResourceFolderNode::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  return addFilesToResource(m_topLevelNode->filePath(), filePaths, notAdded, m_prefix, m_lang);
}

auto SimpleResourceFolderNode::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  return prefixNode()->removeFiles(filePaths, notRemoved);
}

auto SimpleResourceFolderNode::canRenameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  return prefixNode()->canRenameFile(oldFilePath, newFilePath);
}

auto SimpleResourceFolderNode::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  return prefixNode()->renameFile(oldFilePath, newFilePath);
}

} // Internal

ResourceTopLevelNode::ResourceTopLevelNode(const FilePath &filePath, const FilePath &base, const QString &contents) : FolderNode(filePath)
{
  setIcon([filePath] { return FileIconProvider::icon(filePath); });
  setPriority(Node::DefaultFilePriority);
  setListInProject(true);
  setAddFileFilter("*.png; *.jpg; *.gif; *.svg; *.ico; *.qml; *.qml.ui");
  setShowWhenEmpty(true);

  if (!filePath.isEmpty()) {
    if (filePath.isReadableFile())
      setupWatcherIfNeeded();
  } else {
    m_contents = contents;
  }

  if (filePath.isChildOf(base))
    setDisplayName(filePath.relativeChildPath(base).toUserOutput());
  else
    setDisplayName(filePath.toUserOutput());

  addInternalNodes();
}

auto ResourceTopLevelNode::setupWatcherIfNeeded() -> void
{
  if (m_document || !isMainThread())
    return;

  m_document = new ResourceFileWatcher(this);
  DocumentManager::addDocument(m_document);
}

ResourceTopLevelNode::~ResourceTopLevelNode()
{
  if (m_document)
    DocumentManager::removeDocument(m_document);
  delete m_document;
}

static auto compressTree(FolderNode *n) -> void
{
  if (const auto compressable = dynamic_cast<SimpleResourceFolderNode*>(n)) {
    compressable->compress();
    return;
  }
  const auto childFolders = n->folderNodes();
  for (const auto c : childFolders)
    compressTree(c);
}

auto ResourceTopLevelNode::addInternalNodes() -> void
{
  ResourceFile file(filePath(), m_contents);
  if (file.load() != IDocument::OpenResult::Success)
    return;

  QMap<PrefixFolderLang, FolderNode*> folderNodes;

  auto prfxcount = file.prefixCount();
  for (auto i = 0; i < prfxcount; ++i) {
    const auto &prefix = file.prefix(i);
    const auto &lang = file.lang(i);
    // ensure that we don't duplicate prefixes
    PrefixFolderLang prefixId(prefix, QString(), lang);
    if (!folderNodes.contains(prefixId)) {
      auto fn = std::make_unique<ResourceFolderNode>(file.prefix(i), file.lang(i), this);
      folderNodes.insert(prefixId, fn.get());
      addNode(std::move(fn));
    }
    auto currentPrefixNode = static_cast<ResourceFolderNode*>(folderNodes[prefixId]);

    QSet<QString> fileNames;
    auto filecount = file.fileCount(i);
    for (auto j = 0; j < filecount; ++j) {
      const auto &fileName = file.file(i, j);
      if (fileNames.contains(fileName)) {
        // The file name is duplicated, skip it
        // Note: this is wrong, but the qrceditor doesn't allow it either
        // only aliases need to be unique
        continue;
      }

      auto alias = file.alias(i, j);
      if (alias.isEmpty())
        alias = filePath().toFileInfo().absoluteDir().relativeFilePath(fileName);

      auto prefixWithSlash = prefix;
      if (!prefixWithSlash.endsWith(QLatin1Char('/')))
        prefixWithSlash.append(QLatin1Char('/'));

      const auto fullPath = QDir::cleanPath(alias);
      auto pathList = fullPath.split(QLatin1Char('/'));
      const auto displayName = pathList.last();
      pathList.removeLast(); // remove file name

      auto parentIsPrefix = true;

      QString parentFolderName;
      PrefixFolderLang folderId(prefix, QString(), lang);
      QStringList currentPathList;
      foreach(const QString &pathElement, pathList) {
        currentPathList << pathElement;
        const auto folderName = currentPathList.join(QLatin1Char('/'));
        folderId = PrefixFolderLang(prefix, folderName, lang);
        if (!folderNodes.contains(folderId)) {
          const auto absoluteFolderName = filePath().toFileInfo().absoluteDir().absoluteFilePath(currentPathList.join(QLatin1Char('/')));
          const auto folderPath = FilePath::fromString(absoluteFolderName);
          std::unique_ptr<FolderNode> newNode = std::make_unique<SimpleResourceFolderNode>(folderName, pathElement, prefix, lang, folderPath, this, currentPrefixNode);
          folderNodes.insert(folderId, newNode.get());

          auto thisPrefixId = prefixId;
          if (!parentIsPrefix)
            thisPrefixId = PrefixFolderLang(prefix, parentFolderName, lang);
          auto fn = folderNodes[thisPrefixId];
          if (QTC_GUARD(fn))
            fn->addNode(std::move(newNode));
        }
        parentIsPrefix = false;
        parentFolderName = folderName;
      }

      const auto qrcPath = QDir::cleanPath(prefixWithSlash + alias);
      fileNames.insert(fileName);
      auto fn = folderNodes[folderId];
      QTC_CHECK(fn);
      if (fn)
        fn->addNode(std::make_unique<ResourceFileNode>(FilePath::fromString(fileName), qrcPath, displayName));
    }
  }
  compressTree(this);
}

auto ResourceTopLevelNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  if (node != this)
    return false;
  return action == AddNewFile || action == AddExistingFile || action == AddExistingDirectory || action == HidePathActions || action == Rename;
}

auto ResourceTopLevelNode::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  return addFilesToResource(filePath(), filePaths, notAdded, "/", QString());
}

auto ResourceTopLevelNode::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  return parentFolderNode()->removeFiles(filePaths, notRemoved);
}

auto ResourceTopLevelNode::addPrefix(const QString &prefix, const QString &lang) -> bool
{
  ResourceFile file(filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return false;
  auto index = file.addPrefix(prefix, lang);
  if (index == -1)
    return false;
  file.save();

  return true;
}

auto ResourceTopLevelNode::removePrefix(const QString &prefix, const QString &lang) -> bool
{
  ResourceFile file(filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return false;
  for (auto i = 0; i < file.prefixCount(); ++i) {
    if (file.prefix(i) == prefix && file.lang(i) == lang) {
      file.removePrefix(i);
      file.save();
      return true;
    }
  }
  return false;
}

auto ResourceTopLevelNode::removeNonExistingFiles() -> bool
{
  ResourceFile file(filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return false;

  QFileInfo fi;

  for (auto i = 0; i < file.prefixCount(); ++i) {
    auto fileCount = file.fileCount(i);
    for (auto j = fileCount - 1; j >= 0; --j) {
      fi.setFile(file.file(i, j));
      if (!fi.exists())
        file.removeFile(i, j);
    }
  }

  file.save();
  return true;
}

auto ResourceTopLevelNode::addNewInformation(const FilePaths &files, Node *context) const -> FolderNode::AddNewInformation
{
  auto name = QCoreApplication::translate("ResourceTopLevelNode", "%1 Prefix: %2").arg(filePath().fileName()).arg(QLatin1Char('/'));

  auto p = getPriorityFromContextNode(this, context);
  if (p == -1 && hasPriority(files)) {
    // images/* and qml/js mimetypes
    p = 110;
    if (context == this)
      p = 120;
    else if (parentProjectNode() == context)
      p = 150; // steal from our project node
  }

  return AddNewInformation(name, p);
}

auto ResourceTopLevelNode::showInSimpleTree() const -> bool
{
  return true;
}

ResourceFolderNode::ResourceFolderNode(const QString &prefix, const QString &lang, ResourceTopLevelNode *parent) : FolderNode(parent->filePath().pathAppended(prefix)),
                                                                                                                   // TOOD Why add existing directory doesn't work
                                                                                                                   m_topLevelNode(parent), m_prefix(prefix), m_lang(lang) {}

ResourceFolderNode::~ResourceFolderNode() = default;

auto ResourceFolderNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  Q_UNUSED(node)

  if (action == InheritedFromParent) {
    // if the prefix is '/' (without lang) hide this node in add new dialog,
    // as the ResouceTopLevelNode is always shown for the '/' prefix
    return m_prefix == QLatin1String("/") && m_lang.isEmpty();
  }

  return action == AddNewFile || action == AddExistingFile || action == AddExistingDirectory || action == RemoveFile || action == Rename // Note: only works for the filename, works akwardly for relative file paths
    || action == HidePathActions;                                                                                                        // hides open terminal etc.
}

auto ResourceFolderNode::addFiles(const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  return addFilesToResource(m_topLevelNode->filePath(), filePaths, notAdded, m_prefix, m_lang);
}

auto ResourceFolderNode::removeFiles(const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  if (notRemoved)
    *notRemoved = filePaths;
  ResourceFile file(m_topLevelNode->filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return RemovedFilesFromProject::Error;
  auto index = file.indexOfPrefix(m_prefix, m_lang);
  if (index == -1)
    return RemovedFilesFromProject::Error;
  for (auto j = 0; j < file.fileCount(index); ++j) {
    auto fileName = file.file(index, j);
    if (!filePaths.contains(FilePath::fromString(fileName)))
      continue;
    if (notRemoved)
      notRemoved->removeOne(FilePath::fromString(fileName));
    file.removeFile(index, j);
    --j;
  }
  FileChangeBlocker changeGuard(m_topLevelNode->filePath());
  file.save();

  return RemovedFilesFromProject::Ok;
}

// QTCREATORBUG-15280
auto ResourceFolderNode::canRenameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  Q_UNUSED(newFilePath)

  auto fileEntryExists = false;
  ResourceFile file(m_topLevelNode->filePath());

  auto index = (file.load() != IDocument::OpenResult::Success) ? -1 : file.indexOfPrefix(m_prefix, m_lang);
  if (index != -1) {
    for (auto j = 0; j < file.fileCount(index); ++j) {
      if (file.file(index, j) == oldFilePath.toString()) {
        fileEntryExists = true;
        break;
      }
    }
  }

  return fileEntryExists;
}

auto ResourceFolderNode::renameFile(const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  ResourceFile file(m_topLevelNode->filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return false;
  auto index = file.indexOfPrefix(m_prefix, m_lang);
  if (index == -1)
    return false;

  for (auto j = 0; j < file.fileCount(index); ++j) {
    if (file.file(index, j) == oldFilePath.toString()) {
      file.replaceFile(index, j, newFilePath.toString());
      FileChangeBlocker changeGuard(m_topLevelNode->filePath());
      file.save();
      return true;
    }
  }

  return false;
}

auto ResourceFolderNode::renamePrefix(const QString &prefix, const QString &lang) -> bool
{
  ResourceFile file(m_topLevelNode->filePath());
  if (file.load() != IDocument::OpenResult::Success)
    return false;
  auto index = file.indexOfPrefix(m_prefix, m_lang);
  if (index == -1)
    return false;

  if (!file.replacePrefixAndLang(index, prefix, lang))
    return false;

  file.save();
  return true;
}

auto ResourceFolderNode::addNewInformation(const FilePaths &files, Node *context) const -> FolderNode::AddNewInformation
{
  auto name = QCoreApplication::translate("ResourceTopLevelNode", "%1 Prefix: %2").arg(m_topLevelNode->filePath().fileName()).arg(displayName());

  auto p = getPriorityFromContextNode(this, context);
  if (p == -1 && hasPriority(files)) {
    // image/* and qml/js mimetypes
    p = 105; // prefer against .pro and .pri files
    if (context == this)
      p = 120;

    if (auto sfn = dynamic_cast<SimpleResourceFolderNode*>(context)) {
      if (sfn->prefixNode() == this)
        p = 120;
    }
  }

  return AddNewInformation(name, p);
}

auto ResourceFolderNode::displayName() const -> QString
{
  if (m_lang.isEmpty())
    return m_prefix;
  return m_prefix + QLatin1String(" (") + m_lang + QLatin1Char(')');
}

auto ResourceFolderNode::prefix() const -> QString
{
  return m_prefix;
}

auto ResourceFolderNode::lang() const -> QString
{
  return m_lang;
}

auto ResourceFolderNode::resourceNode() const -> ResourceTopLevelNode*
{
  return m_topLevelNode;
}

ResourceFileNode::ResourceFileNode(const FilePath &filePath, const QString &qrcPath, const QString &displayName) : FileNode(filePath, FileNode::fileTypeForFileName(filePath)), m_qrcPath(qrcPath), m_displayName(displayName) {}

auto ResourceFileNode::displayName() const -> QString
{
  return m_displayName;
}

auto ResourceFileNode::qrcPath() const -> QString
{
  return m_qrcPath;
}

auto ResourceFileNode::supportsAction(ProjectAction action, const Node *node) const -> bool
{
  if (action == HidePathActions)
    return false;
  return parentFolderNode()->supportsAction(action, node);
}

} // ResourceEditor
