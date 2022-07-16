// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppsemanticinfo.hpp"

#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/iassistprovider.hpp>

#include <cplusplus/LookupContext.h>

namespace CppEditor {

class CppEditorWidget;
class CppRefactoringFile;
using CppRefactoringFilePtr = QSharedPointer<CppRefactoringFile>;

namespace Internal {

class CppQuickFixInterface : public TextEditor::AssistInterface {
public:
  CppQuickFixInterface(CppEditorWidget *editor, TextEditor::AssistReason reason);

  auto path() const -> const QList<CPlusPlus::AST*>&;
  auto snapshot() const -> CPlusPlus::Snapshot;
  auto semanticInfo() const -> SemanticInfo;
  auto context() const -> const CPlusPlus::LookupContext&;
  auto editor() const -> CppEditorWidget*;
  auto currentFile() const -> CppRefactoringFilePtr;
  auto isCursorOn(unsigned tokenIndex) const -> bool;
  auto isCursorOn(const CPlusPlus::AST *ast) const -> bool;

private:
  CppEditorWidget *m_editor;
  SemanticInfo m_semanticInfo;
  CPlusPlus::Snapshot m_snapshot;
  CppRefactoringFilePtr m_currentFile;
  CPlusPlus::LookupContext m_context;
  QList<CPlusPlus::AST*> m_path;
};

class CppQuickFixAssistProvider : public TextEditor::IAssistProvider {
public:
  CppQuickFixAssistProvider() = default;
  auto runType() const -> IAssistProvider::RunType override;
  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;
};

} // Internal
} // CppEditor
