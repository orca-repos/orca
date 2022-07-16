// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QSharedPointer>
#include <QString>

#include <memory>

namespace TextEditor {
class TextDocument;
class BaseHoverHandler;
} // namespace TextEditor

namespace CppEditor {

class AbstractOverviewModel;
class BaseEditorDocumentProcessor;
class CppCompletionAssistProvider;
class FollowSymbolInterface;
class RefactoringEngineInterface;

class CPPEDITOR_EXPORT ModelManagerSupport {
public:
  using Ptr = QSharedPointer<ModelManagerSupport>;
  
  virtual ~ModelManagerSupport() = 0;

  virtual auto completionAssistProvider() -> CppCompletionAssistProvider* = 0;
  virtual auto functionHintAssistProvider() -> CppCompletionAssistProvider* = 0;
  virtual auto createHoverHandler() -> TextEditor::BaseHoverHandler* = 0;
  virtual auto createEditorDocumentProcessor(TextEditor::TextDocument *baseTextDocument) -> BaseEditorDocumentProcessor* = 0;
  virtual auto followSymbolInterface() -> FollowSymbolInterface& = 0;
  virtual auto refactoringEngineInterface() -> RefactoringEngineInterface& = 0;
  virtual auto createOverviewModel() -> std::unique_ptr<AbstractOverviewModel> = 0;
  virtual auto supportsOutline(const TextEditor::TextDocument *) const -> bool { return true; }
  virtual auto supportsLocalUses(const TextEditor::TextDocument *) const -> bool { return true; }
};

class CPPEDITOR_EXPORT ModelManagerSupportProvider {
public:
  virtual ~ModelManagerSupportProvider() = default;

  virtual auto id() const -> QString = 0;
  virtual auto displayName() const -> QString = 0;
  virtual auto createModelManagerSupport() -> ModelManagerSupport::Ptr = 0;
};

} // CppEditor namespace
