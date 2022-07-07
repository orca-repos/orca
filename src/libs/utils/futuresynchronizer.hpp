// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"
#include <QtGlobal>

#include <QFuture>
#include <QList>

namespace Utils {

class ORCA_UTILS_EXPORT FutureSynchronizer final {
public:
  FutureSynchronizer() = default;
  ~FutureSynchronizer();

  template <typename T>
  auto addFuture(const QFuture<T> &future) -> void
  {
    m_futures.append(QFuture<void>(future));
    flushFinishedFutures();
  }

  auto isEmpty() const -> bool;
  auto waitForFinished() -> void;
  auto cancelAllFutures() -> void;
  auto clearFutures() -> void;
  auto setCancelOnWait(bool enabled) -> void;
  auto isCancelOnWait() const -> bool; // TODO: The original contained cancelOnWait, what suggests action, not a getter
  auto flushFinishedFutures() -> void;

private:
  QList<QFuture<void>> m_futures;
  bool m_cancelOnWait = false; // TODO: True default makes more sense...
};

} // namespace Utils
