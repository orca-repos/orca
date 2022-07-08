// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposalwidget.hpp"

#include <QThread>

namespace TextEditor {

class IAssistProcessor;
class IAssistProposal;
class AssistInterface;

namespace Internal {

class ProcessorRunner : public QThread {
  Q_OBJECT

public:
  ProcessorRunner();
  ~ProcessorRunner() override;

  auto setProcessor(IAssistProcessor *processor) -> void; // Takes ownership of the processor.
  auto setAssistInterface(AssistInterface *interface) -> void;
  auto setDiscardProposal(bool discard) -> void;
  auto run() -> void override;
  auto proposal() const -> IAssistProposal*;

private:
  IAssistProcessor *m_processor = nullptr;
  AssistInterface *m_interface = nullptr;
  bool m_discardProposal = false;
  IAssistProposal *m_proposal = nullptr;
  AssistReason m_reason = IdleEditor;
};

} // Internal
} // TextEditor
