// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "futuresynchronizer.h"

/*! \class Utils::FutureSynchronizer

  \brief The FutureSynchronizer is an enhanced version of QFutureSynchronizer.
*/

namespace Utils {

FutureSynchronizer::~FutureSynchronizer()
{
  waitForFinished();
}

auto FutureSynchronizer::isEmpty() const -> bool
{
  return m_futures.isEmpty();
}

auto FutureSynchronizer::waitForFinished() -> void
{
  if (m_cancelOnWait)
    cancelAllFutures();
  for (QFuture<void> &future : m_futures)
    future.waitForFinished();
  clearFutures();
}

auto FutureSynchronizer::cancelAllFutures() -> void
{
  for (QFuture<void> &future : m_futures)
    future.cancel();
}

auto FutureSynchronizer::clearFutures() -> void
{
  m_futures.clear();
}

auto FutureSynchronizer::setCancelOnWait(bool enabled) -> void
{
  m_cancelOnWait = enabled;
}

auto FutureSynchronizer::isCancelOnWait() const -> bool
{
  return m_cancelOnWait;
}

auto FutureSynchronizer::flushFinishedFutures() -> void
{
  QList<QFuture<void>> newFutures;
  for (const QFuture<void> &future : qAsConst(m_futures)) {
    if (!future.isFinished())
      newFutures.append(future);
  }
  m_futures = newFutures;
}

} // namespace Utils
