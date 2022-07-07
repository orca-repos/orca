// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "networkaccessmanager.hpp"

#include <QCoreApplication>
#include <QLocale>
#include <QNetworkReply>

#ifdef Q_OS_UNIX
#include <sys/utsname.hpp>
#endif

/*!
   \class Utils::NetworkAccessManager
   \inmodule Orca

    \brief The NetworkAccessManager class provides a network access manager for use
    with \QC.

   Common initialization, \QC User Agent.

   Preferably, the instance returned by NetworkAccessManager::instance() should be used for the main
   thread. The constructor is provided only for multithreaded use.
 */

namespace Utils {

static NetworkAccessManager *namInstance = nullptr;

auto cleanupNetworkAccessManager() -> void
{
  delete namInstance;
  namInstance = nullptr;
}

/*!
    Returns a network access manager instance that should be used for the main
    thread.
*/
auto NetworkAccessManager::instance() -> NetworkAccessManager*
{
  if (!namInstance) {
    namInstance = new NetworkAccessManager;
    qAddPostRoutine(cleanupNetworkAccessManager);
  }
  return namInstance;
}

/*!
    Constructs a network access manager instance with the parent \a parent.
*/
NetworkAccessManager::NetworkAccessManager(QObject *parent) : QNetworkAccessManager(parent) {}

/*!
    Creates \a request for the network access manager to perform the operation
    \a op on \a outgoingData.
*/
auto NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply*
{
  QString agentStr = QString::fromLatin1("%1/%2 (QNetworkAccessManager %3; %4; %5; %6 bit)").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion(), QLatin1String(qVersion()), QSysInfo::prettyProductName(), QLocale::system().name()).arg(QSysInfo::WordSize);
  QNetworkRequest req(request);
  req.setRawHeader("User-Agent", agentStr.toLatin1());
  return QNetworkAccessManager::createRequest(op, req, outgoingData);
}


} // namespace utils
