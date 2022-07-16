// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppbuiltinmodelmanagersupport.hpp"

#include "builtineditordocumentprocessor.hpp"
#include "cppcompletionassist.hpp"
#include "cppelementevaluator.hpp"
#include "cppfollowsymbolundercursor.hpp"
#include "cppoverviewmodel.hpp"
#include "cpprefactoringengine.hpp"
#include "cpptoolsreuse.hpp"

#include <app/app_version.hpp>
#include <texteditor/basehoverhandler.hpp>
#include <utils/executeondestruction.hpp>

#include <QCoreApplication>

using namespace Orca::Plugin::Core;
using namespace TextEditor;

namespace CppEditor::Internal {
namespace {

class CppHoverHandler : public TextEditor::BaseHoverHandler {
  auto identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void override
  {
    Utils::ExecuteOnDestruction reportPriority([this, report]() { report(priority()); });

    QTextCursor tc(editorWidget->document());
    tc.setPosition(pos);

    CppElementEvaluator evaluator(editorWidget);
    evaluator.setTextCursor(tc);
    evaluator.execute();
    QString tip;
    if (evaluator.hasDiagnosis()) {
      tip += evaluator.diagnosis();
      setPriority(Priority_Diagnostic);
    }
    const auto fallback = identifierWordsUnderCursor(tc);
    if (evaluator.identifiedCppElement()) {
      const auto &cppElement = evaluator.cppElement();
      const auto candidates = cppElement->helpIdCandidates;
      const HelpItem helpItem(candidates + fallback, cppElement->helpMark, cppElement->helpCategory);
      setLastHelpItemIdentified(helpItem);
      if (!helpItem.isValid())
        tip += cppElement->tooltip;
    } else {
      setLastHelpItemIdentified({fallback, {}, HelpItem::Unknown});
    }
    setToolTip(tip);
  }
};
} // anonymous namespace

auto BuiltinModelManagerSupportProvider::id() const -> QString
{
  return QLatin1String("CppEditor.BuiltinCodeModel");
}

auto BuiltinModelManagerSupportProvider::displayName() const -> QString
{
  return QCoreApplication::translate("ModelManagerSupportInternal::displayName", "%1 Built-in").arg(Orca::Plugin::Core::IDE_DISPLAY_NAME);
}

auto BuiltinModelManagerSupportProvider::createModelManagerSupport() -> ModelManagerSupport::Ptr
{
  return ModelManagerSupport::Ptr(new BuiltinModelManagerSupport);
}

BuiltinModelManagerSupport::BuiltinModelManagerSupport() : m_completionAssistProvider(new InternalCompletionAssistProvider), m_followSymbol(new FollowSymbolUnderCursor), m_refactoringEngine(new CppRefactoringEngine) {}

BuiltinModelManagerSupport::~BuiltinModelManagerSupport() = default;

auto BuiltinModelManagerSupport::createEditorDocumentProcessor(TextEditor::TextDocument *baseTextDocument) -> BaseEditorDocumentProcessor*
{
  return new BuiltinEditorDocumentProcessor(baseTextDocument);
}

auto BuiltinModelManagerSupport::completionAssistProvider() -> CppCompletionAssistProvider*
{
  return m_completionAssistProvider.data();
}

auto BuiltinModelManagerSupport::functionHintAssistProvider() -> CppCompletionAssistProvider*
{
  return nullptr;
}

auto BuiltinModelManagerSupport::createHoverHandler() -> TextEditor::BaseHoverHandler*
{
  return new CppHoverHandler;
}

auto BuiltinModelManagerSupport::followSymbolInterface() -> FollowSymbolInterface&
{
  return *m_followSymbol;
}

auto BuiltinModelManagerSupport::refactoringEngineInterface() -> RefactoringEngineInterface&
{
  return *m_refactoringEngine;
}

auto BuiltinModelManagerSupport::createOverviewModel() -> std::unique_ptr<AbstractOverviewModel>
{
  return std::make_unique<OverviewModel>();
}

} // namespace CppEditor::Internal
