// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "builtineditordocumentparser.hpp"
#include "cppcompletionassistprocessor.hpp"
#include "cppcompletionassistprovider.hpp"
#include "cppmodelmanager.hpp"
#include "cppworkingcopy.hpp"

#include <cplusplus/Icons.h>
#include <cplusplus/Symbol.h>
#include <cplusplus/TypeOfExpression.h>

#include <texteditor/texteditor.hpp>
#include <texteditor/codeassist/genericproposalmodel.hpp>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/snippets/snippetassistcollector.hpp>


#include <QStringList>
#include <QVariant>

namespace CPlusPlus {
class LookupItem;
class ClassOrNamespace;
class Function;
class LookupContext;
} // namespace CPlusPlus

namespace CppEditor::Internal {

class CppCompletionAssistInterface;

class CppAssistProposalModel : public TextEditor::GenericProposalModel {
public:
  CppAssistProposalModel() : TextEditor::GenericProposalModel(), m_typeOfExpression(new CPlusPlus::TypeOfExpression)
  {
    m_typeOfExpression->setExpandTemplates(true);
  }

  auto isSortable(const QString &prefix) const -> bool override;
  auto proposalItem(int index) const -> TextEditor::AssistProposalItemInterface* override;

  unsigned m_completionOperator = CPlusPlus::T_EOF_SYMBOL;
  bool m_replaceDotForArrow = false;
  QSharedPointer<CPlusPlus::TypeOfExpression> m_typeOfExpression;
};

using CppAssistProposalModelPtr = QSharedPointer<CppAssistProposalModel>;

class InternalCompletionAssistProvider : public CppCompletionAssistProvider {
  Q_OBJECT public:
  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;

  auto createAssistInterface(const Utils::FilePath &filePath, const TextEditor::TextEditorWidget *textEditorWidget, const CPlusPlus::LanguageFeatures &languageFeatures, int position, TextEditor::AssistReason reason) const -> TextEditor::AssistInterface* override;
};

class InternalCppCompletionAssistProcessor : public CppCompletionAssistProcessor {
public:
  InternalCppCompletionAssistProcessor();
  ~InternalCppCompletionAssistProcessor() override;

  auto perform(const TextEditor::AssistInterface *interface) -> TextEditor::IAssistProposal* override;

private:
  auto createContentProposal() -> TextEditor::IAssistProposal*;
  auto createHintProposal(QList<CPlusPlus::Function*> symbols) const -> TextEditor::IAssistProposal*;
  auto accepts() const -> bool;
  auto startOfOperator(int positionInDocument, unsigned *kind, bool wantFunctionCall) const -> int;
  auto findStartOfName(int pos = -1) const -> int;
  auto startCompletionHelper() -> int;
  auto tryObjCCompletion() -> bool;
  auto objcKeywordsWanted() const -> bool;
  auto startCompletionInternal(const QString &fileName, int line, int positionInBlock, const QString &expression, int endOfExpression) -> int;
  auto completeObjCMsgSend(CPlusPlus::ClassOrNamespace *binding, bool staticClassAccess) -> void;
  auto completeInclude(const QTextCursor &cursor) -> bool;
  auto completeInclude(const QString &realPath, const QStringList &suffixes) -> void;
  auto completePreprocessor() -> void;
  auto completeConstructorOrFunction(const QList<CPlusPlus::LookupItem> &results, int endOfExpression, bool toolTipOnly) -> bool;
  auto completeMember(const QList<CPlusPlus::LookupItem> &results) -> bool;
  auto completeScope(const QList<CPlusPlus::LookupItem> &results) -> bool;
  auto completeNamespace(CPlusPlus::ClassOrNamespace *binding) -> void;
  auto completeClass(CPlusPlus::ClassOrNamespace *b, bool staticLookup = true) -> void;
  auto addClassMembersToCompletion(CPlusPlus::Scope *scope, bool staticLookup) -> void;

  enum CompleteQtMethodMode {
    CompleteQt4Signals,
    CompleteQt4Slots,
    CompleteQt5Signals,
    CompleteQt5Slots,
  };

  auto completeQtMethod(const QList<CPlusPlus::LookupItem> &results, CompleteQtMethodMode type) -> bool;
  auto completeQtMethodClassName(const QList<CPlusPlus::LookupItem> &results, CPlusPlus::Scope *cursorScope) -> bool;
  auto globalCompletion(CPlusPlus::Scope *scope) -> bool;
  auto addKeywordCompletionItem(const QString &text) -> void;
  auto addCompletionItem(const QString &text, const QIcon &icon = QIcon(), int order = 0, const QVariant &data = QVariant()) -> void;
  auto addCompletionItem(CPlusPlus::Symbol *symbol, int order = 0) -> void;
  auto addKeywords() -> void;
  auto addMacros(const QString &fileName, const CPlusPlus::Snapshot &snapshot) -> void;
  auto addMacros_helper(const CPlusPlus::Snapshot &snapshot, const QString &fileName, QSet<QString> *processed, QSet<QString> *definedMacros) -> void;

  enum {
    CompleteQt5SignalOrSlotClassNameTrigger = CPlusPlus::T_LAST_TOKEN + 1,
    CompleteQt5SignalTrigger,
    CompleteQt5SlotTrigger
  };

  QScopedPointer<const CppCompletionAssistInterface> m_interface;
  CppAssistProposalModelPtr m_model;
};

class CppCompletionAssistInterface : public TextEditor::AssistInterface {
public:
  CppCompletionAssistInterface(const Utils::FilePath &filePath, const TextEditor::TextEditorWidget *textEditorWidget, BuiltinEditorDocumentParser::Ptr parser, const CPlusPlus::LanguageFeatures &languageFeatures, int position, TextEditor::AssistReason reason, const WorkingCopy &workingCopy) : TextEditor::AssistInterface(textEditorWidget->document(), position, filePath, reason), m_parser(parser), m_gotCppSpecifics(false), m_workingCopy(workingCopy), m_languageFeatures(languageFeatures) {}
  CppCompletionAssistInterface(const Utils::FilePath &filePath, QTextDocument *textDocument, int position, TextEditor::AssistReason reason, const CPlusPlus::Snapshot &snapshot, const ProjectExplorer::HeaderPaths &headerPaths, const CPlusPlus::LanguageFeatures &features) : TextEditor::AssistInterface(textDocument, position, filePath, reason), m_gotCppSpecifics(true), m_snapshot(snapshot), m_headerPaths(headerPaths), m_languageFeatures(features) {}

  auto snapshot() const -> const CPlusPlus::Snapshot&
  {
    getCppSpecifics();
    return m_snapshot;
  }

  auto headerPaths() const -> const ProjectExplorer::HeaderPaths&
  {
    getCppSpecifics();
    return m_headerPaths;
  }

  auto languageFeatures() const -> CPlusPlus::LanguageFeatures
  {
    getCppSpecifics();
    return m_languageFeatures;
  }

private:
  auto getCppSpecifics() const -> void;

  BuiltinEditorDocumentParser::Ptr m_parser;
  mutable bool m_gotCppSpecifics;
  WorkingCopy m_workingCopy;
  mutable CPlusPlus::Snapshot m_snapshot;
  mutable ProjectExplorer::HeaderPaths m_headerPaths;
  mutable CPlusPlus::LanguageFeatures m_languageFeatures;
};

} // CppEditor::Internal

Q_DECLARE_METATYPE(CPlusPlus::Symbol *)
