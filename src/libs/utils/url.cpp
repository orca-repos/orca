// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "url.hpp"
#include "temporaryfile.hpp"

#include <QHostAddress>
#include <QTcpServer>

namespace Utils {

auto urlFromLocalHostAndFreePort() -> QUrl
{
  QUrl serverUrl;
  QTcpServer server;
  serverUrl.setScheme(urlTcpScheme());
  if (server.listen(QHostAddress::LocalHost) || server.listen(QHostAddress::LocalHostIPv6)) {
    serverUrl.setHost(server.serverAddress().toString());
    serverUrl.setPort(server.serverPort());
  }
  return serverUrl;
}

auto urlFromLocalSocket() -> QUrl
{
  QUrl serverUrl;
  serverUrl.setScheme(urlSocketScheme());
  TemporaryFile file("qtc-socket");
  // see "man unix" for unix socket file name size limitations
  if (file.fileName().size() > 104) {
    qWarning().nospace() << "Socket file name \"" << file.fileName() << "\" is larger than 104 characters, which will not work on Darwin/macOS/Linux!";
  }
  if (file.open())
    serverUrl.setPath(file.fileName());
  return serverUrl;
}

auto urlSocketScheme() -> QString
{
  return QString("socket");
}

auto urlTcpScheme() -> QString
{
  return QString("tcp");
}

}
