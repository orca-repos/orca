// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "textdocumentmanipulatorinterface.hpp"

#include <utils/declarationmacros.hpp>

#include <QString>

QT_BEGIN_NAMESPACE
class QIcon;
class QString;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT AssistProposalItemInterface {
public:
  // We compare proposals by enum values, be careful changing their values
  enum class ProposalMatch {
    Full = 0,
    Exact = 1,
    Prefix = 2,
    Infix = 3,
    None = 4
  };

  AssistProposalItemInterface() = default;
  virtual ~AssistProposalItemInterface() noexcept = default;

  UTILS_DELETE_MOVE_AND_COPY(AssistProposalItemInterface)

  virtual auto text() const -> QString = 0;
  virtual auto filterText() const -> QString { return text(); }
  virtual auto implicitlyApplies() const -> bool = 0;
  virtual auto prematurelyApplies(const QChar &typedCharacter) const -> bool = 0;
  virtual auto apply(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void = 0;
  virtual auto icon() const -> QIcon = 0;
  virtual auto detail() const -> QString = 0;
  virtual auto isKeyword() const -> bool { return false; }
  virtual auto detailFormat() const -> Qt::TextFormat { return Qt::AutoText; }
  virtual auto isSnippet() const -> bool = 0;
  virtual auto isValid() const -> bool = 0;
  virtual auto hash() const -> quint64 = 0; // it is only for removing duplicates
  virtual auto requiresFixIts() const -> bool { return false; }

  auto order() const -> int { return m_order; }
  auto setOrder(int order) -> void { m_order = order; }
  auto proposalMatch() -> ProposalMatch { return m_proposalMatch; }
  auto setProposalMatch(ProposalMatch match) -> void { m_proposalMatch = match; }

private:
  int m_order = 0;
  ProposalMatch m_proposalMatch = ProposalMatch::None;
};

} // namespace TextEditor
