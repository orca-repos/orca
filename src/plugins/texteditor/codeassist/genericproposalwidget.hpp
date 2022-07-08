// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposalwidget.hpp"

#include "genericproposalmodel.hpp"

#include <texteditor/texteditor_global.hpp>


namespace TextEditor {

class GenericProposalWidgetPrivate;

class TEXTEDITOR_EXPORT GenericProposalWidget : public IAssistProposalWidget {
  Q_OBJECT
  friend class GenericProposalWidgetPrivate;

public:
  GenericProposalWidget();
  ~GenericProposalWidget() override;

  auto setAssistant(CodeAssistant *assistant) -> void override;
  auto setReason(AssistReason reason) -> void override;
  auto setKind(AssistKind kind) -> void override;
  auto setUnderlyingWidget(const QWidget *underlyingWidget) -> void override;
  auto setModel(ProposalModelPtr model) -> void override;
  auto setDisplayRect(const QRect &rect) -> void override;
  auto setIsSynchronized(bool isSync) -> void override;
  auto supportsModelUpdate(const Utils::Id &proposalId) const -> bool override;
  auto updateModel(ProposalModelPtr model) -> void override;
  auto showProposal(const QString &prefix) -> void override;
  auto updateProposal(const QString &prefix) -> void override;
  auto closeProposal() -> void override;

private:
  auto updateAndCheck(const QString &prefix) -> bool;
  auto notifyActivation(int index) -> void;
  auto abort() -> void;
  auto updatePositionAndSize() -> void;
  auto turnOffAutoWidth() -> void;
  auto turnOnAutoWidth() -> void;

protected:
  auto eventFilter(QObject *o, QEvent *e) -> bool override;
  auto activateCurrentProposalItem() -> bool;
  auto model() -> GenericProposalModelPtr;

private:
  GenericProposalWidgetPrivate *d;
};

} // TextEditor
