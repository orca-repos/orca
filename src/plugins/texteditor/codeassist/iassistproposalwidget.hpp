// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "assistenums.hpp"
#include "iassistproposalmodel.hpp"

#include <texteditor/texteditor_global.hpp>

#include <QFrame>

namespace Utils { class Id; }

namespace TextEditor {

class CodeAssistant;
class AssistProposalItemInterface;

class TEXTEDITOR_EXPORT IAssistProposalWidget : public QFrame {
  Q_OBJECT

public:
  IAssistProposalWidget();
  ~IAssistProposalWidget() override;

  virtual auto setAssistant(CodeAssistant *assistant) -> void = 0;
  virtual auto setReason(AssistReason reason) -> void = 0;
  virtual auto setKind(AssistKind kind) -> void = 0;
  virtual auto setUnderlyingWidget(const QWidget *underlyingWidget) -> void = 0;
  virtual auto setModel(ProposalModelPtr model) -> void = 0;
  virtual auto setDisplayRect(const QRect &rect) -> void = 0;
  virtual auto setIsSynchronized(bool isSync) -> void = 0;
  virtual auto showProposal(const QString &prefix) -> void = 0;
  virtual auto updateProposal(const QString &prefix) -> void = 0;
  virtual auto closeProposal() -> void = 0;
  virtual auto proposalIsVisible() const -> bool { return isVisible(); }
  virtual auto supportsModelUpdate(const Utils::Id &/*proposalId*/) const -> bool { return false; }
  virtual auto updateModel(ProposalModelPtr) -> void {}

  auto basePosition() const -> int;
  auto setBasePosition(int basePosition) -> void;

signals:
  auto prefixExpanded(const QString &newPrefix) -> void;
  auto proposalItemActivated(AssistProposalItemInterface *proposalItem) -> void;
  auto explicitlyAborted() -> void;

protected:
  int m_basePosition = -1;
};

} // TextEditor
