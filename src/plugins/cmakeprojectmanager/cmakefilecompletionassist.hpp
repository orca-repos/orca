// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/codeassist/keywordscompletionassist.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeFileCompletionAssist : public TextEditor::KeywordsCompletionAssistProcessor {
public:
  CMakeFileCompletionAssist();

  // IAssistProcessor interface
  auto perform(const TextEditor::AssistInterface *interface) -> TextEditor::IAssistProposal* override;
};

class CMakeFileCompletionAssistProvider : public TextEditor::CompletionAssistProvider {
  Q_OBJECT

public:
  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;
};

} // Internal
} // CMakeProjectManager
