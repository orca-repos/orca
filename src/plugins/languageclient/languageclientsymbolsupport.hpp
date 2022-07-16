// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <core/core-search-result-item.hpp>
#include <texteditor/textdocument.hpp>

#include <languageserverprotocol/languagefeatures.h>

namespace Orca::Plugin::Core {
class SearchResult;
class SearchResultItem;
}

namespace LanguageServerProtocol {
class MessageId;
}

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT SymbolSupport {
  Q_DECLARE_TR_FUNCTIONS(SymbolSupport)

public:
  using ResultHandler = std::function<void(const QList<LanguageServerProtocol::Location> &)>;

  explicit SymbolSupport(Client *client);
  auto findLinkAt(TextEditor::TextDocument *document, const QTextCursor &cursor, Utils::ProcessLinkCallback callback, const bool resolveTarget) -> void;
  auto findUsages(TextEditor::TextDocument *document, const QTextCursor &cursor, const ResultHandler &handler = {}) -> Utils::optional<LanguageServerProtocol::MessageId>;
  auto supportsRename(TextEditor::TextDocument *document) -> bool;
  auto renameSymbol(TextEditor::TextDocument *document, const QTextCursor &cursor) -> void;
  static auto convertRange(const LanguageServerProtocol::Range &range) -> Orca::Plugin::Core::TextRange;
  static auto getFileContents(const Utils::FilePath &filePath) -> QStringList;

private:
  auto handleFindReferencesResponse(const LanguageServerProtocol::FindReferencesRequest::Response &response, const QString &wordUnderCursor, const ResultHandler &handler) -> void;
  auto requestPrepareRename(const LanguageServerProtocol::TextDocumentPositionParams &params, const QString &placeholder) -> void;
  auto requestRename(const LanguageServerProtocol::TextDocumentPositionParams &positionParams, const QString &newName, Orca::Plugin::Core::SearchResult *search) -> void;
  auto startRenameSymbol(const LanguageServerProtocol::TextDocumentPositionParams &params, const QString &placeholder) -> void;
  auto handleRenameResponse(Orca::Plugin::Core::SearchResult *search, const LanguageServerProtocol::RenameRequest::Response &response) -> void;
  auto applyRename(const QList<Orca::Plugin::Core::SearchResultItem> &checkedItems) -> void;

  Client *m_client = nullptr;
};

} // namespace LanguageClient
