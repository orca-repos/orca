// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "applicationlauncher.hpp"
#ifdef Q_OS_WIN
#include "windebuginterface.hpp"
#include <qt_windows.h>
#endif

#include <core/core-interface.hpp>

#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include "devicesupport/deviceprocess.hpp"
#include "projectexplorer.hpp"
#include "projectexplorersettings.hpp"
#include "runcontrol.hpp"

#include <QTextCodec>
#include <QTimer>

/*!
    \class ProjectExplorer::ApplicationLauncher

    \brief The ApplicationLauncher class is the application launcher of the
    ProjectExplorer plugin.

    Encapsulates processes running in a console or as GUI processes,
    captures debug output of GUI processes on Windows (outputDebugString()).

    \sa Utils::QtcProcess
*/

using namespace Utils;

namespace ProjectExplorer {

using namespace Internal;

namespace Internal {

class ApplicationLauncherPrivate : public QObject {
public:
  enum State {
    Inactive,
    Run
  };

  explicit ApplicationLauncherPrivate(ApplicationLauncher *parent);
  ~ApplicationLauncherPrivate() override { setFinished(); }

  auto start(const Runnable &runnable, const IDevice::ConstPtr &device, bool local) -> void;
  auto stop() -> void;

  // Local
  auto handleProcessStarted() -> void;
  auto localProcessError(QProcess::ProcessError error) -> void;
  auto readLocalStandardOutput() -> void;
  auto readLocalStandardError() -> void;
  auto cannotRetrieveLocalDebugOutput() -> void;
  auto checkLocalDebugOutput(qint64 pid, const QString &message) -> void;
  auto localProcessDone(int, QProcess::ExitStatus) -> void;
  auto applicationPID() const -> qint64;
  auto isLocalRunning() const -> bool;

  // Remote
  auto doReportError(const QString &message, QProcess::ProcessError error = QProcess::FailedToStart) -> void;
  auto handleRemoteStderr() -> void;
  auto handleRemoteStdout() -> void;
  auto handleApplicationFinished() -> void;
  auto setFinished() -> void;
  auto handleApplicationError(QProcess::ProcessError error) -> void;
  
  ApplicationLauncher *q;

  bool m_isLocal = true;
  bool m_runAsRoot = false;

  // Local
  std::unique_ptr<QtcProcess> m_localProcess;
  bool m_useTerminal = false;
  QProcess::ProcessChannelMode m_processChannelMode;
  // Keep track whether we need to emit a finished signal
  bool m_processRunning = false;
  QTextCodec *m_outputCodec;
  QTextCodec::ConverterState m_outputCodecState;
  QTextCodec::ConverterState m_errorCodecState;
  qint64 m_listeningPid = 0;

