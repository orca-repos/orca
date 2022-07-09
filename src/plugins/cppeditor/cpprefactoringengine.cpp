// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpprefactoringengine.hpp"

#include "cppcanonicalsymbol.hpp"
#include "cppeditorwidget.hpp"
#include "cppmodelmanager.hpp"
#include "cppsemanticinfo.hpp"
#include "cpptoolsreuse.hpp"
#include "cppfollowsymbolundercursor.hpp"

#include <clangsupport/sourcelocationscontainer.h>
#include <texteditor/texteditor.hpp>

#include <utils/qtcassert.hpp>

namespace CppEditor::Internal {

auto CppRefactoringEngine::startLocalRenaming(const CursorInEditor &data, const ProjectPart *, RenameCallback &&renameSymbolsCallback) -> void
{
  auto editorWidget = data.editorWidget();
  QTC_ASSERT(editorWidget, renameSymbolsCallback(QString(), ClangBackEnd::SourceLocationsContainer(), 0); return;);
  editorWidget->updateSemanticInfo();
  // Call empty callback
  renameSymbolsCallback(QString(), ClangBackEnd::SourceLocationsContainer(), data.cursor().document()->revision());
}

auto CppRefactoringEngine::globalRename(const CursorInEditor &data, UsagesCallback &&, const QString &replacement) -> void
{
  auto modelManager = CppModelManager::instance();
  if (!modelManager)
    return;

  auto editorWidget = data.editorWidget();
  QTC_ASSERT(editorWidget, return;);

  auto info = editorWidget->semanticInfo();
  info.snapshot = modelManager->snapshot();
  info.snapshot.insert(info.doc);
  const auto &cursor = data.cursor();
  if (const CPlusPlus::Macro *macro = findCanonicalMacro(cursor, info.doc)) {
    modelManager->renameMacroUsages(*macro, replacement);
  } else {
    Internal::CanonicalSymbol cs(info.doc, info.snapshot);
    CPlusPlus::Symbol *canonicalSymbol = cs(cursor);
    if (canonicalSymbol)
      modelManager->renameUsages(canonicalSymbol, cs.context(), replacement);
  }
}

auto CppRefactoringEngine::findUsages(const CursorInEditor &data, UsagesCallback &&) const -> void
{
  auto modelManager = CppModelManager::instance();
  if (!modelManager)
    return;

  auto editorWidget = data.editorWidget();
  QTC_ASSERT(editorWidget, return;);

  auto info = editorWidget->semanticInfo();
  info.snapshot = modelManager->snapshot();
  info.snapshot.insert(info.doc);
  const auto &cursor = data.cursor();
  if (const CPlusPlus::Macro *macro = findCanonicalMacro(cursor, info.doc)) {
    modelManager->findMacroUsages(*macro);
  } else {
    Internal::CanonicalSymbol cs(info.doc, info.snapshot);
    CPlusPlus::Symbol *canonicalSymbol = cs(cursor);
    if (canonicalSymbol)
      modelManager->findUsages(canonicalSymbol, cs.context());
  }
}

auto CppRefactoringEngine::globalFollowSymbol(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) const -> void
{
  FollowSymbolUnderCursor followSymbol;
  return followSymbol.findLink(data, std::move(processLinkCallback), true, snapshot, documentFromSemanticInfo, symbolFinder, inNextSplit);
}

} // namespace CppEditor::Internal
