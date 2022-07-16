// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"
#include "languageclientutils.hpp"

#include "utils/optional.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/lsptypes.h>

#include <QMap>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT DocumentSymbolCache : public QObject {
  Q_OBJECT

public:
  DocumentSymbolCache(Client *client);

  auto requestSymbols(const LanguageServerProtocol::DocumentUri &uri, Schedule schedule) -> void;

signals:
  auto gotSymbols(const LanguageServerProtocol::DocumentUri &uri, const LanguageServerProtocol::DocumentSymbolsResult &symbols) -> void;

private:
  auto requestSymbolsImpl() -> void;
  auto handleResponse(const LanguageServerProtocol::DocumentUri &uri, const LanguageServerProtocol::DocumentSymbolsRequest::Response &response) -> void;

  QMap<LanguageServerProtocol::DocumentUri, LanguageServerProtocol::DocumentSymbolsResult> m_cache;
  Client *m_client = nullptr;
  QTimer m_compressionTimer;
  QSet<LanguageServerProtocol::DocumentUri> m_compressedUris;
};

} // namespace LanguageClient
