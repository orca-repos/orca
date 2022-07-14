// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-current-document-find.hpp"

#include "core-constants.hpp"

#include <aggregation/aggregate.hpp>

#include <utils/fadingindicator.hpp>
#include <utils/qtcassert.hpp>

#include <QApplication>
#include <QDebug>
#include <QWidget>

namespace Orca::Plugin::Core {

CurrentDocumentFind::CurrentDocumentFind() : m_current_find(nullptr)
{
  connect(qApp, &QApplication::focusChanged, this, &CurrentDocumentFind::updateCandidateFindFilter);
}

auto CurrentDocumentFind::removeConnections() -> void
{
  disconnect(qApp, nullptr, this, nullptr);
  removeFindSupportConnections();
}

auto CurrentDocumentFind::resetIncrementalSearch() const -> void
{
  QTC_ASSERT(m_current_find, return);
  m_current_find->resetIncrementalSearch();
}

auto CurrentDocumentFind::clearHighlights() const -> void
{
  QTC_ASSERT(m_current_find, return);
  m_current_find->clearHighlights();
}

auto CurrentDocumentFind::isEnabled() const -> bool
{
  return m_current_find && (!m_current_widget || m_current_widget->isVisible());
}

auto CurrentDocumentFind::candidate() const -> IFindSupport*
{
  return m_candidate_find;
}

auto CurrentDocumentFind::supportsReplace() const -> bool
{
  if (!m_current_find)
    return false;
  return m_current_find->supportsReplace();
}

auto CurrentDocumentFind::supportsSelectAll() const -> bool
{
  if (!m_current_find)
    return false;
  return m_current_find->supportsSelectAll();
}

auto CurrentDocumentFind::supportedFindFlags() const -> FindFlags
{
  QTC_ASSERT(m_current_find, return {});
  return m_current_find->supportedFindFlags();
}

auto CurrentDocumentFind::currentFindString() const -> QString
{
  if (!m_current_find)
    return {};
  return m_current_find->currentFindString();
}

auto CurrentDocumentFind::completedFindString() const -> QString
{
  QTC_ASSERT(m_current_find, return QString());
  return m_current_find->completedFindString();
}

auto CurrentDocumentFind::highlightAll(const QString &txt, const FindFlags find_flags) const -> void
{
  if (!m_current_find)
    return;
  m_current_find->highlightAll(txt, find_flags);
}

auto CurrentDocumentFind::findIncremental(const QString &txt, const FindFlags find_flags) const -> IFindSupport::Result
{
  QTC_ASSERT(m_current_find, return IFindSupport::NotFound);
  return m_current_find->findIncremental(txt, find_flags);
}

auto CurrentDocumentFind::findStep(const QString &txt, const FindFlags find_flags) const -> IFindSupport::Result
{
  QTC_ASSERT(m_current_find, return IFindSupport::NotFound);
  return m_current_find->findStep(txt, find_flags);
}

auto CurrentDocumentFind::selectAll(const QString &txt, const FindFlags find_flags) const -> void
{
  QTC_ASSERT(m_current_find && m_current_find->supportsSelectAll(), return);
  m_current_find->selectAll(txt, find_flags);
}

auto CurrentDocumentFind::replace(const QString &before, const QString &after, const FindFlags find_flags) const -> void
{
  QTC_ASSERT(m_current_find, return);
  m_current_find->replace(before, after, find_flags);
}

auto CurrentDocumentFind::replaceStep(const QString &before, const QString &after, const FindFlags find_flags) const -> bool
{
  QTC_ASSERT(m_current_find, return false);
  return m_current_find->replaceStep(before, after, find_flags);
}

auto CurrentDocumentFind::replaceAll(const QString &before, const QString &after, const FindFlags find_flags) const -> int
{
  QTC_ASSERT(m_current_find, return 0);
  QTC_CHECK(m_current_widget);
  const auto count = m_current_find->replaceAll(before, after, find_flags);
  showText(m_current_widget, tr("%n occurrences replaced.", nullptr, count), Utils::FadingIndicator::SmallText);
  return count;
}

auto CurrentDocumentFind::defineFindScope() const -> void
{
  if (!m_current_find)
    return;
  m_current_find->defineFindScope();
}

auto CurrentDocumentFind::clearFindScope() const -> void
{
  QTC_ASSERT(m_current_find, return);
  m_current_find->clearFindScope();
}

auto CurrentDocumentFind::updateCandidateFindFilter(const QWidget *old, QWidget *now) -> void
{
  Q_UNUSED(old)
  auto candidate = now;
  QPointer<IFindSupport> impl = nullptr;

  while (!impl && candidate) {
    impl = Aggregation::query<IFindSupport>(candidate);
    if (!impl)
      candidate = candidate->parentWidget();
  }

  if (candidate == m_candidate_widget && impl == m_candidate_find) {
    // trigger update of action state since a changed focus can still require disabling the
    // Find/Replace action
    emit changed();
    return;
  }

  if (m_candidate_widget)
    disconnect(Aggregation::Aggregate::parentAggregate(m_candidate_widget), &Aggregation::Aggregate::changed, this, &CurrentDocumentFind::candidateAggregationChanged);

  m_candidate_widget = candidate;
  m_candidate_find = impl;

  if (m_candidate_widget)
    connect(Aggregation::Aggregate::parentAggregate(m_candidate_widget), &Aggregation::Aggregate::changed, this, &CurrentDocumentFind::candidateAggregationChanged);

  emit candidateChanged();
}

auto CurrentDocumentFind::acceptCandidate() -> void
{
  if (!m_candidate_find || m_candidate_find == m_current_find)
    return;

  removeFindSupportConnections();

  if (m_current_find)
    m_current_find->clearHighlights();

  if (m_current_widget)
    disconnect(Aggregation::Aggregate::parentAggregate(m_current_widget), &Aggregation::Aggregate::changed, this, &CurrentDocumentFind::aggregationChanged);

  m_current_widget = m_candidate_widget;
  connect(Aggregation::Aggregate::parentAggregate(m_current_widget), &Aggregation::Aggregate::changed, this, &CurrentDocumentFind::aggregationChanged);

  m_current_find = m_candidate_find;

  if (m_current_find) {
    connect(m_current_find.data(), &IFindSupport::changed, this, &CurrentDocumentFind::changed);
    connect(m_current_find.data(), &QObject::destroyed, this, &CurrentDocumentFind::clearFindSupport);
  }

  if (m_current_widget)
    m_current_widget->installEventFilter(this);

  emit changed();
}

auto CurrentDocumentFind::removeFindSupportConnections() -> void
{
  if (m_current_find) {
    disconnect(m_current_find.data(), &IFindSupport::changed, this, &CurrentDocumentFind::changed);
    disconnect(m_current_find.data(), &IFindSupport::destroyed, this, &CurrentDocumentFind::clearFindSupport);
  }

  if (m_current_widget)
    m_current_widget->removeEventFilter(this);
}

auto CurrentDocumentFind::clearFindSupport() -> void
{
  removeFindSupportConnections();
  m_current_widget = nullptr;
  m_current_find = nullptr;
  emit changed();
}

auto CurrentDocumentFind::setFocusToCurrentFindSupport() const -> bool
{
  if (m_current_find && m_current_widget) {
    auto w = m_current_widget->focusWidget();
    if (!w)
      w = m_current_widget;
    w->setFocus();
    return true;
  }
  return false;
}

auto CurrentDocumentFind::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if (m_current_widget && obj == m_current_widget) {
    if (event->type() == QEvent::Hide || event->type() == QEvent::Show) emit changed();
  }
  return QObject::eventFilter(obj, event);
}

auto CurrentDocumentFind::aggregationChanged() -> void
{
  if (m_current_widget) {
    if (const auto current_find = Aggregation::query<IFindSupport>(m_current_widget); current_find != m_current_find) {
      // There's a change in the find support
      if (current_find) {
        m_candidate_widget = m_current_widget;
        m_candidate_find = current_find;
        acceptCandidate();
      } else {
        clearFindSupport();
      }
    }
  }
}

auto CurrentDocumentFind::candidateAggregationChanged() -> void
{
  if (m_candidate_widget && m_candidate_widget != m_current_widget) {
    m_candidate_find = Aggregation::query<IFindSupport>(m_candidate_widget);
    emit candidateChanged();
  }
}

} // namespace Orca::Plugin::Core
