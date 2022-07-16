// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/snippets/snippetassistcollector.hpp>

#include <functional>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace CPlusPlus {
struct LanguageFeatures;
}

namespace CppEditor {

class CPPEDITOR_EXPORT CppCompletionAssistProcessor : public TextEditor::IAssistProcessor {
public:
  explicit CppCompletionAssistProcessor(int snippetItemOrder = 0);

  static auto preprocessorCompletions() -> const QStringList;

protected:
  auto addSnippets() -> void;

  using DotAtIncludeCompletionHandler = std::function<void(int &startPosition, unsigned *kind)>;
  static auto startOfOperator(QTextDocument *textDocument, int positionInDocument, unsigned *kind, int &start, const CPlusPlus::LanguageFeatures &languageFeatures, bool adjustForQt5SignalSlotCompletion = false, DotAtIncludeCompletionHandler dotAtIncludeCompletionHandler = DotAtIncludeCompletionHandler()) -> void;

  int m_positionForProposal = -1;
  QList<TextEditor::AssistProposalItemInterface*> m_completions;
  TextEditor::IAssistProposal *m_hintProposal = nullptr;

private:
  TextEditor::SnippetAssistCollector m_snippetCollector;
};

} // namespace CppEditor
