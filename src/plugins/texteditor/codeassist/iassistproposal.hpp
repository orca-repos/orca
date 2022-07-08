// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "assistenums.hpp"
#include "iassistproposalmodel.hpp"

#include <texteditor/texteditor_global.hpp>

#include <utils/id.hpp>

namespace TextEditor {

class IAssistProposalWidget;
class TextEditorWidget;

class TEXTEDITOR_EXPORT IAssistProposal {
public:
  IAssistProposal(Utils::Id id, int basePosition);
  virtual ~IAssistProposal();

  auto basePosition() const -> int;
  auto isFragile() const -> bool;
  auto supportsPrefix() const -> bool;

  virtual auto hasItemsToPropose(const QString &, AssistReason) const -> bool { return true; }
  virtual auto isCorrective(TextEditorWidget *editorWidget) const -> bool;
  virtual auto makeCorrection(TextEditorWidget *editorWidget) -> void;
  virtual auto model() const -> ProposalModelPtr = 0;
  virtual auto createWidget() const -> IAssistProposalWidget* = 0;

  auto setFragile(bool fragile) -> void;
  auto setSupportsPrefix(bool supportsPrefix) -> void;
  auto id() const -> Utils::Id { return m_id; }

protected:
  Utils::Id m_id;
  int m_basePosition;
  bool m_isFragile = false;
  bool m_supportsPrefix = true;
};

} // TextEditor
