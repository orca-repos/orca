// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "documentsymbolcache.hpp"

#include "client.hpp"

#include <core/core-editor-manager.hpp>
#include <texteditor/textdocument.hpp>

using namespace LanguageServerProtocol;

namespace LanguageClient {

DocumentSymbolCache::DocumentSymbolCache(Client *client) : QObject(client), m_client(client)
{
  auto connectDocument = [this](Orca::Plugin::Core::IDocument *document) {
    connect(document, &Orca::Plugin::Core::IDocument::contentsChanged, this, [document, this]() {
      m_cache.remove(DocumentUri::fromFilePath(document->filePath()));
    });
  };

  for (const auto document : Orca::Plugin::Core::DocumentModel::openedDocuments())
    connectDocument(document);
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::documentOpened, this, connectDocument);
  m_compressionTimer.setSingleShot(true);
  connect(&m_compressionTimer, &QTimer::timeout, this, &DocumentSymbolCache::requestSymbolsImpl);
}

auto DocumentSymbolCache::requestSymbols(const DocumentUri &uri, Schedule schedule) -> void
{
  m_compressedUris.insert(uri);
  switch (schedule) {
  case Schedule::Now:
    requestSymbolsImpl();
    break;
  case Schedule::Delayed:
    m_compressionTimer.start(200);
    break;
  }
}

auto DocumentSymbolCache::requestSymbolsImpl() -> void
{
  if (!m_client->reachable()) {
    m_compressionTimer.start(200);
    return;
  }
  for (const auto &uri : qAsConst(m_compressedUris)) {
    auto entry = m_cache.find(uri);
    if (entry != m_cache.end()) {
      emit gotSymbols(uri, entry.value());
      continue;
    }

    const DocumentSymbolParams params((TextDocumentIdentifier(uri)));
    DocumentSymbolsRequest request(params);
    request.setResponseCallback([uri, self = QPointer<DocumentSymbolCache>(this)](const DocumentSymbolsRequest::Response &response) {
      if (self)
        self->handleResponse(uri, response);
    });
    m_client->sendContent(request);
  }
  m_compressedUris.clear();
}

auto DocumentSymbolCache::handleResponse(const DocumentUri &uri, const DocumentSymbolsRequest::Response &response) -> void
{
  if (const auto error = response.error()) {
    if (m_client)
      m_client->log(error.value());
  }
  const auto &symbols = response.result().value_or(DocumentSymbolsResult());
  m_cache[uri] = symbols;
  emit gotSymbols(uri, symbols);
}

} // namespace LanguageClient
