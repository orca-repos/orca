// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppmodelmanager.hpp"
#include "searchsymbols.hpp"

#include <cplusplus/CppDocument.h>

#include <QHash>

namespace CppEditor {

class CPPEDITOR_EXPORT CppLocatorData : public QObject {
  Q_OBJECT

  // Only one instance, created by the CppModelManager.
  CppLocatorData();
  friend class Internal::CppModelManagerPrivate;

public:
  auto filterAllFiles(IndexItem::Visitor func) const -> void
  {
    QMutexLocker locker(&m_pendingDocumentsMutex);
    flushPendingDocument(true);
    auto infosByFile = m_infosByFile;
    locker.unlock();
    for (auto i = infosByFile.constBegin(), ei = infosByFile.constEnd(); i != ei; ++i)
      if (i.value()->visitAllChildren(func) == IndexItem::Break)
        return;
  }

public slots:
  auto onDocumentUpdated(const CPlusPlus::Document::Ptr &document) -> void;
  auto onAboutToRemoveFiles(const QStringList &files) -> void;

private:
  // Ensure to protect every call to this method with m_pendingDocumentsMutex
  auto flushPendingDocument(bool force) const -> void;

  mutable SearchSymbols m_search;
  mutable QHash<QString, IndexItem::Ptr> m_infosByFile;
  mutable QMutex m_pendingDocumentsMutex;
  mutable QVector<CPlusPlus::Document::Ptr> m_pendingDocuments;
};

} // namespace CppEditor
