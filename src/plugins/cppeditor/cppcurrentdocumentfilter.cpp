// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcurrentdocumentfilter.hpp"

#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-document-interface.hpp>

#include <QRegularExpression>

using namespace CPlusPlus;

namespace CppEditor::Internal {

CppCurrentDocumentFilter::CppCurrentDocumentFilter(CppModelManager *manager) : m_modelManager(manager)
{
  setId(Constants::CURRENT_DOCUMENT_FILTER_ID);
  setDisplayName(Constants::CURRENT_DOCUMENT_FILTER_DISPLAY_NAME);
  setDefaultShortcutString(".");
  setPriority(High);
  setDefaultIncludedByDefault(false);

  search.setSymbolsToSearchFor(SymbolSearcher::Declarations | SymbolSearcher::Enums | SymbolSearcher::Functions | SymbolSearcher::Classes);

  connect(manager, &CppModelManager::documentUpdated, this, &CppCurrentDocumentFilter::onDocumentUpdated);
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::currentEditorChanged, this, &CppCurrentDocumentFilter::onCurrentEditorChanged);
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::editorAboutToClose, this, &CppCurrentDocumentFilter::onEditorAboutToClose);
}

auto CppCurrentDocumentFilter::matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Orca::Plugin::Core::LocatorFilterEntry>
{
  QList<Orca::Plugin::Core::LocatorFilterEntry> goodEntries;
  QList<Orca::Plugin::Core::LocatorFilterEntry> betterEntries;

  const auto regexp = createRegExp(entry);
  if (!regexp.isValid())
    return goodEntries;

  const auto items = itemsOfCurrentDocument();
  for (const auto &info : items) {
    if (future.isCanceled())
      break;

    auto matchString = info->symbolName();
    if (info->type() == IndexItem::Declaration)
      matchString = info->representDeclaration();
    else if (info->type() == IndexItem::Function)
      matchString += info->symbolType();

    auto match = regexp.match(matchString);
    if (match.hasMatch()) {
      const auto betterMatch = match.capturedStart() == 0;
      auto id = QVariant::fromValue(info);
      auto name = matchString;
      auto extraInfo = info->symbolScope();
      if (info->type() == IndexItem::Function) {
        if (info->unqualifiedNameAndScope(matchString, &name, &extraInfo)) {
          name += info->symbolType();
          match = regexp.match(name);
        }
      }

      Orca::Plugin::Core::LocatorFilterEntry filterEntry(this, name, id, info->icon());
      filterEntry.extra_info = extraInfo;
      if (match.hasMatch()) {
        filterEntry.highlight_info = highlightInfo(match);
      } else {
        match = regexp.match(extraInfo);
        filterEntry.highlight_info = highlightInfo(match, Orca::Plugin::Core::LocatorFilterEntry::HighlightInfo::ExtraInfo);
      }

      if (betterMatch)
        betterEntries.append(filterEntry);
      else
        goodEntries.append(filterEntry);
    }
  }

  // entries are unsorted by design!

  betterEntries += goodEntries;
  return betterEntries;
}

auto CppCurrentDocumentFilter::accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void
{
  Q_UNUSED(newText)
  Q_UNUSED(selectionStart)
  Q_UNUSED(selectionLength)
  auto info = qvariant_cast<IndexItem::Ptr>(selection.internal_data);
  Orca::Plugin::Core::EditorManager::openEditorAt({Utils::FilePath::fromString(info->fileName()), info->line(), info->column()});
}

auto CppCurrentDocumentFilter::onDocumentUpdated(Document::Ptr doc) -> void
{
  QMutexLocker locker(&m_mutex);
  if (m_currentFileName == doc->fileName())
    m_itemsOfCurrentDoc.clear();
}

auto CppCurrentDocumentFilter::onCurrentEditorChanged(Orca::Plugin::Core::IEditor *currentEditor) -> void
{
  QMutexLocker locker(&m_mutex);
  if (currentEditor)
    m_currentFileName = currentEditor->document()->filePath().toString();
  else
    m_currentFileName.clear();
  m_itemsOfCurrentDoc.clear();
}

auto CppCurrentDocumentFilter::onEditorAboutToClose(Orca::Plugin::Core::IEditor *editorAboutToClose) -> void
{
  if (!editorAboutToClose)
    return;

  QMutexLocker locker(&m_mutex);
  if (m_currentFileName == editorAboutToClose->document()->filePath().toString()) {
    m_currentFileName.clear();
    m_itemsOfCurrentDoc.clear();
  }
}

auto CppCurrentDocumentFilter::itemsOfCurrentDocument() -> QList<IndexItem::Ptr>
{
  QMutexLocker locker(&m_mutex);

  if (m_currentFileName.isEmpty())
    return QList<IndexItem::Ptr>();

  if (m_itemsOfCurrentDoc.isEmpty()) {
    const Snapshot snapshot = m_modelManager->snapshot();
    if (const Document::Ptr thisDocument = snapshot.document(m_currentFileName)) {
      IndexItem::Ptr rootNode = search(thisDocument);
      rootNode->visitAllChildren([&](const IndexItem::Ptr &info) -> IndexItem::VisitorResult {
        m_itemsOfCurrentDoc.append(info);
        return IndexItem::Recurse;
      });
    }
  }

  return m_itemsOfCurrentDoc;
}

} // namespace CppEditor::Internal
