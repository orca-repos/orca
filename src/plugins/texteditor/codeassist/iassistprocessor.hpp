// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <functional>

namespace TextEditor {

class AssistInterface;
class IAssistProposal;

class TEXTEDITOR_EXPORT IAssistProcessor {
public:
  IAssistProcessor();
  virtual ~IAssistProcessor();

  virtual auto immediateProposal(const AssistInterface *) -> IAssistProposal* { return nullptr; }
  virtual auto perform(const AssistInterface *interface) -> IAssistProposal* = 0;
  auto setAsyncProposalAvailable(IAssistProposal *proposal) -> void;
  // Internal, used by CodeAssist
  using AsyncCompletionsAvailableHandler = std::function<void (IAssistProposal *proposal)>;
  auto setAsyncCompletionAvailableHandler(const AsyncCompletionsAvailableHandler &handler) -> void;
  virtual auto running() -> bool { return false; }
  virtual auto needsRestart() const -> bool { return false; }
  virtual auto cancel() -> void {}

private:
  AsyncCompletionsAvailableHandler m_asyncCompletionsAvailableHandler;
};

} // TextEditor
