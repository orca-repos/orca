// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "terminalprocess_p.h"

#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/commandline.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/terminalcommand.h>
#include <utils/winutils.h>

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QTimer>
#include <QWinEventNotifier>

#ifdef Q_OS_WIN

#  include <windows.h>
#  include <stdlib.h>
#  include <cstring>

#else

#  include <sys/stat.h>
#  include <sys/types.h>
#  include <errno.h>
#  include <string.h>
#  include <unistd.h>

#endif

namespace Utils {
namespace Internal {

static auto modeOption(QtcProcess::TerminalMode m) -> QString
{
  switch (m) {
  case QtcProcess::TerminalRun:
    return QLatin1String("run");
  case QtcProcess::TerminalDebug:
    return QLatin1String("debug");
  case QtcProcess::TerminalSuspend:
    return QLatin1String("suspend");
  case QtcProcess::TerminalOff: QTC_CHECK(false);
    break;
  }
  return {};
}

static auto msgCommChannelFailed(const QString &error) -> QString
{
  return TerminalProcess::tr("Cannot set up communication channel: %1").arg(error);
}

static auto msgPromptToClose() -> QString
{
  // Shown in a terminal which might have a different character set on Windows.
  return TerminalProcess::tr("Press <RETURN> to close this window...");
}

static auto msgCannotCreateTempFile(const QString &why) -> QString
{
  return TerminalProcess::tr("Cannot create temporary file: %1").arg(why);
}

static auto msgCannotWriteTempFile() -> QString
{
  return TerminalProcess::tr("Cannot write temporary file. Disk full?");
}

static auto msgCannotCreateTempDir(const QString &dir, const QString &why) -> QString
{
  return TerminalProcess::tr("Cannot create temporary directory \"%1\": %2").arg(dir, why);
}

static auto msgUnexpectedOutput(const QByteArray &what) -> QString
{
  return TerminalProcess::tr("Unexpected output from helper program (%1).").arg(QString::fromLatin1(what));
}

static auto msgCannotChangeToWorkDir(const FilePath &dir, const QString &why) -> QString
{
  return TerminalProcess::tr("Cannot change to working directory \"%1\": %2").arg(dir.toString(), why);
}

static auto msgCannotExecute(const QString &p, const QString &why) -> QString
{
  return TerminalProcess::tr("Cannot execute \"%1\": %2").arg(p, why);
}

class TerminalProcessPrivate {
public:
  TerminalProcessPrivate(QObject *parent, QtcProcess::ProcessImpl processImpl, QtcProcess::TerminalMode terminalMode) : m_terminalMode(terminalMode), m_process(processImpl, parent) {}

  const QtcProcess::TerminalMode m_terminalMode;
  FilePath m_workingDir;
  Environment m_environment;
  qint64 m_processId = 0;
  int m_exitCode = 0;
  CommandLine m_commandLine;
  QProcess::ExitStatus m_appStatus = QProcess::NormalExit;
  QLocalServer m_stubServer;
  QLocalSocket *m_stubSocket = nullptr;
  QTemporaryFile *m_tempFile = nullptr;
  QProcess::ProcessError m_error = QProcess::UnknownError;
  QString m_errorString;
  bool m_abortOnMetaChars = true;

  // Used on Unix only
  QtcProcess m_process;
  QTimer *m_stubConnectTimer = nullptr;
  QByteArray m_stubServerDir;

  // Used on Windows only
  qint64 m_appMainThreadId = 0;

