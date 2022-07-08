// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "assistproposaliteminterface.hpp"

#include <texteditor/texteditor_global.hpp>

#include <QIcon>
#include <QString>
#include <QVariant>

namespace TextEditor {

class TEXTEDITOR_EXPORT AssistProposalItem : public AssistProposalItemInterface {
public:
  ~AssistProposalItem() noexcept override = default;

  auto text() const -> QString override;
  auto implicitlyApplies() const -> bool override;
  auto prematurelyApplies(const QChar &c) const -> bool override;
  auto apply(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void override;
  auto setIcon(const QIcon &icon) -> void;
  auto icon() const -> QIcon final;
  auto setText(const QString &text) -> void;
  auto setDetail(const QString &detail) -> void;
  auto detail() const -> QString final;
  auto setData(const QVariant &var) -> void;
  auto data() const -> const QVariant&;
  auto isSnippet() const -> bool final;
  auto isValid() const -> bool final;
  auto hash() const -> quint64 override;

  virtual auto applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void;
  virtual auto applySnippet(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void;
  virtual auto applyQuickFix(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void;

private:
  QIcon m_icon;
  QString m_text;
  QString m_detail;
  QVariant m_data;
};

} // namespace TextEditor
