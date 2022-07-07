// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runextensions.hpp"

namespace Utils {
namespace Internal {

RunnableThread::RunnableThread(QRunnable *runnable, QObject *parent) : QThread(parent), m_runnable(runnable) {}

auto RunnableThread::run() -> void
{
  m_runnable->run();
  if (m_runnable->autoDelete())
    delete m_runnable;
}

} // Internal
} // Utils
