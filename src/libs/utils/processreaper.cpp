// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processreaper.h"
#include "qtcassert.h"

#include <QCoreApplication>
#include <QProcess>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_WIN
#ifdef ORCA_PCH_H
#define CALLBACK WINAPI
#endif
#include <qt_windows.h>
#endif

using namespace Utils;

namespace Utils {
namespace Internal {

class Reaper final : public QObject {
public:
  Reaper(QProcess *p, int timeoutMs);
  ~Reaper();

  auto timeoutMs() const -> int;
  auto isFinished() const -> bool;
  auto nextIteration() -> void;

private:
  mutable QTimer m_iterationTimer;
  QProcess *m_process = nullptr;
  int m_emergencyCounter = 0;
  QProcess::ProcessState m_lastState = QProcess::NotRunning;
};

static QList<Reaper*> g_reapers;

Reaper::Reaper(QProcess *p, int timeoutMs) : m_process(p)
{
  g_reapers.append(this);

  m_iterationTimer.setInterval(timeoutMs);
  m_iterationTimer.setSingleShot(true);
  connect(&m_iterationTimer, &QTimer::timeout, this, &Reaper::nextIteration);

  QMetaObject::invokeMethod(this, &Reaper::nextIteration, Qt::QueuedConnection);
}

Reaper::~Reaper()
{
  g_reapers.removeOne(this);
}

auto Reaper::timeoutMs() const -> int
{
  const int remaining = m_iterationTimer.remainingTime();
  if (remaining < 0)
    return m_iterationTimer.interval();
  m_iterationTimer.stop();
  return remaining;
}

auto Reaper::isFinished() const -> bool
{
  return !m_process;
}

#ifdef Q_OS_WIN
static auto sendMessage(UINT message, HWND hwnd, LPARAM lParam) -> BOOL
{
  DWORD dwProcessID;
  GetWindowThreadProcessId(hwnd, &dwProcessID);
  if ((DWORD)lParam == dwProcessID) {
    SendNotifyMessage(hwnd, message, 0, 0);
    return FALSE;
  }
  return TRUE;
}

auto CALLBACK sendShutDownMessageToAllWindowsOfProcess_enumWnd(HWND hwnd, LPARAM lParam) -> BOOL
{
  static UINT uiShutDownMessage = RegisterWindowMessage(L"qtcctrlcstub_shutdown");
  return sendMessage(uiShutDownMessage, hwnd, lParam);
}

#endif

auto Reaper::nextIteration() -> void
{
  QProcess::ProcessState state = m_process ? m_process->state() : QProcess::NotRunning;
  if (state == QProcess::NotRunning || m_emergencyCounter > 5) {
    delete m_process;
    m_process = nullptr;
    return;
  }

  if (state == QProcess::Starting) {
    if (m_lastState == QProcess::Starting)
      m_process->kill();
  } else if (state == QProcess::Running) {
    if (m_lastState == QProcess::Running) {
      m_process->kill();
    } else if (m_process->program().endsWith(QLatin1String("orca_ctrlc_stub.exe"))) {
      #ifdef Q_OS_WIN
      EnumWindows(sendShutDownMessageToAllWindowsOfProcess_enumWnd, m_process->processId());
      #endif
    } else {
      m_process->terminate();
    }
  }

  m_lastState = state;
  m_iterationTimer.start();

  ++m_emergencyCounter;
}

} // namespace Internal

ProcessReaper::~ProcessReaper()
{
  while (!Internal::g_reapers.isEmpty()) {
    int alreadyWaited = 0;
    QList<Internal::Reaper*> toDelete;

    // push reapers along:
    for (Internal::Reaper *pr : qAsConst(Internal::g_reapers)) {
      const int timeoutMs = pr->timeoutMs();
      if (alreadyWaited < timeoutMs) {
        const unsigned long toSleep = static_cast<unsigned long>(timeoutMs - alreadyWaited);
        QThread::msleep(toSleep);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        alreadyWaited += toSleep;
      }

      pr->nextIteration();

      if (pr->isFinished())
        toDelete.append(pr);
    }

    // Clean out reapers that finished in the meantime
    qDeleteAll(toDelete);
    toDelete.clear();
  }
}

auto ProcessReaper::reap(QProcess *process, int timeoutMs) -> void
{
  if (!process)
    return;

  QTC_ASSERT(QThread::currentThread() == process->thread(), return);

  process->disconnect();
  if (process->state() == QProcess::NotRunning) {
    process->deleteLater();
    return;
  }
  // Neither can move object with a parent into a different thread
  // nor reaping the process with a parent makes any sense.
  process->setParent(nullptr);
  if (process->thread() != QCoreApplication::instance()->thread()) {
    process->moveToThread(QCoreApplication::instance()->thread());
    QMetaObject::invokeMethod(process, [process, timeoutMs] {
      reap(process, timeoutMs);
    }); // will be queued
    return;
  }

  ProcessReaper::instance();
  new Internal::Reaper(process, timeoutMs);
}

} // namespace Utils

