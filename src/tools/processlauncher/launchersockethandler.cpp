// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "launchersockethandler.hpp"

#include "launcherlogging.hpp"
#include "processreaper.hpp"
#include "processutils.hpp"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QProcess>

namespace Utils {
namespace Internal {

class Process : public ProcessHelper
{
    Q_OBJECT
public:
    Process(quintptr token, QObject *parent = nullptr) :
        ProcessHelper(parent), m_token(token) { }

    auto token() const -> quintptr { return m_token; }

private:
    const quintptr m_token;
};

LauncherSocketHandler::LauncherSocketHandler(QString serverPath, QObject *parent)
    : QObject(parent),
      m_server_path(std::move(serverPath)),
      m_socket(new QLocalSocket(this))
{
    m_packet_parser.setDevice(m_socket);
}

LauncherSocketHandler::~LauncherSocketHandler()
{
    m_socket->disconnect();
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        logWarn("socket handler destroyed while connection was active");
        m_socket->close();
    }
    for (auto it = m_processes.cbegin(); it != m_processes.cend(); ++it)
        ProcessReaper::reap(it.value());
}

auto LauncherSocketHandler::start() -> void
{
    connect(m_socket, &QLocalSocket::disconnected,
            this, &LauncherSocketHandler::handleSocketClosed);
    connect(m_socket, &QLocalSocket::readyRead, this, &LauncherSocketHandler::handleSocketData);
    connect(m_socket,
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
            static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
#else
            &QLocalSocket::errorOccurred,
#endif
            this, &LauncherSocketHandler::handleSocketError);
    m_socket->connectToServer(m_server_path);
}

auto LauncherSocketHandler::handleSocketData() -> void
{
    try {
        if (!m_packet_parser.parse())
            return;
    } catch (const PacketParser::InvalidPacketSizeException &e) {
        logWarn(QStringLiteral("Internal protocol error: invalid packet size %1.")
                .arg(e.size));
        return;
    }
    switch (m_packet_parser.type()) {
    case LauncherPacketType::StartProcess:
        handleStartPacket();
        break;
    case LauncherPacketType::WriteIntoProcess:
        handleWritePacket();
        break;
    case LauncherPacketType::StopProcess:
        handleStopPacket();
        break;
    case LauncherPacketType::Shutdown:
        handleShutdownPacket();
        return;
    default:
        logWarn(QStringLiteral("Internal protocol error: invalid packet type %1.")
                .arg(static_cast<int>(m_packet_parser.type())));
        return;
    }
    handleSocketData();
}

auto LauncherSocketHandler::handleSocketError() -> void
{
    if (m_socket->error() != QLocalSocket::PeerClosedError) {
        logError(QStringLiteral("socket error: %1").arg(m_socket->errorString()));
        m_socket->disconnect();
        qApp->quit();
    }
}

auto LauncherSocketHandler::handleSocketClosed() -> void
{
    for (auto it = m_processes.cbegin(); it != m_processes.cend(); ++it) {
        if (it.value()->state() != QProcess::NotRunning) {
            logWarn("client closed connection while process still running");
            break;
        }
    }
    m_socket->disconnect();
    qApp->quit();
}

auto LauncherSocketHandler::handleProcessError() -> void
{
    Process * proc = senderProcess();
    ProcessErrorPacket packet(proc->token());
    packet.error = proc->error();
    packet.errorString = proc->errorString();
    sendPacket(packet);

    // In case of FailedToStart we won't receive finished signal, so we remove the process here.
    // For all other errors we should expect corresponding finished signal to appear.
    if (proc->error() == QProcess::FailedToStart)
        removeProcess(proc->token());
}

auto LauncherSocketHandler::handleProcessStarted() -> void
{
    Process *proc = senderProcess();
    ProcessStartedPacket packet(proc->token());
    packet.processId = proc->processId();
    proc->processStartHandler()->handleProcessStarted();
    sendPacket(packet);
}

auto LauncherSocketHandler::handleReadyReadStandardOutput() -> void
{
    Process * proc = senderProcess();
    ReadyReadStandardOutputPacket packet(proc->token());
    packet.standardChannel = proc->readAllStandardOutput();
    sendPacket(packet);
}

auto LauncherSocketHandler::handleReadyReadStandardError() -> void
{
    Process * proc = senderProcess();
    ReadyReadStandardErrorPacket packet(proc->token());
    packet.standardChannel = proc->readAllStandardError();
    sendPacket(packet);
}

auto LauncherSocketHandler::handleProcessFinished() -> void
{
    Process * proc = senderProcess();
    ProcessFinishedPacket packet(proc->token());
    packet.error = proc->error();
    packet.errorString = proc->errorString();
    packet.exitCode = proc->exitCode();
    packet.exitStatus = proc->exitStatus();
    if (proc->processChannelMode() != QProcess::MergedChannels)
        packet.stdErr = proc->readAllStandardError();
    packet.stdOut = proc->readAllStandardOutput();
    sendPacket(packet);
    removeProcess(proc->token());
}

auto LauncherSocketHandler::handleStartPacket() -> void
{
    Process *& process = m_processes[m_packet_parser.token()];
    if (!process)
        process = setupProcess(m_packet_parser.token());
    if (process->state() != QProcess::NotRunning) {
        logWarn("got start request while process was running");
        return;
    }
    const auto packet = LauncherPacket::extractPacket<StartProcessPacket>(
                m_packet_parser.token(),
                m_packet_parser.packetData());
    process->setEnvironment(packet.env);
    process->setWorkingDirectory(packet.workingDir);
    // Forwarding is handled by the LauncherInterface
    process->setProcessChannelMode(packet.channelMode == QProcess::MergedChannels ?
                                       QProcess::MergedChannels : QProcess::SeparateChannels);
    process->setStandardInputFile(packet.standardInputFile);
    ProcessStartHandler *handler = process->processStartHandler();
    handler->setProcessMode(packet.processMode);
    handler->setWriteData(packet.writeData);
    if (packet.belowNormalPriority)
        handler->setBelowNormalPriority();
    handler->setNativeArguments(packet.nativeArguments);
    if (packet.lowPriority)
        process->setLowPriority();
    if (packet.unixTerminalDisabled)
        process->setUnixTerminalDisabled();
    process->start(packet.command, packet.arguments, handler->openMode());
    handler->handleProcessStart();
}

auto LauncherSocketHandler::handleWritePacket() -> void
{
    Process * const process = m_processes.value(m_packet_parser.token());
    if (!process) {
        logWarn("got write request for unknown process");
        return;
    }
    if (process->state() != QProcess::Running) {
        logDebug("can't write into not running process");
        return;
    }
    const auto packet = LauncherPacket::extractPacket<WritePacket>(
                m_packet_parser.token(),
                m_packet_parser.packetData());
    process->write(packet.inputData);
}

auto LauncherSocketHandler::handleStopPacket() -> void
{
    Process * const process = m_processes.value(m_packet_parser.token());
    if (!process) {
        // This can happen when the process finishes on its own at about the same time the client
        // sends the request. In this case the process was already deleted.
        logDebug("got stop request for unknown process");
        return;
    }
    if (process->state() == QProcess::NotRunning) {
        // This shouldn't happen, since as soon as process finishes or error occurrs
        // the process is being removed.
        logWarn("got stop request when process was not running");
    } else {
        // We got the client request to stop the starting / running process.
        // We report process exit to the client.
        ProcessFinishedPacket packet(process->token());
        packet.error = QProcess::Crashed;
        packet.exitCode = -1;
        packet.exitStatus = QProcess::CrashExit;
        if (process->processChannelMode() != QProcess::MergedChannels)
            packet.stdErr = process->readAllStandardError();
        packet.stdOut = process->readAllStandardOutput();
        sendPacket(packet);
    }
    removeProcess(process->token());
}

auto LauncherSocketHandler::handleShutdownPacket() -> void
{
    logDebug("got shutdown request, closing down");
    for (auto it = m_processes.cbegin(); it != m_processes.cend(); ++it) {
        it.value()->disconnect();
        if (it.value()->state() != QProcess::NotRunning) {
            logWarn("got shutdown request while process was running");
            it.value()->terminate();
        }
    }
    m_socket->disconnect();
    qApp->quit();
}

auto LauncherSocketHandler::sendPacket(const LauncherPacket &packet) -> void
{
    m_socket->write(packet.serialize());
}

auto LauncherSocketHandler::setupProcess(quintptr token) -> Process*
{
    const auto p = new Process(token, this);
    connect(p, &QProcess::errorOccurred, this, &LauncherSocketHandler::handleProcessError);
    connect(p, &QProcess::started, this, &LauncherSocketHandler::handleProcessStarted);
    connect(p, &QProcess::readyReadStandardOutput,
            this, &LauncherSocketHandler::handleReadyReadStandardOutput);
    connect(p, &QProcess::readyReadStandardError,
            this, &LauncherSocketHandler::handleReadyReadStandardError);
    connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &LauncherSocketHandler::handleProcessFinished);
    return p;
}

auto LauncherSocketHandler::removeProcess(quintptr token) -> void
{
    const auto it = m_processes.find(token);
    if (it == m_processes.end())
        return;

    Process *process = it.value();
    m_processes.erase(it);
    ProcessReaper::reap(process);
}

auto LauncherSocketHandler::senderProcess() const -> Process*
{
    return static_cast<Process *>(sender());
}

} // namespace Internal
} // namespace Utils

#include <launchersockethandler.moc>
