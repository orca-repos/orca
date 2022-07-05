// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ifindsupport.h"

#include <QPointer>

namespace Core {
namespace Internal {

class CurrentDocumentFind final : public QObject {
  Q_OBJECT

public:
  CurrentDocumentFind();

  auto resetIncrementalSearch() const -> void;
  auto clearHighlights() const -> void;
  auto supportsReplace() const -> bool;
  auto supportsSelectAll() const -> bool;
  auto supportedFindFlags() const -> FindFlags;
  auto currentFindString() const -> QString;
  auto completedFindString() const -> QString;
  auto isEnabled() const -> bool;
  auto candidate() const -> IFindSupport*;
  auto highlightAll(const QString &txt, FindFlags find_flags) const -> void;
  auto findIncremental(const QString &txt, FindFlags find_flags) const -> IFindSupport::Result;
  auto findStep(const QString &txt, FindFlags find_flags) const -> IFindSupport::Result;
  auto selectAll(const QString &txt, FindFlags find_flags) const -> void;
  auto replace(const QString &before, const QString &after, FindFlags find_flags) const -> void;
  auto replaceStep(const QString &before, const QString &after, FindFlags find_flags) const -> bool;
  auto replaceAll(const QString &before, const QString &after, FindFlags find_flags) const -> int;
  auto defineFindScope() const -> void;
  auto clearFindScope() const -> void;
  auto acceptCandidate() -> void;
  auto removeConnections() -> void;
  auto setFocusToCurrentFindSupport() const -> bool;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

signals:
  auto changed() -> void;
  auto candidateChanged() -> void;

private:
  auto updateCandidateFindFilter(const QWidget *old, QWidget *now) -> void;
  auto clearFindSupport() -> void;
  auto aggregationChanged() -> void;
  auto candidateAggregationChanged() -> void;
  auto removeFindSupportConnections() -> void;

  QPointer<IFindSupport> m_current_find;
  QPointer<QWidget> m_current_widget;
  QPointer<IFindSupport> m_candidate_find;
  QPointer<QWidget> m_candidate_widget;
};

} // namespace Internal
} // namespace Core
