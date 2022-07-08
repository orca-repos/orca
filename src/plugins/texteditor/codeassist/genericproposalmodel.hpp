// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistproposalmodel.hpp"
#include "assistenums.hpp"

#include <texteditor/completionsettings.hpp>
#include <texteditor/texteditor_global.hpp>

#include <utils/fuzzymatcher.hpp>

#include <QHash>
#include <QList>

QT_FORWARD_DECLARE_CLASS(QIcon)

namespace TextEditor {

class AssistProposalItemInterface;

class TEXTEDITOR_EXPORT GenericProposalModel : public IAssistProposalModel {
public:
  GenericProposalModel();
  ~GenericProposalModel() override;

  auto reset() -> void override;
  auto size() const -> int override;
  auto text(int index) const -> QString override;

  virtual auto icon(int index) const -> QIcon;
  virtual auto detail(int index) const -> QString;
  virtual auto detailFormat(int index) const -> Qt::TextFormat;
  virtual auto persistentId(int index) const -> int;
  virtual auto containsDuplicates() const -> bool;
  virtual auto removeDuplicates() -> void;
  virtual auto filter(const QString &prefix) -> void;
  virtual auto isSortable(const QString &prefix) const -> bool;
  virtual auto sort(const QString &prefix) -> void;
  virtual auto supportsPrefixExpansion() const -> bool;
  virtual auto proposalPrefix() const -> QString;
  virtual auto keepPerfectMatch(AssistReason reason) const -> bool;
  virtual auto proposalItem(int index) const -> AssistProposalItemInterface*;
  virtual auto indexOf(const std::function<bool (AssistProposalItemInterface *)> &predicate) const -> int;

  auto loadContent(const QList<AssistProposalItemInterface*> &items) -> void;
  auto isPerfectMatch(const QString &prefix) const -> bool;
  auto hasItemsToPropose(const QString &prefix, AssistReason reason) const -> bool;
  auto isPrefiltered(const QString &prefix) const -> bool;
  auto setPrefilterPrefix(const QString &prefix) -> void;
  auto convertCaseSensitivity(CaseSensitivity textEditorCaseSensitivity) -> FuzzyMatcher::CaseSensitivity;

protected:
  QList<AssistProposalItemInterface*> m_currentItems;

private:
  QHash<QString, int> m_idByText;
  QList<AssistProposalItemInterface*> m_originalItems;
  QString m_prefilterPrefix;
  bool m_duplicatesRemoved = false;
};

using GenericProposalModelPtr = QSharedPointer<GenericProposalModel>;

} // TextEditor
