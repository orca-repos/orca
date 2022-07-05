// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <qtlockedfile.h>

#include <QLocalServer>
#include <QLocalSocket>
#include <QDir>

namespace SharedTools {

class QtLocalPeer : public QObject
{
    Q_OBJECT

public:
    explicit QtLocalPeer(QObject *parent = 0, const QString &appId = QString());
    auto isClient() -> bool;
    auto sendMessage(const QString &message, int timeout, bool block) -> bool;
    auto applicationId() const -> QString
    { return id; }
    static auto appSessionId(const QString &appId) -> QString;

Q_SIGNALS:
    auto messageReceived(const QString &message, QObject *socket) -> void;

protected:
    auto receiveConnection() -> void;

    QString id;
    QString socketName;
    QLocalServer* server;
    QtLockedFile lockFile;
};

} // namespace SharedTools
