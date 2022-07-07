// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "threadutils.hpp"

#include <QCoreApplication>
#include <QThread>

namespace Utils {

auto isMainThread() -> bool
{
  return QThread::currentThread() == qApp->thread();
}

} // namespace Utils
