// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "treescanner.hpp"

#include "projectexplorerconstants.hpp"
#include "projectnodeshelper.hpp"
#include "projecttree.hpp"

#include <core/core-version-control-interface.hpp>
#include <core/core-vcs-manager.hpp>

#include <cppeditor/cppeditorconstants.hpp>

#include <utils/qtcassert.hpp>
#include <utils/algorithm.hpp>
#include <utils/runextensions.hpp>

#include <memory>

namespace ProjectExplorer {

TreeScanner::TreeScanner(QObject *parent) : QObject(parent)
{
  m_factory = genericFileType;
  m_filter = [](const Utils::MimeType &mimeType, const Utils::FilePath &fn) {
    return isWellKnownBinary(mimeType, fn) && isMimeBinary(mimeType, fn);
  };

  connect(&m_futureWatcher, &FutureWatcher::finished, this, &TreeScanner::finished);
}

TreeScanner::~TreeScanner()
{
  disconnect(&m_futureWatcher, nullptr, nullptr, nullptr); // Do not trigger signals anymore!

  if (!m_futureWatcher.isFinished()) {
    m_futureWatcher.cancel();
    m_futureWatcher.waitForFinished();
  }
}

auto TreeScanner::asyncScanForFiles(const Utils::FilePath &directory) -> bool
{
  if (!m_futureWatcher.isFinished())
    return false;

  m_scanFuture = Utils::runAsync([this, directory](FutureInterface &fi) {
    scanForFiles(fi, directory, m_filter, m_factory);
  });
  m_futureWatcher.setFuture(m_scanFuture);

  return true;
}

auto TreeScanner::setFilter(FileFilter filter) -> void
{
  if (isFinished())
    m_filter = filter;
}

auto TreeScanner::setTypeFactory(FileTypeFactory factory) -> void
{
  if (isFinished())
    m_factory = factory;
}

auto TreeScanner::future() const -> Future
{
  return m_scanFuture;
}

auto TreeScanner::isFinished() const -> bool
{
  return m_futureWatcher.isFinished();
}

auto TreeScanner::result() const -> Result
{
  if (isFinished())
    return m_scanFuture.result();
  return Result();
}

auto TreeScanner::release() -> Result
{
  if (isFinished() && m_scanFuture.resultCount() > 0) {
    auto result = m_scanFuture.result();
    m_scanFuture = Future();
    return result;
  }
  m_scanFuture = Future();
  return Result();
}

auto TreeScanner::reset() -> void
{
  if (isFinished())
    m_scanFuture = Future();
}

auto TreeScanner::isWellKnownBinary(const Utils::MimeType & /*mdb*/, const Utils::FilePath &fn) -> bool
{
  return fn.endsWith(QLatin1String(".a")) || fn.endsWith(QLatin1String(".o")) || fn.endsWith(QLatin1String(".d")) || fn.endsWith(QLatin1String(".exe")) || fn.endsWith(QLatin1String(".dll")) || fn.endsWith(QLatin1String(".obj")) || fn.endsWith(QLatin1String(".elf"));
}

auto TreeScanner::isMimeBinary(const Utils::MimeType &mimeType, const Utils::FilePath &/*fn*/) -> bool
{
  auto isBinary = false;
  if (mimeType.isValid()) {
    QStringList mimes;
    mimes << mimeType.name() << mimeType.allAncestors();
    isBinary = !mimes.contains(QLatin1String("text/plain"));
  }
  return isBinary;
}

auto TreeScanner::genericFileType(const Utils::MimeType &mimeType, const Utils::FilePath &/*fn*/) -> FileType
{
  return Node::fileTypeForMimeType(mimeType);
}

static auto createFolderNode(const Utils::FilePath &directory, const QList<FileNode*> &allFiles) -> std::unique_ptr<FolderNode>
{
  auto fileSystemNode = std::make_unique<FolderNode>(directory);
  for (const FileNode *fn : allFiles) {
    if (!fn->filePath().isChildOf(directory))
      continue;

    std::unique_ptr<FileNode> node(fn->clone());
    fileSystemNode->addNestedNode(std::move(node));
  }
  ProjectTree::applyTreeManager(fileSystemNode.get(), ProjectTree::AsyncPhase); // QRC nodes
  return fileSystemNode;
}

auto TreeScanner::scanForFiles(FutureInterface &fi, const Utils::FilePath &directory, const FileFilter &filter, const FileTypeFactory &factory) -> void
{
  auto nodes = ProjectExplorer::scanForFiles(fi, directory, [&filter, &factory](const Utils::FilePath &fn) -> FileNode* {
    const auto mimeType = mimeTypeForFile(fn);

    // Skip some files during scan.
    if (filter && filter(mimeType, fn))
      return nullptr;

    // Type detection
    auto type = FileType::Unknown;
    if (factory)
      type = factory(mimeType, fn);

    return new FileNode(fn, type);
  });

  Utils::sort(nodes, Node::sortByPath);

  fi.setProgressValue(fi.progressMaximum());
  const Result result{createFolderNode(directory, nodes), nodes};

  fi.reportResult(result);
}

} // namespace ProjectExplorer
