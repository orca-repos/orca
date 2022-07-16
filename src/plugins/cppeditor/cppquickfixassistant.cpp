// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppquickfixassistant.hpp"

#include "cppeditorconstants.hpp"
#include "cppeditorwidget.hpp"
#include "cppmodelmanager.hpp"
#include "cppquickfixes.hpp"
#include "cpprefactoringchanges.hpp"

#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/textdocument.hpp>

#include <cplusplus/ASTPath.h>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace CPlusPlus;
using namespace TextEditor;

namespace CppEditor {
namespace Internal {

// -------------------------
// CppQuickFixAssistProcessor
// -------------------------
class CppQuickFixAssistProcessor : public IAssistProcessor {
  auto perform(const AssistInterface *interface) -> IAssistProposal* override
  {
    QSharedPointer<const AssistInterface> assistInterface(interface);
    auto cppInterface = assistInterface.staticCast<const CppQuickFixInterface>();

    QuickFixOperations quickFixes;
    for (auto factory : CppQuickFixFactory::cppQuickFixFactories())
      factory->match(*cppInterface, quickFixes);

    return GenericProposal::createProposal(interface, quickFixes);
  }
};

// -------------------------
// CppQuickFixAssistProvider
// -------------------------
auto CppQuickFixAssistProvider::runType() const -> IAssistProvider::RunType
{
  return Synchronous;
}

auto CppQuickFixAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new CppQuickFixAssistProcessor;
}

// --------------------------
// CppQuickFixAssistInterface
// --------------------------
CppQuickFixInterface::CppQuickFixInterface(CppEditorWidget *editor, AssistReason reason) : AssistInterface(editor->document(), editor->position(), editor->textDocument()->filePath(), reason), m_editor(editor), m_semanticInfo(editor->semanticInfo()), m_snapshot(CppModelManager::instance()->snapshot()), m_currentFile(CppRefactoringChanges::file(editor, m_semanticInfo.doc)), m_context(m_semanticInfo.doc, m_snapshot)
{
  QTC_CHECK(m_semanticInfo.doc);
  QTC_CHECK(m_semanticInfo.doc->translationUnit());
  QTC_CHECK(m_semanticInfo.doc->translationUnit()->ast());
  ASTPath astPath(m_semanticInfo.doc);
  m_path = astPath(editor->textCursor());
}

auto CppQuickFixInterface::path() const -> const QList<AST*>&
{
  return m_path;
}

auto CppQuickFixInterface::snapshot() const -> Snapshot
{
  return m_snapshot;
}

auto CppQuickFixInterface::semanticInfo() const -> SemanticInfo
{
  return m_semanticInfo;
}

auto CppQuickFixInterface::context() const -> const LookupContext&
{
  return m_context;
}

auto CppQuickFixInterface::editor() const -> CppEditorWidget*
{
  return m_editor;
}

auto CppQuickFixInterface::currentFile() const -> CppRefactoringFilePtr
{
  return m_currentFile;
}

auto CppQuickFixInterface::isCursorOn(unsigned tokenIndex) const -> bool
{
  return currentFile()->isCursorOn(tokenIndex);
}

auto CppQuickFixInterface::isCursorOn(const AST *ast) const -> bool
{
  return currentFile()->isCursorOn(ast);
}

} // namespace Internal
} // namespace CppEditor
