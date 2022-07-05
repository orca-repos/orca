// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "launcherpackets.h"

#include <QByteArray>
#include <QCoreApplication>

namespace Utils {
namespace Internal {

LauncherPacket::~LauncherPacket() = default;

auto LauncherPacket::serialize() const -> QByteArray
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << static_cast<int>(0) << static_cast<quint8>(type) << token;
    doSerialize(stream);
    stream.device()->reset();
    stream << static_cast<int>(data.size() - sizeof(int));
    return data;
}

auto LauncherPacket::deserialize(const QByteArray &data) -> void
{
    QDataStream stream(data);
    doDeserialize(stream);
}


StartProcessPacket::StartProcessPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::StartProcess, token)
{
}

auto StartProcessPacket::doSerialize(QDataStream &stream) const -> void
{
    stream << command << arguments << workingDir << env << int(processMode) << writeData << int(channelMode)
           << standardInputFile << belowNormalPriority << nativeArguments << lowPriority
           << unixTerminalDisabled;
}

auto StartProcessPacket::doDeserialize(QDataStream &stream) -> void
{
    int cm, pm;
    stream >> command >> arguments >> workingDir >> env >> pm >> writeData >> cm
           >> standardInputFile >> belowNormalPriority >> nativeArguments >> lowPriority
           >> unixTerminalDisabled;
    channelMode = QProcess::ProcessChannelMode(cm);
    processMode = Utils::ProcessMode(pm);
}


ProcessStartedPacket::ProcessStartedPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::ProcessStarted, token)
{
}

auto ProcessStartedPacket::doSerialize(QDataStream &stream) const -> void
{
    stream << processId;
}

auto ProcessStartedPacket::doDeserialize(QDataStream &stream) -> void
{
    stream >> processId;
}


StopProcessPacket::StopProcessPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::StopProcess, token)
{
}

auto StopProcessPacket::doSerialize(QDataStream &stream) const -> void
{
    Q_UNUSED(stream);
}

auto StopProcessPacket::doDeserialize(QDataStream &stream) -> void
{
    Q_UNUSED(stream);
}

auto WritePacket::doSerialize(QDataStream &stream) const -> void
{
    stream << inputData;
}

auto WritePacket::doDeserialize(QDataStream &stream) -> void
{
    stream >> inputData;
}

ProcessErrorPacket::ProcessErrorPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::ProcessError, token)
{
}

auto ProcessErrorPacket::doSerialize(QDataStream &stream) const -> void
{
    stream << static_cast<quint8>(error) << errorString;
}

auto ProcessErrorPacket::doDeserialize(QDataStream &stream) -> void
{
    quint8 e;
    stream >> e;
    error = static_cast<QProcess::ProcessError>(e);
    stream >> errorString;
}


auto ReadyReadPacket::doSerialize(QDataStream &stream) const -> void
{
    stream << standardChannel;
}

auto ReadyReadPacket::doDeserialize(QDataStream &stream) -> void
{
    stream >> standardChannel;
}


ProcessFinishedPacket::ProcessFinishedPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::ProcessFinished, token)
{
}

auto ProcessFinishedPacket::doSerialize(QDataStream &stream) const -> void
{
    stream << errorString << stdOut << stdErr
           << static_cast<quint8>(exitStatus) << static_cast<quint8>(error)
           << exitCode;
}

auto ProcessFinishedPacket::doDeserialize(QDataStream &stream) -> void
{
    stream >> errorString >> stdOut >> stdErr;
    quint8 val;
    stream >> val;
    exitStatus = static_cast<QProcess::ExitStatus>(val);
    stream >> val;
    error = static_cast<QProcess::ProcessError>(val);
    stream >> exitCode;
}

ShutdownPacket::ShutdownPacket() : LauncherPacket(LauncherPacketType::Shutdown, 0) { }
auto ShutdownPacket::doSerialize(QDataStream &stream) const -> void
{ Q_UNUSED(stream); }
auto ShutdownPacket::doDeserialize(QDataStream &stream) -> void
{ Q_UNUSED(stream); }

auto PacketParser::setDevice(QIODevice *device) -> void
{
    m_stream.setDevice(device);
    m_sizeOfNextPacket = -1;
}

auto PacketParser::parse() -> bool
{
    static const int commonPayloadSize = static_cast<int>(1 + sizeof(quintptr));
    if (m_sizeOfNextPacket == -1) {
        if (m_stream.device()->bytesAvailable() < static_cast<int>(sizeof m_sizeOfNextPacket))
            return false;
        m_stream >> m_sizeOfNextPacket;
        if (m_sizeOfNextPacket < commonPayloadSize)
            throw InvalidPacketSizeException(m_sizeOfNextPacket);
    }
    if (m_stream.device()->bytesAvailable() < m_sizeOfNextPacket)
        return false;
    quint8 type;
    m_stream >> type;
    m_type = static_cast<LauncherPacketType>(type);
    m_stream >> m_token;
    m_packetData = m_stream.device()->read(m_sizeOfNextPacket - commonPayloadSize);
    m_sizeOfNextPacket = -1;
    return true;
}

} // namespace Internal
} // namespace Utils
