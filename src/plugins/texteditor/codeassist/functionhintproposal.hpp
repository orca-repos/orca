// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposal.hpp"
#include "ifunctionhintproposalmodel.hpp"

namespace TextEditor {

class TEXTEDITOR_EXPORT FunctionHintProposal : public IAssistProposal {
public:
  FunctionHintProposal(int cursorPos, FunctionHintProposalModelPtr model);
  ~FunctionHintProposal() override;

  auto model() const -> ProposalModelPtr override;
  auto createWidget() const -> IAssistProposalWidget* override;

private:
  FunctionHintProposalModelPtr m_model;
};

} // TextEditor
