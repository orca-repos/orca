// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppmodelmanagersupport.hpp"

#include <QScopedPointer>

namespace CppEditor::Internal {

class BuiltinModelManagerSupport : public ModelManagerSupport {
  Q_DISABLE_COPY(BuiltinModelManagerSupport)

public:
  BuiltinModelManagerSupport();
  ~BuiltinModelManagerSupport() override;

  auto completionAssistProvider() -> CppCompletionAssistProvider* final;
  auto functionHintAssistProvider() -> CppCompletionAssistProvider* override;
  auto createHoverHandler() -> TextEditor::BaseHoverHandler* final;
  auto createEditorDocumentProcessor(TextEditor::TextDocument *baseTextDocument) -> BaseEditorDocumentProcessor* final;
  auto followSymbolInterface() -> FollowSymbolInterface& final;
  auto refactoringEngineInterface() -> RefactoringEngineInterface& final;
  auto createOverviewModel() -> std::unique_ptr<AbstractOverviewModel> final;

private:
  QScopedPointer<CppCompletionAssistProvider> m_completionAssistProvider;
  QScopedPointer<FollowSymbolInterface> m_followSymbol;
  QScopedPointer<RefactoringEngineInterface> m_refactoringEngine;
};

class BuiltinModelManagerSupportProvider : public ModelManagerSupportProvider {
public:
  auto id() const -> QString override;
  auto displayName() const -> QString override;
  auto createModelManagerSupport() -> ModelManagerSupport::Ptr override;
};

} // namespace CppEditor::Internal
