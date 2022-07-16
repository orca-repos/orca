// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistprocessor.hpp"
#include "assistproposalitem.hpp"
#include "ifunctionhintproposalmodel.hpp"
#include "completionassistprovider.hpp"

#include <texteditor/snippets/snippetassistcollector.hpp>
#include <texteditor/texteditorconstants.hpp>

namespace TextEditor {

class AssistInterface;

class TEXTEDITOR_EXPORT Keywords {
public:
  Keywords() = default;
  Keywords(const QStringList &variables, const QStringList &functions = QStringList(), const QMap<QString, QStringList> &functionArgs = QMap<QString, QStringList>());
  auto isVariable(const QString &word) const -> bool;
  auto isFunction(const QString &word) const -> bool;

  auto variables() const -> QStringList;
  auto functions() const -> QStringList;
  auto argsForFunction(const QString &function) const -> QStringList;

private:
  QStringList m_variables;
  QStringList m_functions;
  QMap<QString, QStringList> m_functionArgs;
};

class TEXTEDITOR_EXPORT KeywordsAssistProposalItem : public AssistProposalItem {
public:
  KeywordsAssistProposalItem(bool isFunction);

  auto prematurelyApplies(const QChar &c) const -> bool final;
  auto applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void final;

private:
  bool m_isFunction;
};

class TEXTEDITOR_EXPORT KeywordsFunctionHintModel final : public IFunctionHintProposalModel {
public:
  KeywordsFunctionHintModel(const QStringList &functionSymbols);
  ~KeywordsFunctionHintModel() final = default;

  auto reset() -> void final;
  auto size() const -> int final;
  auto text(int index) const -> QString final;
  auto activeArgument(const QString &prefix) const -> int final;

private:
  QStringList m_functionSymbols;
};

using DynamicCompletionFunction = std::function<void (const AssistInterface *, QList<AssistProposalItemInterface*> *, int &)>;

class TEXTEDITOR_EXPORT KeywordsCompletionAssistProvider : public CompletionAssistProvider {
public:
  KeywordsCompletionAssistProvider(const Keywords &keyWords = Keywords(), const QString &snippetGroup = QString(Constants::TEXT_SNIPPET_GROUP_ID));

  auto setDynamicCompletionFunction(const DynamicCompletionFunction &func) -> void;

  // IAssistProvider interface
  auto runType() const -> RunType override;
  auto createProcessor(const AssistInterface *) const -> IAssistProcessor* override;

private:
  Keywords m_keyWords;
  QString m_snippetGroup;
  DynamicCompletionFunction m_completionFunc;
};

class TEXTEDITOR_EXPORT KeywordsCompletionAssistProcessor : public IAssistProcessor {
public:
  KeywordsCompletionAssistProcessor(const Keywords &keywords);
  ~KeywordsCompletionAssistProcessor() override = default;

  auto perform(const AssistInterface *interface) -> IAssistProposal* override;
  auto setSnippetGroup(const QString &id) -> void;
  auto setDynamicCompletionFunction(DynamicCompletionFunction func) -> void;

protected:
  auto setKeywords(const Keywords &keywords) -> void;

private:
  auto isInComment(const AssistInterface *interface) const -> bool;
  auto generateProposalList(const QStringList &words, const QIcon &icon) -> QList<AssistProposalItemInterface*>;

  SnippetAssistCollector m_snippetCollector;
  const QIcon m_variableIcon;
  const QIcon m_functionIcon;
  Keywords m_keywords;
  DynamicCompletionFunction m_dynamicCompletionFunction;
};

TEXTEDITOR_EXPORT auto pathComplete(const AssistInterface *interface, QList<AssistProposalItemInterface*> *items, int &startPosition) -> void;

} // TextEditor
