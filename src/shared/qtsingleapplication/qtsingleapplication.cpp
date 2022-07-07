// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtsingleapplication.hpp"
#include "qtlocalpeer.hpp"

#include <qtlockedfile.hpp>

#include <QDir>
#include <QFileOpenEvent>
#include <QSharedMemory>
#include <QWidget>

namespace SharedTools {

static const int instancesSize = 1024;

static auto instancesLockFilename(const QString &appSessionId) -> QString
{
  const QChar slash(QLatin1Char('/'));
  QString res = QDir::tempPath();
  if (!res.endsWith(slash))
    res += slash;
  return res + appSessionId + QLatin1String("-instances");
}

QtSingleApplication::QtSingleApplication(const QString &appId, int &argc, char **argv) : QApplication(argc, argv), firstPeer(-1), pidPeer(0)
{
  this->appId = appId;

  const QString appSessionId = QtLocalPeer::appSessionId(appId);

  // This shared memory holds a zero-terminated array of active (or crashed) instances
  instances = new QSharedMemory(appSessionId, this);
  actWin = 0;
  block = false;

  // First instance creates the shared memory, later instances attach to it
  const bool created = instances->create(instancesSize);
  if (!created) {
    if (!instances->attach()) {
      qWarning() << "Failed to initialize instances shared memory: " << instances->errorString();
      delete instances;
      instances = 0;
      return;
    }
  }

  // QtLockedFile is used to workaround QTBUG-10364
  QtLockedFile lockfile(instancesLockFilename(appSessionId));

  lockfile.open(QtLockedFile::ReadWrite);
  lockfile.lock(QtLockedFile::WriteLock);
  qint64 *pids = static_cast<qint64*>(instances->data());
  if (!created) {
    // Find the first instance that it still running
    // The whole list needs to be iterated in order to append to it
    for (; *pids; ++pids) {
      if (firstPeer == -1 && isRunning(*pids))
        firstPeer = *pids;
    }
  }
  // Add current pid to list and terminate it
  *pids++ = applicationPid();
  *pids = 0;
  pidPeer = new QtLocalPeer(this, appId + QLatin1Char('-') + QString::number(applicationPid()));
  connect(pidPeer, &QtLocalPeer::messageReceived, this, &QtSingleApplication::messageReceived);
  pidPeer->isClient();
  lockfile.unlock();
}

QtSingleApplication::~QtSingleApplication()
{
  if (!instances)
    return;
  const qint64 appPid = applicationPid();
  QtLockedFile lockfile(instancesLockFilename(QtLocalPeer::appSessionId(appId)));
  lockfile.open(QtLockedFile::ReadWrite);
  lockfile.lock(QtLockedFile::WriteLock);
  // Rewrite array, removing current pid and previously crashed ones
  qint64 *pids = static_cast<qint64*>(instances->data());
  qint64 *newpids = pids;
  for (; *pids; ++pids) {
    if (*pids != appPid && isRunning(*pids))
      *newpids++ = *pids;
  }
  *newpids = 0;
  lockfile.unlock();
}

auto QtSingleApplication::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::FileOpen) {
    QFileOpenEvent *foe = static_cast<QFileOpenEvent*>(event);
    emit fileOpenRequest(foe->file());
    return true;
  }
  return QApplication::event(event);
}

auto QtSingleApplication::isRunning(qint64 pid) -> bool
{
  if (pid == -1) {
    pid = firstPeer;
    if (pid == -1)
      return false;
  }

  QtLocalPeer peer(this, appId + QLatin1Char('-') + QString::number(pid, 10));
  return peer.isClient();
}

auto QtSingleApplication::sendMessage(const QString &message, int timeout, qint64 pid) -> bool
{
  if (pid == -1) {
    pid = firstPeer;
    if (pid == -1)
      return false;
  }

  QtLocalPeer peer(this, appId + QLatin1Char('-') + QString::number(pid, 10));
  return peer.sendMessage(message, timeout, block);
}

auto QtSingleApplication::applicationId() const -> QString
{
  return appId;
}

auto QtSingleApplication::setBlock(bool value) -> void
{
  block = value;
}

auto QtSingleApplication::setActivationWindow(QWidget *aw, bool activateOnMessage) -> void
{
  actWin = aw;
  if (!pidPeer)
    return;
  if (activateOnMessage)
    connect(pidPeer, &QtLocalPeer::messageReceived, this, &QtSingleApplication::activateWindow);
  else
    disconnect(pidPeer, &QtLocalPeer::messageReceived, this, &QtSingleApplication::activateWindow);
}

auto QtSingleApplication::activationWindow() const -> QWidget*
{
  return actWin;
}

auto QtSingleApplication::activateWindow() -> void
{
  if (actWin) {
    actWin->setWindowState(actWin->windowState() & ~Qt::WindowMinimized);
    actWin->raise();
    actWin->activateWindow();
  }
}

} // namespace SharedTools
