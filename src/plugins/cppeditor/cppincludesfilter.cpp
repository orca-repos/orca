// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppincludesfilter.hpp"

#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"

#include <cplusplus/CppDocument.h>
#include <core/core-document-model.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/session.hpp>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CppEditor::Internal {

class CppIncludesIterator final : public BaseFileFilter::Iterator {
public:
  CppIncludesIterator(CPlusPlus::Snapshot snapshot, const QSet<QString> &seedPaths);

  auto toFront() -> void override;
  auto hasNext() const -> bool override;
  auto next() -> Utils::FilePath override;
  auto filePath() const -> Utils::FilePath override;

private:
  auto fetchMore() -> void;

  CPlusPlus::Snapshot m_snapshot;
  QSet<QString> m_paths;
  QSet<QString> m_queuedPaths;
  QSet<QString> m_allResultPaths;
  QStringList m_resultQueue;
  FilePath m_currentPath;
};

CppIncludesIterator::CppIncludesIterator(CPlusPlus::Snapshot snapshot, const QSet<QString> &seedPaths) : m_snapshot(snapshot), m_paths(seedPaths)
{
  toFront();
}

auto CppIncludesIterator::toFront() -> void
{
  m_queuedPaths = m_paths;
  m_allResultPaths.clear();
  m_resultQueue.clear();
  fetchMore();
}

auto CppIncludesIterator::hasNext() const -> bool
{
  return !m_resultQueue.isEmpty();
}

auto CppIncludesIterator::next() -> FilePath
{
  if (m_resultQueue.isEmpty())
    return {};
  m_currentPath = FilePath::fromString(m_resultQueue.takeFirst());
  if (m_resultQueue.isEmpty())
    fetchMore();
  return m_currentPath;
}

auto CppIncludesIterator::filePath() const -> FilePath
{
  return m_currentPath;
}

auto CppIncludesIterator::fetchMore() -> void
{
  while (!m_queuedPaths.isEmpty() && m_resultQueue.isEmpty()) {
    const auto filePath = *m_queuedPaths.begin();
    m_queuedPaths.remove(filePath);
    CPlusPlus::Document::Ptr doc = m_snapshot.document(filePath);
    if (!doc)
      continue;
    const QStringList includedFiles = doc->includedFiles();
    for (const auto &includedPath : includedFiles) {
      if (!m_allResultPaths.contains(includedPath)) {
        m_allResultPaths.insert(includedPath);
        m_queuedPaths.insert(includedPath);
        m_resultQueue.append(includedPath);
      }
    }
  }
}

CppIncludesFilter::CppIncludesFilter()
{
  setId(Constants::INCLUDES_FILTER_ID);
  setDisplayName(Constants::INCLUDES_FILTER_DISPLAY_NAME);
  setDescription(tr("Matches all files that are included by all C++ files in all projects. Append " "\"+<number>\" or \":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
  setDefaultShortcutString("ai");
  setDefaultIncludedByDefault(true);
  setPriority(ILocatorFilter::Low);

  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::fileListChanged, this, &CppIncludesFilter::markOutdated);
  connect(CppModelManager::instance(), &CppModelManager::documentUpdated, this, &CppIncludesFilter::markOutdated);
  connect(CppModelManager::instance(), &CppModelManager::aboutToRemoveFiles, this, &CppIncludesFilter::markOutdated);
  connect(DocumentModel::model(), &QAbstractItemModel::rowsInserted, this, &CppIncludesFilter::markOutdated);
  connect(DocumentModel::model(), &QAbstractItemModel::rowsRemoved, this, &CppIncludesFilter::markOutdated);
  connect(DocumentModel::model(), &QAbstractItemModel::dataChanged, this, &CppIncludesFilter::markOutdated);
  connect(DocumentModel::model(), &QAbstractItemModel::modelReset, this, &CppIncludesFilter::markOutdated);
}

auto CppIncludesFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  if (m_needsUpdate) {
    m_needsUpdate = false;
    QSet<QString> seedPaths;
    for (auto project : SessionManager::projects()) {
      const auto allFiles = project->files(Project::SourceFiles);
      for (const auto &filePath : allFiles)
        seedPaths.insert(filePath.toString());
    }
    const auto entries = DocumentModel::entries();
    for (auto entry : entries) {
      if (entry)
        seedPaths.insert(entry->fileName().toString());
    }
    CPlusPlus::Snapshot snapshot = CppModelManager::instance()->snapshot();
    setFileIterator(new CppIncludesIterator(snapshot, seedPaths));
  }
  BaseFileFilter::prepareSearch(entry);
}

auto CppIncludesFilter::refresh(QFutureInterface<void> &future) -> void
{
  Q_UNUSED(future)
  QMetaObject::invokeMethod(this, &CppIncludesFilter::markOutdated, Qt::QueuedConnection);
}

auto CppIncludesFilter::markOutdated() -> void
{
  m_needsUpdate = true;
  setFileIterator(nullptr); // clean up
}

} // namespace CppEditor::Internal
