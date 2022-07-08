// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposal.hpp"
#include "genericproposalmodel.hpp"

#include <texteditor/quickfix.hpp>

namespace TextEditor {

class AssistProposalItemInterface;

class TEXTEDITOR_EXPORT GenericProposal : public IAssistProposal {
public:
  GenericProposal(int cursorPos, GenericProposalModelPtr model);
  GenericProposal(int cursorPos, const QList<AssistProposalItemInterface*> &items);
  ~GenericProposal() override;

  static auto createProposal(const AssistInterface *interface, const QuickFixOperations &quickFixes) -> GenericProposal*;
  auto hasItemsToPropose(const QString &prefix, AssistReason reason) const -> bool override;
  auto model() const -> ProposalModelPtr override;
  auto createWidget() const -> IAssistProposalWidget* override;

protected:
  auto moveBasePosition(int length) -> void;

private:
  GenericProposalModelPtr m_model;
};

} // TextEditor