  // Remote
  DeviceProcess *m_deviceProcess = nullptr;
  QString m_remoteErrorString;
  QProcess::ProcessError m_remoteError = QProcess::UnknownError;
  QProcess::ExitStatus m_remoteExitStatus = QProcess::CrashExit;
  State m_state = Inactive;
  bool m_stopRequested = false;
};

} // Internal

static auto defaultProcessChannelMode() -> QProcess::ProcessChannelMode
{
  return ProjectExplorerPlugin::appOutputSettings().mergeChannels ? QProcess::MergedChannels : QProcess::SeparateChannels;
}

ApplicationLauncherPrivate::ApplicationLauncherPrivate(ApplicationLauncher *parent) : q(parent), m_processChannelMode(defaultProcessChannelMode()), m_outputCodec(QTextCodec::codecForLocale())
{
  #ifdef Q_OS_WIN
  connect(WinDebugInterface::instance(), &WinDebugInterface::cannotRetrieveDebugOutput, this, &ApplicationLauncherPrivate::cannotRetrieveLocalDebugOutput);
  connect(WinDebugInterface::instance(), &WinDebugInterface::debugOutput, this, &ApplicationLauncherPrivate::checkLocalDebugOutput);
  #endif
}

ApplicationLauncher::ApplicationLauncher(QObject *parent) : QObject(parent), d(std::make_unique<ApplicationLauncherPrivate>(this)) {}

ApplicationLauncher::~ApplicationLauncher() = default;

auto ApplicationLauncher::setProcessChannelMode(QProcess::ProcessChannelMode mode) -> void
{
  d->m_processChannelMode = mode;
}

auto ApplicationLauncher::setUseTerminal(bool on) -> void
{
  d->m_useTerminal = on;
}

auto ApplicationLauncher::setRunAsRoot(bool on) -> void
{
  d->m_runAsRoot = on;
}

auto ApplicationLauncher::stop() -> void
{
  d->stop();
}

auto ApplicationLauncherPrivate::stop() -> void
{
  if (m_isLocal) {
    if (!isLocalRunning())
      return;
    QTC_ASSERT(m_localProcess, return);
    m_localProcess->stopProcess();
    localProcessDone(0, QProcess::CrashExit);
  } else {
    if (m_stopRequested)
      return;
    m_stopRequested = true;
    m_remoteExitStatus = QProcess::CrashExit;
    emit q->appendMessage(ApplicationLauncher::tr("User requested stop. Shutting down..."), NormalMessageFormat);
    switch (m_state) {
    case Run:
      m_deviceProcess->terminate();
      break;
    case Inactive:
      break;
    }
  }
}

auto ApplicationLauncher::isRunning() const -> bool
{
  return d->isLocalRunning();
}

auto ApplicationLauncher::isLocal() const -> bool
{
  return d->m_isLocal;
}

auto ApplicationLauncherPrivate::isLocalRunning() const -> bool
{
  if (!m_localProcess)
    return false;
  return m_localProcess->state() != QProcess::NotRunning;
}

auto ApplicationLauncher::applicationPID() const -> ProcessHandle
{
  return ProcessHandle(d->applicationPID());
}

auto ApplicationLauncherPrivate::applicationPID() const -> qint64
{
  if (!isLocalRunning())
    return 0;

  return m_localProcess->processId();
}

auto ApplicationLauncher::errorString() const -> QString
{
  if (d->m_isLocal)
    return d->m_localProcess ? d->m_localProcess->errorString() : QString();
  return d->m_remoteErrorString;
}

auto ApplicationLauncher::processError() const -> QProcess::ProcessError
{
  if (d->m_isLocal)
    return d->m_localProcess ? d->m_localProcess->error() : QProcess::UnknownError;
  return d->m_remoteError;
}

auto ApplicationLauncherPrivate::localProcessError(QProcess::ProcessError error) -> void
{
  // TODO: why below handlings are different?
  if (m_useTerminal) {
    emit q->appendMessage(m_localProcess->errorString(), ErrorMessageFormat);
    if (m_processRunning && m_localProcess->processId() == 0) {
      m_processRunning = false;
      emit q->processExited(-1, QProcess::NormalExit);
    }
  } else {
    QString error;
    auto status = QProcess::NormalExit;
    switch (m_localProcess->error()) {
    case QProcess::FailedToStart:
      error = ApplicationLauncher::tr("Failed to start program. Path or permissions wrong?");
      break;
    case QProcess::Crashed:
      status = QProcess::CrashExit;
      break;
    default:
      error = ApplicationLauncher::tr("Some error has occurred while running the program.");
    }
    if (!error.isEmpty()) emit q->appendMessage(error, ErrorMessageFormat);
    if (m_processRunning && !isLocalRunning()) {
      m_processRunning = false;
      emit q->processExited(-1, status);
    }
  }
  emit q->error(error);
}

auto ApplicationLauncherPrivate::readLocalStandardOutput() -> void
{
  const auto data = m_localProcess->readAllStandardOutput();
  const auto msg = m_outputCodec->toUnicode(data.constData(), data.length(), &m_outputCodecState);
  emit q->appendMessage(msg, StdOutFormat, false);
}

auto ApplicationLauncherPrivate::readLocalStandardError() -> void
{
  const auto data = m_localProcess->readAllStandardError();
  const auto msg = m_outputCodec->toUnicode(data.constData(), data.length(), &m_errorCodecState);
  emit q->appendMessage(msg, StdErrFormat, false);
}

auto ApplicationLauncherPrivate::cannotRetrieveLocalDebugOutput() -> void
{
  #ifdef Q_OS_WIN
  disconnect(WinDebugInterface::instance(), nullptr, this, nullptr);
  emit q->appendMessage(ApplicationLauncher::msgWinCannotRetrieveDebuggingOutput(), ErrorMessageFormat);
  #endif
}

auto ApplicationLauncherPrivate::checkLocalDebugOutput(qint64 pid, const QString &message) -> void
{
  if (m_listeningPid == pid) emit q->appendMessage(message, DebugFormat);
}

auto ApplicationLauncherPrivate::localProcessDone(int exitCode, QProcess::ExitStatus status) -> void
{
  QTimer::singleShot(100, this, [this, exitCode, status]() {
    m_listeningPid = 0;
    emit q->processExited(exitCode, status);
  });
}

auto ApplicationLauncher::msgWinCannotRetrieveDebuggingOutput() -> QString
{
  return tr("Cannot retrieve debugging output.") + QLatin1Char('\n');
}

auto ApplicationLauncherPrivate::handleProcessStarted() -> void
{
  m_listeningPid = applicationPID();
  emit q->processStarted();
}

auto ApplicationLauncher::start(const Runnable &runnable) -> void
{
  d->start(runnable, IDevice::ConstPtr(), true);
}

auto ApplicationLauncher::start(const Runnable &runnable, const IDevice::ConstPtr &device) -> void
{
  d->start(runnable, device, false);
}

auto ApplicationLauncherPrivate::start(const Runnable &runnable, const IDevice::ConstPtr &device, bool local) -> void
{
  m_isLocal = local;

  if (m_isLocal) {
    const auto terminalMode = m_useTerminal ? QtcProcess::TerminalOn : QtcProcess::TerminalOff;
    m_localProcess.reset(new QtcProcess(terminalMode, this));
    m_localProcess->setProcessChannelMode(m_processChannelMode);

    if (m_processChannelMode == QProcess::SeparateChannels) {
      connect(m_localProcess.get(), &QtcProcess::readyReadStandardError, this, &ApplicationLauncherPrivate::readLocalStandardError);
    }
    if (!m_useTerminal) {
      connect(m_localProcess.get(), &QtcProcess::readyReadStandardOutput, this, &ApplicationLauncherPrivate::readLocalStandardOutput);
    }

    connect(m_localProcess.get(), &QtcProcess::started, this, &ApplicationLauncherPrivate::handleProcessStarted);
    connect(m_localProcess.get(), &QtcProcess::finished, this, [this] {
      localProcessDone(m_localProcess->exitCode(), m_localProcess->exitStatus());
    });
    connect(m_localProcess.get(), &QtcProcess::errorOccurred, this, &ApplicationLauncherPrivate::localProcessError);

    // Work around QTBUG-17529 (QtDeclarative fails with 'File name case mismatch' ...)
    const auto fixedPath = runnable.workingDirectory.normalizedPathName();
    m_localProcess->setWorkingDirectory(fixedPath);

    auto env = runnable.environment;
    if (m_runAsRoot)
      RunControl::provideAskPassEntry(env);

    m_localProcess->setEnvironment(env);

    m_processRunning = true;
    #ifdef Q_OS_WIN
    if (!WinDebugInterface::instance()->isRunning())
      WinDebugInterface::instance()->start(); // Try to start listener again...
    #endif

    auto cmdLine = runnable.command;

    if (HostOsInfo::isMacHost()) {
      CommandLine disclaim(Orca::Plugin::Core::ICore::libexecPath("disclaim"));
      disclaim.addCommandLineAsArgs(cmdLine);
      cmdLine = disclaim;
    }

    m_localProcess->setRunAsRoot(m_runAsRoot);
    m_localProcess->setCommand(cmdLine);
    m_localProcess->start();
  } else {
    QTC_ASSERT(m_state == Inactive, return);

    m_state = Run;
    if (!device) {
      doReportError(ApplicationLauncher::tr("Cannot run: No device."));
      setFinished();
      return;
    }

    if (!device->canCreateProcess()) {
      doReportError(ApplicationLauncher::tr("Cannot run: Device is not able to create processes."));
      setFinished();
      return;
    }

    if (!device->isEmptyCommandAllowed() && runnable.command.isEmpty()) {
      doReportError(ApplicationLauncher::tr("Cannot run: No command given."));
      setFinished();
      return;
    }

    m_stopRequested = false;
    m_remoteExitStatus = QProcess::NormalExit;

    m_deviceProcess = device->createProcess(this);
    m_deviceProcess->setRunInTerminal(m_useTerminal);
    connect(m_deviceProcess, &DeviceProcess::started, q, &ApplicationLauncher::processStarted);
    connect(m_deviceProcess, &DeviceProcess::readyReadStandardOutput, this, &ApplicationLauncherPrivate::handleRemoteStdout);
    connect(m_deviceProcess, &DeviceProcess::readyReadStandardError, this, &ApplicationLauncherPrivate::handleRemoteStderr);
    connect(m_deviceProcess, &DeviceProcess::errorOccurred, this, &ApplicationLauncherPrivate::handleApplicationError);
    connect(m_deviceProcess, &DeviceProcess::finished, this, &ApplicationLauncherPrivate::handleApplicationFinished);
    m_deviceProcess->start(runnable);
  }
}

auto ApplicationLauncherPrivate::handleApplicationError(QProcess::ProcessError error) -> void
{
  if (error == QProcess::FailedToStart) {
    doReportError(ApplicationLauncher::tr("Application failed to start: %1").arg(m_deviceProcess->errorString()));
    setFinished();
  }
}

auto ApplicationLauncherPrivate::setFinished() -> void
{
  if (m_state == Inactive)
    return;

  auto exitCode = 0;
  if (m_deviceProcess)
    exitCode = m_deviceProcess->exitCode();

  m_state = Inactive;
  emit q->processExited(exitCode, m_remoteExitStatus);
}

auto ApplicationLauncherPrivate::handleApplicationFinished() -> void
{
  QTC_ASSERT(m_state == Run, return);

  if (m_deviceProcess->exitStatus() == QProcess::CrashExit)
    doReportError(m_deviceProcess->errorString(), QProcess::Crashed);
  setFinished();
}

auto ApplicationLauncherPrivate::handleRemoteStdout() -> void
{
  QTC_ASSERT(m_state == Run, return);
  const auto output = m_deviceProcess->readAllStandardOutput();
  emit q->appendMessage(QString::fromUtf8(output), StdOutFormat, false);
}

auto ApplicationLauncherPrivate::handleRemoteStderr() -> void
{
  QTC_ASSERT(m_state == Run, return);
  const auto output = m_deviceProcess->readAllStandardError();
  emit q->appendMessage(QString::fromUtf8(output), StdErrFormat, false);
}

auto ApplicationLauncherPrivate::doReportError(const QString &message, QProcess::ProcessError error) -> void
{
  m_remoteErrorString = message;
  m_remoteError = error;
  m_remoteExitStatus = QProcess::CrashExit;
  emit q->error(error);
}

} // namespace ProjectExplorer
