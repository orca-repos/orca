// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <launcherpackets.hpp>

#include <QByteArray>
#include <QHash>
#include <QObject>

QT_BEGIN_NAMESPACE
class QLocalSocket;
QT_END_NAMESPACE

namespace Utils {
namespace Internal {
class Process;

class LauncherSocketHandler : public QObject
{
    Q_OBJECT
public:
    explicit LauncherSocketHandler(QString socket_path, QObject *parent = nullptr);
    ~LauncherSocketHandler() override;

    auto start() -> void;

private:
    auto handleSocketData() -> void;
    auto handleSocketError() -> void;
    auto handleSocketClosed() -> void;
    auto handleProcessError() -> void;
    auto handleProcessStarted() -> void;
    auto handleReadyReadStandardOutput() -> void;
    auto handleReadyReadStandardError() -> void;
    auto handleProcessFinished() -> void;
    auto handleStartPacket() -> void;
    auto handleWritePacket() -> void;
    auto handleStopPacket() -> void;
    auto handleShutdownPacket() -> void;
    auto sendPacket(const LauncherPacket &packet) -> void;
    auto setupProcess(quintptr token) -> Process*;
    auto removeProcess(quintptr token) -> void;
    auto senderProcess() const -> Process*;

    const QString m_server_path;
    QLocalSocket * const m_socket;
    PacketParser m_packet_parser;
    QHash<quintptr, Process *> m_processes;
};

} // namespace Internal
} // namespace Utils
