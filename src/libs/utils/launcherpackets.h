// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "processutils.h"

#include <QDataStream>
#include <QProcess>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QByteArray;
QT_END_NAMESPACE

namespace Utils {
namespace Internal {

enum class LauncherPacketType {
    // client -> launcher packets:
    Shutdown,
    StartProcess,
    WriteIntoProcess,
    StopProcess,
    // launcher -> client packets:
    ProcessError,
    ProcessStarted,
    ReadyReadStandardOutput,
    ReadyReadStandardError,
    ProcessFinished
};

class PacketParser
{
public:
    class InvalidPacketSizeException
    {
    public:
        InvalidPacketSizeException(int size) : size(size) { }
        const int size;
    };

    auto setDevice(QIODevice *device) -> void;
    auto parse() -> bool;
    auto type() const -> LauncherPacketType { return m_type; }
    auto token() const -> quintptr { return m_token; }
    auto packetData() const -> const QByteArray& { return m_packetData; }

private:
    QDataStream m_stream;
    LauncherPacketType m_type = LauncherPacketType::Shutdown;
    quintptr m_token = 0;
    QByteArray m_packetData;
    int m_sizeOfNextPacket = -1;
};

class LauncherPacket
{
public:
    virtual ~LauncherPacket();

    template<class Packet> static auto extractPacket(quintptr token, const QByteArray &data) -> Packet
    {
        Packet p(token);
        p.deserialize(data);
        return p;
    }

    auto serialize() const -> QByteArray;
    auto deserialize(const QByteArray &data) -> void;

    const LauncherPacketType type;
    const quintptr token = 0;

protected:
    LauncherPacket(LauncherPacketType type, quintptr token) : type(type), token(token) { }

private:
    virtual auto doSerialize(QDataStream &stream) const -> void = 0;
    virtual auto doDeserialize(QDataStream &stream) -> void = 0;
};

class StartProcessPacket : public LauncherPacket
{
public:
    StartProcessPacket(quintptr token);

    QString command;
    QStringList arguments;
    QString workingDir;
    QStringList env;
    ProcessMode processMode = ProcessMode::Reader;
    QByteArray writeData;
    QProcess::ProcessChannelMode channelMode = QProcess::SeparateChannels;
    QString standardInputFile;
    bool belowNormalPriority = false;
    QString nativeArguments;
    bool lowPriority = false;
    bool unixTerminalDisabled = false;

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class ProcessStartedPacket : public LauncherPacket
{
public:
    ProcessStartedPacket(quintptr token);

    int processId = 0;

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class StopProcessPacket : public LauncherPacket
{
public:
    StopProcessPacket(quintptr token);

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class WritePacket : public LauncherPacket
{
public:
    WritePacket(quintptr token) : LauncherPacket(LauncherPacketType::WriteIntoProcess, token) { }

    QByteArray inputData;

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class ShutdownPacket : public LauncherPacket
{
public:
    ShutdownPacket();

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class ProcessErrorPacket : public LauncherPacket
{
public:
    ProcessErrorPacket(quintptr token);

    QProcess::ProcessError error = QProcess::UnknownError;
    QString errorString;

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class ReadyReadPacket : public LauncherPacket
{
public:
    QByteArray standardChannel;

protected:
    ReadyReadPacket(LauncherPacketType type, quintptr token) : LauncherPacket(type, token) { }

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

class ReadyReadStandardOutputPacket : public ReadyReadPacket
{
public:
    ReadyReadStandardOutputPacket(quintptr token)
        : ReadyReadPacket(LauncherPacketType::ReadyReadStandardOutput, token) { }
};

class ReadyReadStandardErrorPacket : public ReadyReadPacket
{
public:
    ReadyReadStandardErrorPacket(quintptr token)
        : ReadyReadPacket(LauncherPacketType::ReadyReadStandardError, token) { }
};

class ProcessFinishedPacket : public LauncherPacket
{
public:
    ProcessFinishedPacket(quintptr token);

    QString errorString;
    QByteArray stdOut;
    QByteArray stdErr;
    QProcess::ExitStatus exitStatus = QProcess::ExitStatus::NormalExit;
    QProcess::ProcessError error = QProcess::ProcessError::UnknownError;
    int exitCode = 0;

private:
    auto doSerialize(QDataStream &stream) const -> void override;
    auto doDeserialize(QDataStream &stream) -> void override;
};

} // namespace Internal
} // namespace Utils

Q_DECLARE_METATYPE(Utils::Internal::LauncherPacketType);
