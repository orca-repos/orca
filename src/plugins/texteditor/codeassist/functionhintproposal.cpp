// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "functionhintproposal.hpp"
#include "ifunctionhintproposalmodel.hpp"
#include "functionhintproposalwidget.hpp"

static const char functionHintId[] = "TextEditor.FunctionHintId";

using namespace TextEditor;

FunctionHintProposal::FunctionHintProposal(int cursorPos, FunctionHintProposalModelPtr model) : IAssistProposal(functionHintId, cursorPos), m_model(model)
{
  setFragile(true);
}

FunctionHintProposal::~FunctionHintProposal() = default;

auto FunctionHintProposal::model() const -> ProposalModelPtr
{
  return m_model;
}

auto FunctionHintProposal::createWidget() const -> IAssistProposalWidget*
{
  return new FunctionHintProposalWidget;
}
