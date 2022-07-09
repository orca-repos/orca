// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpplocatordata.hpp"

#include "stringtable.hpp"

namespace CppEditor {

enum {
  MaxPendingDocuments = 10
};

CppLocatorData::CppLocatorData()
{
  m_search.setSymbolsToSearchFor(SymbolSearcher::Enums | SymbolSearcher::Classes | SymbolSearcher::Functions | SymbolSearcher::TypeAliases);
  m_pendingDocuments.reserve(MaxPendingDocuments);
}

auto CppLocatorData::onDocumentUpdated(const CPlusPlus::Document::Ptr &document) -> void
{
  QMutexLocker locker(&m_pendingDocumentsMutex);

  auto isPending = false;
  for (int i = 0, ei = m_pendingDocuments.size(); i < ei; ++i) {
    const CPlusPlus::Document::Ptr &doc = m_pendingDocuments.at(i);
    if (doc->fileName() == document->fileName()) {
      isPending = true;
      if (document->revision() >= doc->revision())
        m_pendingDocuments[i] = document;
      break;
    }
  }

  if (!isPending && QFileInfo(document->fileName()).suffix() != "moc")
    m_pendingDocuments.append(document);

  flushPendingDocument(false);
}

auto CppLocatorData::onAboutToRemoveFiles(const QStringList &files) -> void
{
  if (files.isEmpty())
    return;

  QMutexLocker locker(&m_pendingDocumentsMutex);

  foreach(const QString &file, files) {
    m_infosByFile.remove(file);

    for (auto i = 0; i < m_pendingDocuments.size(); ++i) {
      if (m_pendingDocuments.at(i)->fileName() == file) {
        m_pendingDocuments.remove(i);
        break;
      }
    }
  }

  Internal::StringTable::scheduleGC();
  flushPendingDocument(false);
}

auto CppLocatorData::flushPendingDocument(bool force) const -> void
{
  // TODO: move this off the UI thread and into a future.
  if (!force && m_pendingDocuments.size() < MaxPendingDocuments)
    return;
  if (m_pendingDocuments.isEmpty())
    return;

  foreach(CPlusPlus::Document::Ptr doc, m_pendingDocuments)
    m_infosByFile.insert(Internal::StringTable::insert(doc->fileName()), m_search(doc));

  m_pendingDocuments.clear();
  m_pendingDocuments.reserve(MaxPendingDocuments);
}

} // namespace CppEditor