  #ifdef Q_OS_WIN
  PROCESS_INFORMATION *m_pid = nullptr;
  HANDLE m_hInferior = NULL;
  QWinEventNotifier *inferiorFinishedNotifier = nullptr;
  QWinEventNotifier *processFinishedNotifier = nullptr;
  #endif
};

TerminalProcess::TerminalProcess(QObject *parent, QtcProcess::ProcessImpl processImpl, QtcProcess::TerminalMode terminalMode) : QObject(parent), d(new TerminalProcessPrivate(this, processImpl, terminalMode))
{
  connect(&d->m_stubServer, &QLocalServer::newConnection, this, &TerminalProcess::stubConnectionAvailable);

  d->m_process.setProcessChannelMode(QProcess::ForwardedChannels);
}

TerminalProcess::~TerminalProcess()
{
  stopProcess();
  delete d;
}

auto TerminalProcess::setCommand(const CommandLine &command) -> void
{
  d->m_commandLine = command;
}

auto TerminalProcess::commandLine() const -> const CommandLine&
{
  return d->m_commandLine;
}

auto TerminalProcess::setAbortOnMetaChars(bool abort) -> void
{
  d->m_abortOnMetaChars = abort;
}

auto TerminalProcess::applicationMainThreadID() const -> qint64
{
  if (HostOsInfo::isWindowsHost())
    return d->m_appMainThreadId;
  return -1;
}

auto TerminalProcess::start() -> void
{
  if (isRunning())
    return;

  d->m_errorString.clear();
  d->m_error = QProcess::UnknownError;

  #ifdef Q_OS_WIN

  QString pcmd;
  QString pargs;
  if (d->m_terminalMode != QtcProcess::TerminalRun) {
    // The debugger engines already pre-process the arguments.
    pcmd = d->m_commandLine.executable().toString();
    pargs = d->m_commandLine.arguments();
  } else {
    ProcessArgs outArgs;
    ProcessArgs::prepareCommand(d->m_commandLine, &pcmd, &outArgs, &d->m_environment, &d->m_workingDir);
    pargs = outArgs.toWindowsArgs();
  }

  const QString err = stubServerListen();
  if (!err.isEmpty()) {
    emitError(QProcess::FailedToStart, msgCommChannelFailed(err));
    return;
  }

  QStringList env = d->m_environment.toStringList();
  if (!env.isEmpty()) {
    d->m_tempFile = new QTemporaryFile();
    if (!d->m_tempFile->open()) {
      cleanupAfterStartFailure(msgCannotCreateTempFile(d->m_tempFile->errorString()));
      return;
    }
    QString outString;
    QTextStream out(&outString);
    // Add PATH and SystemRoot environment variables in case they are missing
    const QStringList fixedEnvironment = [env] {
      QStringList envStrings = env;
      // add PATH if necessary (for DLL loading)
      if (envStrings.filter(QRegularExpression("^PATH=.*", QRegularExpression::CaseInsensitiveOption)).isEmpty()) {
        QByteArray path = qgetenv("PATH");
        if (!path.isEmpty())
          envStrings.prepend(QString::fromLatin1("PATH=%1").arg(QString::fromLocal8Bit(path)));
      }
      // add systemroot if needed
      if (envStrings.filter(QRegularExpression("^SystemRoot=.*", QRegularExpression::CaseInsensitiveOption)).isEmpty()) {
        QByteArray systemRoot = qgetenv("SystemRoot");
        if (!systemRoot.isEmpty())
          envStrings.prepend(QString::fromLatin1("SystemRoot=%1").arg(QString::fromLocal8Bit(systemRoot)));
      }
      return envStrings;
    }();

    for (const QString &var : fixedEnvironment)
      out << var << QChar(0);
    out << QChar(0);
    const QTextCodec *textCodec = QTextCodec::codecForName("UTF-16LE");
    QTC_CHECK(textCodec);
    const QByteArray outBytes = textCodec ? textCodec->fromUnicode(outString) : QByteArray();
    if (!textCodec || d->m_tempFile->write(outBytes) < 0) {
      cleanupAfterStartFailure(msgCannotWriteTempFile());
      return;
    }
    d->m_tempFile->flush();
  }

  STARTUPINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  d->m_pid = new PROCESS_INFORMATION;
  ZeroMemory(d->m_pid, sizeof(PROCESS_INFORMATION));

  QString workDir = workingDirectory().toUserOutput();
  if (!workDir.isEmpty() && !workDir.endsWith(QLatin1Char('\\')))
    workDir.append(QLatin1Char('\\'));

  // Quote a Windows command line correctly for the "CreateProcess" API
  static const auto quoteWinCommand = [](const QString &program) {
    const QChar doubleQuote = QLatin1Char('"');

    // add the program as the first arg ... it works better
    QString programName = program;
    programName.replace(QLatin1Char('/'), QLatin1Char('\\'));
    if (!programName.startsWith(doubleQuote) && !programName.endsWith(doubleQuote) && programName.contains(QLatin1Char(' '))) {
      programName.prepend(doubleQuote);
      programName.append(doubleQuote);
    }
    return programName;
  };
  static const auto quoteWinArgument = [](const QString &arg) {
    if (arg.isEmpty())
      return QString::fromLatin1("\"\"");

    QString ret(arg);
    // Quotes are escaped and their preceding backslashes are doubled.
    ret.replace(QRegularExpression("(\\\\*)\""), "\\1\\1\\\"");
    if (ret.contains(QRegularExpression("\\s"))) {
      // The argument must not end with a \ since this would be interpreted
      // as escaping the quote -- rather put the \ behind the quote: e.g.
      // rather use "foo"\ than "foo\"
      int i = ret.length();
      while (i > 0 && ret.at(i - 1) == QLatin1Char('\\'))
        --i;
      ret.insert(i, QLatin1Char('"'));
      ret.prepend(QLatin1Char('"'));
    }
    return ret;
  };
  static const auto createWinCommandlineMultiArgs = [](const QString &program, const QStringList &args) {
    QString programName = quoteWinCommand(program);
    for (const QString &arg : args) {
      programName += QLatin1Char(' ');
      programName += quoteWinArgument(arg);
    }
    return programName;
  };
  static const auto createWinCommandlineSingleArg = [](const QString &program, const QString &args) {
    QString programName = quoteWinCommand(program);
    if (!args.isEmpty()) {
      programName += QLatin1Char(' ');
      programName += args;
    }
    return programName;
  };

  QStringList stubArgs;
  stubArgs << modeOption(d->m_terminalMode) << d->m_stubServer.fullServerName() << workDir << (d->m_tempFile ? d->m_tempFile->fileName() : QString()) << createWinCommandlineSingleArg(pcmd, pargs) << msgPromptToClose();

  const QString cmdLine = createWinCommandlineMultiArgs(QCoreApplication::applicationDirPath() + QLatin1String("/orca_process_stub.exe"), stubArgs);

  bool success = CreateProcessW(0, (WCHAR*)cmdLine.utf16(), 0, 0, FALSE, CREATE_NEW_CONSOLE, 0, 0, &si, d->m_pid);

  if (!success) {
    delete d->m_pid;
    d->m_pid = nullptr;
    const QString msg = tr("The process \"%1\" could not be started: %2").arg(cmdLine, winErrorMessage(GetLastError()));
    cleanupAfterStartFailure(msg);
    return;
  }

  d->processFinishedNotifier = new QWinEventNotifier(d->m_pid->hProcess, this);
  connect(d->processFinishedNotifier, &QWinEventNotifier::activated, this, &TerminalProcess::stubExited);

  #else

    ProcessArgs::SplitError perr;
    ProcessArgs pargs = ProcessArgs::prepareArgs(d->m_commandLine.arguments(),
                                                 &perr,
                                                 HostOsInfo::hostOs(),
                                                 &d->m_environment,
                                                 &d->m_workingDir,
                                                 d->m_abortOnMetaChars);

    QString pcmd;
    if (perr == ProcessArgs::SplitOk) {
        pcmd = d->m_commandLine.executable().toString();
    } else {
        if (perr != ProcessArgs::FoundMeta) {
            emitError(QProcess::FailedToStart, tr("Quoting error in command."));
            return;
        }
        if (d->m_terminalMode == QtcProcess::TerminalDebug) {
            // FIXME: ORCABUG-2809
            emitError(QProcess::FailedToStart, tr("Debugging complex shell commands in a terminal"
                                 " is currently not supported."));
            return;
        }
        pcmd = qEnvironmentVariable("SHELL", "/bin/sh");
        pargs = ProcessArgs::createUnixArgs(
                        {"-c", (ProcessArgs::quoteArg(d->m_commandLine.executable().toString())
                         + ' ' + d->m_commandLine.arguments())});
    }

    ProcessArgs::SplitError qerr;
    const TerminalCommand terminal = TerminalCommand::terminalEmulator();
    const ProcessArgs terminalArgs = ProcessArgs::prepareArgs(terminal.executeArgs,
                                                              &qerr,
                                                              HostOsInfo::hostOs(),
                                                              &d->m_environment,
                                                              &d->m_workingDir);
    if (qerr != ProcessArgs::SplitOk) {
        emitError(QProcess::FailedToStart, qerr == ProcessArgs::BadQuoting
                          ? tr("Quoting error in terminal command.")
                          : tr("Terminal command may not be a shell command."));
        return;
    }

    const QString err = stubServerListen();
    if (!err.isEmpty()) {
        emitError(QProcess::FailedToStart, msgCommChannelFailed(err));
        return;
    }

    d->m_environment.unset(QLatin1String("TERM"));

    const QStringList env = d->m_environment.toStringList();
    if (!env.isEmpty()) {
        d->m_tempFile = new QTemporaryFile();
        if (!d->m_tempFile->open()) {
            cleanupAfterStartFailure(msgCannotCreateTempFile(d->m_tempFile->errorString()));
            return;
        }
        QByteArray contents;
        for (const QString &var : env) {
            const QByteArray l8b = var.toLocal8Bit();
            contents.append(l8b.constData(), l8b.size() + 1);
        }
        if (d->m_tempFile->write(contents) != contents.size() || !d->m_tempFile->flush()) {
            cleanupAfterStartFailure(msgCannotWriteTempFile());
            return;
        }
    }

    const QString stubPath = QCoreApplication::applicationDirPath()
            + QLatin1String("/" RELATIVE_LIBEXEC_PATH "/orca_process_stub");

    QStringList allArgs = terminalArgs.toUnixArgs();

    allArgs << stubPath
            << modeOption(d->m_terminalMode)
            << d->m_stubServer.fullServerName()
            << msgPromptToClose()
            << workingDirectory().path()
            << (d->m_tempFile ? d->m_tempFile->fileName() : QString())
            << QString::number(getpid())
            << pcmd
            << pargs.toUnixArgs();

    if (terminal.needsQuotes)
        allArgs = QStringList { ProcessArgs::joinArgs(allArgs) };

    d->m_process.setEnvironment(d->m_environment);
    d->m_process.setCommand({FilePath::fromString(terminal.command), allArgs});
    d->m_process.start();
    if (!d->m_process.waitForStarted()) {
        const QString msg = tr("Cannot start the terminal emulator \"%1\", change the setting in the "
                               "Environment options.").arg(terminal.command);
        cleanupAfterStartFailure(msg);
        return;
    }
    d->m_stubConnectTimer = new QTimer(this);
    connect(d->m_stubConnectTimer, &QTimer::timeout, this, &TerminalProcess::stopProcess);
    d->m_stubConnectTimer->setSingleShot(true);
    d->m_stubConnectTimer->start(10000);

  #endif
}

auto TerminalProcess::cleanupAfterStartFailure(const QString &errorMessage) -> void
{
  stubServerShutdown();
  emitError(QProcess::FailedToStart, errorMessage);
  delete d->m_tempFile;
  d->m_tempFile = nullptr;
}

auto TerminalProcess::finish(int exitCode, QProcess::ExitStatus exitStatus) -> void
{
  d->m_processId = 0;
  d->m_exitCode = exitCode;
  d->m_appStatus = exitStatus;
  emit finished(exitCode, exitStatus);
}

auto TerminalProcess::kickoffProcess() -> void
{
  #ifdef Q_OS_WIN
  // Not used.
  #else
    if (d->m_stubSocket && d->m_stubSocket->isWritable()) {
        d->m_stubSocket->write("c", 1);
        d->m_stubSocket->flush();
    }
  #endif
}

auto TerminalProcess::interruptProcess() -> void
{
  #ifdef Q_OS_WIN
  // Not used.
  #else
    if (d->m_stubSocket && d->m_stubSocket->isWritable()) {
        d->m_stubSocket->write("i", 1);
        d->m_stubSocket->flush();
    }
  #endif
}

auto TerminalProcess::killProcess() -> void
{
  #ifdef Q_OS_WIN
  if (d->m_hInferior != NULL) {
    TerminateProcess(d->m_hInferior, (unsigned)-1);
    cleanupInferior();
  }
  #else
    if (d->m_stubSocket && d->m_stubSocket->isWritable()) {
        d->m_stubSocket->write("k", 1);
        d->m_stubSocket->flush();
    }
  #endif
  d->m_processId = 0;
}

auto TerminalProcess::killStub() -> void
{
  #ifdef Q_OS_WIN
  if (d->m_pid) {
    TerminateProcess(d->m_pid->hProcess, (unsigned)-1);
    WaitForSingleObject(d->m_pid->hProcess, INFINITE);
    cleanupStub();
  }
  #else
    if (d->m_stubSocket && d->m_stubSocket->isWritable()) {
        d->m_stubSocket->write("s", 1);
        d->m_stubSocket->flush();
    }
    stubServerShutdown();
  #endif
}

auto TerminalProcess::stopProcess() -> void
{
  killProcess();
  killStub();
  if (isRunning() && HostOsInfo::isAnyUnixHost()) {
    d->m_process.terminate();
    if (!d->m_process.waitForFinished(1000) && d->m_process.state() == QProcess::Running) {
      d->m_process.kill();
      d->m_process.waitForFinished();
    }
  }
}

auto TerminalProcess::isRunning() const -> bool
{
  #ifdef Q_OS_WIN
  return d->m_pid != nullptr;
  #else
    return d->m_process.state() != QProcess::NotRunning
            || (d->m_stubSocket && d->m_stubSocket->isOpen());
  #endif
}

auto TerminalProcess::state() const -> QProcess::ProcessState
{
  #ifdef Q_OS_WIN
  return (d->m_pid != nullptr) ? QProcess::Running : QProcess::NotRunning;
  #else
    return (d->m_stubSocket && d->m_stubSocket->isOpen())
            ? QProcess::Running : d->m_process.state();
  #endif
}

auto TerminalProcess::stubServerListen() -> QString
{
  #ifdef Q_OS_WIN
  if (d->m_stubServer.listen(QString::fromLatin1("creator-%1-%2").arg(QCoreApplication::applicationPid()).arg(rand())))
    return QString();
  return d->m_stubServer.errorString();
  #else
    // We need to put the socket in a private directory, as some systems simply do not
    // check the file permissions of sockets.
    QString stubFifoDir;
    while (true) {
        {
            QTemporaryFile tf;
            if (!tf.open())
                return msgCannotCreateTempFile(tf.errorString());
            stubFifoDir = tf.fileName();
        }
        // By now the temp file was deleted again
        d->m_stubServerDir = QFile::encodeName(stubFifoDir);
        if (!::mkdir(d->m_stubServerDir.constData(), 0700))
            break;
        if (errno != EEXIST)
            return msgCannotCreateTempDir(stubFifoDir, QString::fromLocal8Bit(strerror(errno)));
    }
    const QString stubServer  = stubFifoDir + QLatin1String("/stub-socket");
    if (!d->m_stubServer.listen(stubServer)) {
        ::rmdir(d->m_stubServerDir.constData());
        return tr("Cannot create socket \"%1\": %2").arg(stubServer, d->m_stubServer.errorString());
    }
    return QString();
  #endif
}

auto TerminalProcess::stubServerShutdown() -> void
{
  #ifdef Q_OS_WIN
  delete d->m_stubSocket;
  d->m_stubSocket = nullptr;
  if (d->m_stubServer.isListening())
    d->m_stubServer.close();
  #else
    if (d->m_stubSocket) {
        readStubOutput(); // we could get the shutdown signal before emptying the buffer
        d->m_stubSocket->disconnect(); // avoid getting queued readyRead signals
        d->m_stubSocket->deleteLater(); // we might be called from the disconnected signal of m_stubSocket
    }
    d->m_stubSocket = nullptr;
    if (d->m_stubServer.isListening()) {
        d->m_stubServer.close();
        ::rmdir(d->m_stubServerDir.constData());
    }
  #endif
}

auto TerminalProcess::stubConnectionAvailable() -> void
{
  if (d->m_stubConnectTimer) {
    delete d->m_stubConnectTimer;
    d->m_stubConnectTimer = nullptr;
  }

  d->m_stubSocket = d->m_stubServer.nextPendingConnection();
  connect(d->m_stubSocket, &QIODevice::readyRead, this, &TerminalProcess::readStubOutput);

  if (HostOsInfo::isAnyUnixHost())
    connect(d->m_stubSocket, &QLocalSocket::disconnected, this, &TerminalProcess::stubExited);
}

static auto errorMsg(int code) -> QString
{
  return QString::fromLocal8Bit(strerror(code));
}

auto TerminalProcess::readStubOutput() -> void
{
  while (d->m_stubSocket->canReadLine()) {
    QByteArray out = d->m_stubSocket->readLine();
    #ifdef Q_OS_WIN
    out.chop(2); // \r\n
    if (out.startsWith("err:chdir ")) {
      emitError(QProcess::FailedToStart, msgCannotChangeToWorkDir(workingDirectory(), winErrorMessage(out.mid(10).toInt())));
    } else if (out.startsWith("err:exec ")) {
      emitError(QProcess::FailedToStart, msgCannotExecute(d->m_commandLine.executable().toUserOutput(), winErrorMessage(out.mid(9).toInt())));
    } else if (out.startsWith("thread ")) {
      // Windows only
      d->m_appMainThreadId = out.mid(7).toLongLong();
    } else if (out.startsWith("pid ")) {
      // Will not need it any more
      delete d->m_tempFile;
      d->m_tempFile = nullptr;
      d->m_processId = out.mid(4).toLongLong();

      d->m_hInferior = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, d->m_processId);
      if (d->m_hInferior == NULL) {
        emitError(QProcess::FailedToStart, tr("Cannot obtain a handle to the inferior: %1").arg(winErrorMessage(GetLastError())));
        // Uhm, and now what?
        continue;
      }
      d->inferiorFinishedNotifier = new QWinEventNotifier(d->m_hInferior, this);
      connect(d->inferiorFinishedNotifier, &QWinEventNotifier::activated, this, [this] {
        DWORD chldStatus;

        if (!GetExitCodeProcess(d->m_hInferior, &chldStatus))
          emitError(QProcess::UnknownError, tr("Cannot obtain exit status from inferior: %1").arg(winErrorMessage(GetLastError())));
        cleanupInferior();
        finish(chldStatus, QProcess::NormalExit);
      });

      emit started();
    } else {
      emitError(QProcess::UnknownError, msgUnexpectedOutput(out));
      TerminateProcess(d->m_pid->hProcess, (unsigned)-1);
      break;
    }
    #else
        out.chop(1); // \n
        if (out.startsWith("err:chdir ")) {
            emitError(QProcess::FailedToStart, msgCannotChangeToWorkDir(workingDirectory(), errorMsg(out.mid(10).toInt())));
        } else if (out.startsWith("err:exec ")) {
            emitError(QProcess::FailedToStart, msgCannotExecute(d->m_commandLine.executable().toString(), errorMsg(out.mid(9).toInt())));
        } else if (out.startsWith("spid ")) {
            delete d->m_tempFile;
            d->m_tempFile = nullptr;
        } else if (out.startsWith("pid ")) {
            d->m_processId = out.mid(4).toInt();
            emit started();
        } else if (out.startsWith("exit ")) {
            finish(out.mid(5).toInt(), QProcess::NormalExit);
        } else if (out.startsWith("crash ")) {
            finish(out.mid(6).toInt(), QProcess::CrashExit);
        } else {
            emitError(QProcess::UnknownError, msgUnexpectedOutput(out));
            d->m_process.terminate();
            break;
        }
    #endif
  } // while
}

auto TerminalProcess::stubExited() -> void
{
  // The stub exit might get noticed before we read the pid for the kill on Windows
  // or the error status elsewhere.
  if (d->m_stubSocket && d->m_stubSocket->state() == QLocalSocket::ConnectedState)
    d->m_stubSocket->waitForDisconnected();

  #ifdef Q_OS_WIN
  cleanupStub();
  if (d->m_hInferior != NULL) {
    TerminateProcess(d->m_hInferior, (unsigned)-1);
    cleanupInferior();
    finish(-1, QProcess::CrashExit);
  }
  #else
    stubServerShutdown();
    delete d->m_tempFile;
    d->m_tempFile = nullptr;
    if (d->m_processId)
        finish(-1, QProcess::CrashExit);
  #endif
}

auto TerminalProcess::cleanupInferior() -> void
{
  #ifdef Q_OS_WIN
  delete d->inferiorFinishedNotifier;
  d->inferiorFinishedNotifier = nullptr;
  CloseHandle(d->m_hInferior);
  d->m_hInferior = NULL;
  #endif
}

auto TerminalProcess::cleanupStub() -> void
{
  #ifdef Q_OS_WIN
  stubServerShutdown();
  delete d->processFinishedNotifier;
  d->processFinishedNotifier = nullptr;
  CloseHandle(d->m_pid->hThread);
  CloseHandle(d->m_pid->hProcess);
  delete d->m_pid;
  d->m_pid = nullptr;
  delete d->m_tempFile;
  d->m_tempFile = nullptr;
  #endif
}

auto TerminalProcess::processId() const -> qint64
{
  return d->m_processId;
}

auto TerminalProcess::exitCode() const -> int
{
  return d->m_exitCode;
} // This will be the signal number if exitStatus == CrashExit

auto TerminalProcess::exitStatus() const -> QProcess::ExitStatus
{
  return d->m_appStatus;
}

auto TerminalProcess::setWorkingDirectory(const FilePath &dir) -> void
{
  d->m_workingDir = dir;
}

auto TerminalProcess::workingDirectory() const -> FilePath
{
  return d->m_workingDir;
}

auto TerminalProcess::setEnvironment(const Environment &env) -> void
{
  d->m_environment = env;
}

auto TerminalProcess::environment() const -> const Environment&
{
  return d->m_environment;
}

auto TerminalProcess::error() const -> QProcess::ProcessError
{
  return d->m_error;
}

auto TerminalProcess::errorString() const -> QString
{
  return d->m_errorString;
}

auto TerminalProcess::emitError(QProcess::ProcessError err, const QString &errorString) -> void
{
  d->m_error = err;
  d->m_errorString = errorString;
  emit errorOccurred(err);
}

} // Internal
} // Utils
