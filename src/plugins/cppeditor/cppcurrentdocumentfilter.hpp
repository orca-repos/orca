// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchsymbols.hpp"

#include <core/locator/ilocatorfilter.hpp>

namespace Core {
class IEditor;
}

namespace CppEditor {

class CppModelManager;

namespace Internal {

class CppCurrentDocumentFilter : public Core::ILocatorFilter {
  Q_OBJECT

public:
  explicit CppCurrentDocumentFilter(CppModelManager *manager);
  ~CppCurrentDocumentFilter() override = default;

  auto matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Core::LocatorFilterEntry> override;
  auto accept(const Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void override;

private:
  auto onDocumentUpdated(CPlusPlus::Document::Ptr doc) -> void;
  auto onCurrentEditorChanged(Core::IEditor *currentEditor) -> void;
  auto onEditorAboutToClose(Core::IEditor *currentEditor) -> void;
  auto itemsOfCurrentDocument() -> QList<IndexItem::Ptr>;

  CppModelManager *m_modelManager;
  SearchSymbols search;
  mutable QMutex m_mutex;
  QString m_currentFileName;
  QList<IndexItem::Ptr> m_itemsOfCurrentDoc;
};

} // namespace Internal
} // namespace CppEditor
