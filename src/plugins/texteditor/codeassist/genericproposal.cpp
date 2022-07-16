// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "assistinterface.hpp"
#include "assistproposalitem.hpp"
#include "genericproposal.hpp"
#include "genericproposalmodel.hpp"
#include "genericproposalwidget.hpp"

#include <texteditor/texteditorconstants.hpp>

namespace TextEditor {

GenericProposal::GenericProposal(int cursorPos, GenericProposalModelPtr model) : IAssistProposal(Constants::GENERIC_PROPOSAL_ID, cursorPos), m_model(model) {}

GenericProposal::GenericProposal(int cursorPos, const QList<AssistProposalItemInterface*> &items) : IAssistProposal(Constants::GENERIC_PROPOSAL_ID, cursorPos), m_model(new GenericProposalModel)
{
  m_model->loadContent(items);
}

GenericProposal::~GenericProposal() = default;

auto GenericProposal::createProposal(const AssistInterface *interface, const QuickFixOperations &quickFixes) -> GenericProposal*
{
  if (quickFixes.isEmpty())
    return nullptr;

  QList<AssistProposalItemInterface*> items;
  foreach(const QuickFixOperation::Ptr &op, quickFixes) {
    QVariant v;
    v.setValue(op);
    const auto item = new AssistProposalItem;
    item->setText(op->description());
    item->setData(v);
    item->setOrder(op->priority());
    items.append(item);
  }

  return new GenericProposal(interface->position(), items);
}

auto GenericProposal::hasItemsToPropose(const QString &prefix, AssistReason reason) const -> bool
{
  if (!prefix.isEmpty()) {
    if (m_model->containsDuplicates())
      m_model->removeDuplicates();
    m_model->filter(prefix);
    m_model->setPrefilterPrefix(prefix);
  }

  return m_model->hasItemsToPropose(prefix, reason);
}

auto GenericProposal::model() const -> ProposalModelPtr
{
  return m_model;
}

auto GenericProposal::createWidget() const -> IAssistProposalWidget*
{
  return new GenericProposalWidget;
}

auto GenericProposal::moveBasePosition(int length) -> void
{
  m_basePosition += length;
}

} // namespace TextEditor
