// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "launcherinterface.h"

#include "filepath.h"
#include "launcherpackets.h"
#include "launchersocket.h"
#include "qtcassert.h"
#include "temporarydirectory.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QProcess>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace Utils {
namespace Internal {

class LauncherProcess : public QProcess {
public:
  LauncherProcess(QObject *parent) : QProcess(parent)
  {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && defined(Q_OS_UNIX)
        setChildProcessModifier([this] { setupChildProcess_impl(); });
    #endif
  }

private:
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void setupChildProcess() override
    {
        setupChildProcess_impl();
    }
  #endif

  auto setupChildProcess_impl() -> void
  {
    #ifdef Q_OS_UNIX
        const auto pid = static_cast<pid_t>(processId());
        setpgid(pid, pid);
    #endif
  }
};

static auto launcherSocketName() -> QString
{
  return Utils::TemporaryDirectory::masterDirectoryPath() + QStringLiteral("/launcher-%1").arg(QString::number(qApp->applicationPid()));
}

class LauncherInterfacePrivate : public QObject {
  Q_OBJECT

public:
  LauncherInterfacePrivate();
  ~LauncherInterfacePrivate() override;

  auto doStart() -> void;
  auto doStop() -> void;
  auto handleNewConnection() -> void;
  auto handleProcessError() -> void;
  auto handleProcessFinished() -> void;
  auto handleProcessStderr() -> void;
  auto socket() const -> Internal::LauncherSocket* { return m_socket; }

  auto setPathToLauncher(const QString &path) -> void
  {
    if (!path.isEmpty())
      m_pathToLauncher = path;
  }

  auto launcherFilePath() const -> QString { return m_pathToLauncher + QLatin1String("/orca_processlauncher"); }

signals:
  auto errorOccurred(const QString &error) -> void;

private:
  QLocalServer *const m_server;
  Internal::LauncherSocket *const m_socket;
  Internal::LauncherProcess *m_process = nullptr;
  QString m_pathToLauncher;
};

LauncherInterfacePrivate::LauncherInterfacePrivate() : m_server(new QLocalServer(this)), m_socket(new LauncherSocket(this))
{
  m_pathToLauncher = qApp->applicationDirPath() + '/' + QLatin1String(RELATIVE_LIBEXEC_PATH);
  QObject::connect(m_server, &QLocalServer::newConnection, this, &LauncherInterfacePrivate::handleNewConnection);
}

LauncherInterfacePrivate::~LauncherInterfacePrivate()
{
  m_server->disconnect();
}

auto LauncherInterfacePrivate::doStart() -> void
{
  const QString &socketName = launcherSocketName();
  QLocalServer::removeServer(socketName);
  if (!m_server->listen(socketName)) {
    emit errorOccurred(m_server->errorString());
    return;
  }
  m_process = new LauncherProcess(this);
  connect(m_process, &QProcess::errorOccurred, this, &LauncherInterfacePrivate::handleProcessError);
  connect(m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &LauncherInterfacePrivate::handleProcessFinished);
  connect(m_process, &QProcess::readyReadStandardError, this, &LauncherInterfacePrivate::handleProcessStderr);
  m_process->start(launcherFilePath(), QStringList(m_server->fullServerName()));
}

auto LauncherInterfacePrivate::doStop() -> void
{
  m_server->close();
  QTC_ASSERT(m_process, return);
  m_socket->shutdown();
  m_process->waitForFinished(-1); // Let the process interface finish so that it finishes
  // reaping any possible processes it has started.
  delete m_process;
  m_process = nullptr;
}

auto LauncherInterfacePrivate::handleNewConnection() -> void
{
  QLocalSocket *const socket = m_server->nextPendingConnection();
  if (!socket)
    return;
  m_server->close();
  m_socket->setSocket(socket);
}

auto LauncherInterfacePrivate::handleProcessError() -> void
{
  if (m_process->error() == QProcess::FailedToStart) {
    const QString launcherPathForUser = QDir::toNativeSeparators(QDir::cleanPath(m_process->program()));
    emit errorOccurred(QCoreApplication::translate("Utils::LauncherSocket", "Failed to start process launcher at \"%1\": %2").arg(launcherPathForUser, m_process->errorString()));
  }
}

auto LauncherInterfacePrivate::handleProcessFinished() -> void
{
  emit errorOccurred(QCoreApplication::translate("Utils::LauncherSocket", "Process launcher closed unexpectedly: %1").arg(m_process->errorString()));
}

auto LauncherInterfacePrivate::handleProcessStderr() -> void
{
  qDebug() << "[launcher]" << m_process->readAllStandardError();
}

} // namespace Internal

using namespace Utils::Internal;

static QMutex s_instanceMutex;
static QString s_pathToLauncher;
static std::atomic_bool s_started = false;

LauncherInterface::LauncherInterface() : m_private(new LauncherInterfacePrivate())
{
  m_private->moveToThread(&m_thread);
  QObject::connect(&m_thread, &QThread::finished, m_private, &QObject::deleteLater);
  m_thread.start();
  m_thread.moveToThread(qApp->thread());

  m_private->setPathToLauncher(s_pathToLauncher);
  const FilePath launcherFilePath = FilePath::fromString(m_private->launcherFilePath()).cleanPath().withExecutableSuffix();
  auto launcherIsNotExecutable = [&launcherFilePath]() {
    qWarning() << "The Creator's process launcher" << launcherFilePath << "is not executable.";
  };
  QTC_ASSERT(launcherFilePath.isExecutableFile(), launcherIsNotExecutable(); return);
  s_started = true;
  // Call in launcher's thread.
  QMetaObject::invokeMethod(m_private, &LauncherInterfacePrivate::doStart);
}

LauncherInterface::~LauncherInterface()
{
  QMutexLocker locker(&s_instanceMutex);
  LauncherInterfacePrivate *p = instance()->m_private;
  // Call in launcher's thread.
  QMetaObject::invokeMethod(p, &LauncherInterfacePrivate::doStop, Qt::BlockingQueuedConnection);
  m_thread.quit();
  m_thread.wait();
}

auto LauncherInterface::setPathToLauncher(const QString &pathToLauncher) -> void
{
  s_pathToLauncher = pathToLauncher;
}

auto LauncherInterface::isStarted() -> bool
{
  return s_started;
}

auto LauncherInterface::isReady() -> bool
{
  QMutexLocker locker(&s_instanceMutex);
  return instance()->m_private->socket()->isReady();
}

auto LauncherInterface::sendData(const QByteArray &data) -> void
{
  QMutexLocker locker(&s_instanceMutex);
  instance()->m_private->socket()->sendData(data);
}

auto LauncherInterface::registerHandle(QObject *parent, quintptr token, ProcessMode mode) -> Utils::Internal::CallerHandle*
{
  QMutexLocker locker(&s_instanceMutex);
  return instance()->m_private->socket()->registerHandle(parent, token, mode);
}

auto LauncherInterface::unregisterHandle(quintptr token) -> void
{
  QMutexLocker locker(&s_instanceMutex);
  instance()->m_private->socket()->unregisterHandle(token);
}

} // namespace Utils

#include "launcherinterface.moc"
