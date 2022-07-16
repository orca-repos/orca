// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposalwidget.hpp"

namespace TextEditor {

struct FunctionHintProposalWidgetPrivate;

class TEXTEDITOR_EXPORT FunctionHintProposalWidget : public IAssistProposalWidget {
  Q_OBJECT

public:
  FunctionHintProposalWidget();
  ~FunctionHintProposalWidget() override;

  auto setAssistant(CodeAssistant *assistant) -> void override;
  auto setReason(AssistReason reason) -> void override;
  auto setKind(AssistKind kind) -> void override;
  auto setUnderlyingWidget(const QWidget *underlyingWidget) -> void override;
  auto setModel(ProposalModelPtr model) -> void override;
  auto setDisplayRect(const QRect &rect) -> void override;
  auto setIsSynchronized(bool isSync) -> void override;
  auto showProposal(const QString &prefix) -> void override;
  auto updateProposal(const QString &prefix) -> void override;
  auto closeProposal() -> void override;
  auto proposalIsVisible() const -> bool override;

protected:
  auto eventFilter(QObject *o, QEvent *e) -> bool override;

private:
  auto nextPage() -> void;
  auto previousPage() -> void;
  auto updateAndCheck(const QString &prefix) -> bool;
  auto updateContent() -> void;
  auto updatePosition() -> void;
  auto abort() -> void;
  auto loadSelectedHint() const -> int;
  auto storeSelectedHint() -> void;

  FunctionHintProposalWidgetPrivate *d;
};

} // TextEditor
