// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientsymbolsupport.hpp"

#include "client.hpp"
#include "languageclientutils.hpp"

#include <core/core-editor-manager.hpp>
#include <core/core-search-result-window.hpp>

#include <utils/mimetypes/mimedatabase.hpp>

#include <QFile>

using namespace LanguageServerProtocol;

namespace LanguageClient {

SymbolSupport::SymbolSupport(Client *client) : m_client(client) {}

template <typename Request>
static auto sendTextDocumentPositionParamsRequest(Client *client, const Request &request, const DynamicCapabilities &dynamicCapabilities, const ServerCapabilities &serverCapability) -> void
{
  if (!request.isValid(nullptr))
    return;
  const DocumentUri uri = request.params().value().textDocument().uri();
  const auto supportedFile = client->isSupportedUri(uri);
  bool sendMessage = dynamicCapabilities.isRegistered(Request::methodName).value_or(false);
  if (sendMessage) {
    const TextDocumentRegistrationOptions option(dynamicCapabilities.option(Request::methodName));
    if (option.isValid())
      sendMessage = option.filterApplies(Utils::FilePath::fromString(QUrl(uri).adjusted(QUrl::PreferLocalFile).toString()));
    else
      sendMessage = supportedFile;
  } else {
    const auto &provider = serverCapability.referencesProvider();
    sendMessage = provider.has_value();
    if (sendMessage && Utils::holds_alternative<bool>(*provider))
      sendMessage = Utils::get<bool>(*provider);
  }
  if (sendMessage)
    client->sendContent(request);
}

static auto handleGotoDefinitionResponse(const GotoDefinitionRequest::Response &response, Utils::ProcessLinkCallback callback, Utils::optional<Utils::Link> linkUnderCursor) -> void
{
  if (const auto _result = response.result()) {
    const auto result = _result.value();
    if (Utils::holds_alternative<std::nullptr_t>(result)) {
      callback({});
    } else if (const auto ploc = Utils::get_if<Location>(&result)) {
      callback(linkUnderCursor.value_or(ploc->toLink()));
    } else if (const auto plloc = Utils::get_if<QList<Location>>(&result)) {
      if (!plloc->isEmpty())
        callback(linkUnderCursor.value_or(plloc->value(0).toLink()));
      else
        callback({});
    }
  } else {
    callback({});
  }
}

static auto generateDocPosParams(TextEditor::TextDocument *document, const QTextCursor &cursor) -> TextDocumentPositionParams
{
  const auto uri = DocumentUri::fromFilePath(document->filePath());
  const TextDocumentIdentifier documentId(uri);
  const Position pos(cursor);
  return TextDocumentPositionParams(documentId, pos);
}

auto SymbolSupport::findLinkAt(TextEditor::TextDocument *document, const QTextCursor &cursor, Utils::ProcessLinkCallback callback, const bool resolveTarget) -> void
{
  if (!m_client->reachable())
    return;
  GotoDefinitionRequest request(generateDocPosParams(document, cursor));
  Utils::optional<Utils::Link> linkUnderCursor;
  if (!resolveTarget) {
    auto linkCursor = cursor;
    linkCursor.select(QTextCursor::WordUnderCursor);
    Utils::Link link(document->filePath(), linkCursor.blockNumber() + 1, linkCursor.positionInBlock());
    link.linkTextStart = linkCursor.selectionStart();
    link.linkTextEnd = linkCursor.selectionEnd();
    linkUnderCursor = link;
  }
  request.setResponseCallback([callback, linkUnderCursor](const GotoDefinitionRequest::Response &response) {
    handleGotoDefinitionResponse(response, callback, linkUnderCursor);
  });

  sendTextDocumentPositionParamsRequest(m_client, request, m_client->dynamicCapabilities(), m_client->capabilities());
}

struct ItemData {
  Orca::Plugin::Core::TextRange range;
  QVariant userData;
};

auto SymbolSupport::getFileContents(const Utils::FilePath &filePath) -> QStringList
{
  QString fileContent;
  if (const auto document = TextEditor::TextDocument::textDocumentForFilePath(filePath)) {
    fileContent = document->plainText();
  } else {
    Utils::TextFileFormat format;
    format.lineTerminationMode = Utils::TextFileFormat::LFLineTerminator;
    QString error;
    const QTextCodec *codec = Orca::Plugin::Core::EditorManager::defaultTextCodec();
    if (Utils::TextFileFormat::readFile(filePath, codec, &fileContent, &format, &error) != Utils::TextFileFormat::ReadSuccess) {
      qDebug() << "Failed to read file" << filePath << ":" << error;
    }
  }
  return fileContent.split("\n");
}

auto generateSearchResultItems(const QMap<Utils::FilePath, QList<ItemData>> &rangesInDocument) -> QList<Orca::Plugin::Core::SearchResultItem>
{
  QList<Orca::Plugin::Core::SearchResultItem> result;
  for (auto it = rangesInDocument.begin(); it != rangesInDocument.end(); ++it) {
    const auto &filePath = it.key();

    Orca::Plugin::Core::SearchResultItem item;
    item.setFilePath(filePath);
    item.setUseTextEditorFont(true);

    auto lines = SymbolSupport::getFileContents(filePath);
    for (const auto &data : it.value()) {
      item.setMainRange(data.range);
      if (data.range.begin.line > 0 && data.range.begin.line <= lines.size())
        item.setLineText(lines[data.range.begin.line - 1]);
      item.setUserData(data.userData);
      result << item;
    }
  }
  return result;
}

auto generateSearchResultItems(const LanguageClientArray<Location> &locations) -> QList<Orca::Plugin::Core::SearchResultItem>
{
  if (locations.isNull())
    return {};
  QMap<Utils::FilePath, QList<ItemData>> rangesInDocument;
  for (const auto &location : locations.toList())
    rangesInDocument[location.uri().toFilePath()] << ItemData{SymbolSupport::convertRange(location.range()), {}};
  return generateSearchResultItems(rangesInDocument);
}

auto SymbolSupport::handleFindReferencesResponse(const FindReferencesRequest::Response &response, const QString &wordUnderCursor, const ResultHandler &handler) -> void
{
  const auto result = response.result();
  if (handler) {
    const auto locations = result.value_or(nullptr);
    handler(locations.isNull() ? QList<Location>() : locations.toList());
    return;
  }
  if (result) {
    const auto search = Orca::Plugin::Core::SearchResultWindow::instance()->startNewSearch(tr("Find References with %1 for:").arg(m_client->name()), "", wordUnderCursor);
    search->addResults(generateSearchResultItems(result.value()), Orca::Plugin::Core::SearchResult::AddOrdered);
    QObject::connect(search, &Orca::Plugin::Core::SearchResult::activated, [](const Orca::Plugin::Core::SearchResultItem &item) {
      Orca::Plugin::Core::EditorManager::openEditorAtSearchResult(item);
    });
    search->finishSearch(false);
    search->popup();
  }
}

auto SymbolSupport::findUsages(TextEditor::TextDocument *document, const QTextCursor &cursor, const ResultHandler &handler) -> Utils::optional<MessageId>
{
  if (!m_client->reachable())
    return {};
  ReferenceParams params(generateDocPosParams(document, cursor));
  params.setContext(ReferenceParams::ReferenceContext(true));
  FindReferencesRequest request(params);
  auto termCursor(cursor);
  termCursor.select(QTextCursor::WordUnderCursor);
  request.setResponseCallback([this, wordUnderCursor = termCursor.selectedText(), handler](const FindReferencesRequest::Response &response) {
    handleFindReferencesResponse(response, wordUnderCursor, handler);
  });

  sendTextDocumentPositionParamsRequest(m_client, request, m_client->dynamicCapabilities(), m_client->capabilities());
  return request.id();
}

static auto supportsRename(Client *client, TextEditor::TextDocument *document, bool &prepareSupported) -> bool
{
  if (!client->reachable())
    return false;
  prepareSupported = false;
  if (client->dynamicCapabilities().isRegistered(RenameRequest::methodName)) {
    const auto options = client->dynamicCapabilities().option(RenameRequest::methodName).toObject();
    prepareSupported = ServerCapabilities::RenameOptions(options).prepareProvider().value_or(false);
    const TextDocumentRegistrationOptions docOps(options);
    if (docOps.isValid() && !docOps.filterApplies(document->filePath(), Utils::mimeTypeForName(document->mimeType()))) {
      return false;
    }
  }
  if (const auto renameProvider = client->capabilities().renameProvider()) {
    if (Utils::holds_alternative<bool>(*renameProvider)) {
      if (!Utils::get<bool>(*renameProvider))
        return false;
    } else if (Utils::holds_alternative<ServerCapabilities::RenameOptions>(*renameProvider)) {
      prepareSupported = Utils::get<ServerCapabilities::RenameOptions>(*renameProvider).prepareProvider().value_or(false);
    }
  } else {
    return false;
  }
  return true;
}

auto SymbolSupport::supportsRename(TextEditor::TextDocument *document) -> bool
{
  bool prepareSupported;
  return LanguageClient::supportsRename(m_client, document, prepareSupported);
}

auto SymbolSupport::renameSymbol(TextEditor::TextDocument *document, const QTextCursor &cursor) -> void
{
  bool prepareSupported;
  if (!LanguageClient::supportsRename(m_client, document, prepareSupported))
    return;

  auto tc = cursor;
  tc.select(QTextCursor::WordUnderCursor);
  if (prepareSupported)
    requestPrepareRename(generateDocPosParams(document, cursor), tc.selectedText());
  else
    startRenameSymbol(generateDocPosParams(document, cursor), tc.selectedText());
}

auto SymbolSupport::requestPrepareRename(const TextDocumentPositionParams &params, const QString &placeholder) -> void
{
  PrepareRenameRequest request(params);
  request.setResponseCallback([this, params, placeholder](const PrepareRenameRequest::Response &response) {
    const auto &error = response.error();
    if (error.has_value())
      m_client->log(*error);

    const auto &result = response.result();
    if (result.has_value()) {
      if (Utils::holds_alternative<PlaceHolderResult>(*result)) {
        const auto placeHolderResult = Utils::get<PlaceHolderResult>(*result);
        startRenameSymbol(params, placeHolderResult.placeHolder());
      } else if (Utils::holds_alternative<Range>(*result)) {
        auto range = Utils::get<Range>(*result);
        startRenameSymbol(params, placeholder);
      }
    }
  });
  m_client->sendContent(request);
}

auto SymbolSupport::requestRename(const TextDocumentPositionParams &positionParams, const QString &newName, Orca::Plugin::Core::SearchResult *search) -> void
{
  RenameParams params(positionParams);
  params.setNewName(newName);
  RenameRequest request(params);
  request.setResponseCallback([this, search](const RenameRequest::Response &response) {
    handleRenameResponse(search, response);
  });
  m_client->sendContent(request);
  search->setTextToReplace(newName);
  search->popup();
}

auto generateReplaceItems(const WorkspaceEdit &edits) -> QList<Orca::Plugin::Core::SearchResultItem>
{
  auto convertEdits = [](const QList<TextEdit> &edits) {
    return Utils::transform(edits, [](const TextEdit &edit) {
      return ItemData{SymbolSupport::convertRange(edit.range()), QVariant(edit)};
    });
  };
  QMap<Utils::FilePath, QList<ItemData>> rangesInDocument;
  auto documentChanges = edits.documentChanges().value_or(QList<TextDocumentEdit>());
  if (!documentChanges.isEmpty()) {
    for (const auto &documentChange : qAsConst(documentChanges)) {
      rangesInDocument[documentChange.textDocument().uri().toFilePath()] = convertEdits(documentChange.edits());
    }
  } else {
    auto changes = edits.changes().value_or(WorkspaceEdit::Changes());
    for (auto it = changes.begin(), end = changes.end(); it != end; ++it)
      rangesInDocument[it.key().toFilePath()] = convertEdits(it.value());
  }
  return generateSearchResultItems(rangesInDocument);
}

auto SymbolSupport::startRenameSymbol(const TextDocumentPositionParams &positionParams, const QString &placeholder) -> void
{
  auto search = Orca::Plugin::Core::SearchResultWindow::instance()->startNewSearch(tr("Find References with %1 for:").arg(m_client->name()), "", placeholder, Orca::Plugin::Core::SearchResultWindow::SearchAndReplace);
  search->setSearchAgainSupported(true);
  const auto label = new QLabel(tr("Search Again to update results and re-enable Replace"));
  label->setVisible(false);
  search->setAdditionalReplaceWidget(label);
  QObject::connect(search, &Orca::Plugin::Core::SearchResult::activated, [](const Orca::Plugin::Core::SearchResultItem &item) {
    Orca::Plugin::Core::EditorManager::openEditorAtSearchResult(item);
  });
  QObject::connect(search, &Orca::Plugin::Core::SearchResult::replaceTextChanged, [search]() {
    search->additionalReplaceWidget()->setVisible(true);
    search->setSearchAgainEnabled(true);
    search->setReplaceEnabled(false);
  });
  QObject::connect(search, &Orca::Plugin::Core::SearchResult::searchAgainRequested, [this, positionParams, search]() {
    search->restart();
    requestRename(positionParams, search->textToReplace(), search);
  });
  QObject::connect(search, &Orca::Plugin::Core::SearchResult::replaceButtonClicked, [this, positionParams](const QString & /*replaceText*/, const QList<Orca::Plugin::Core::SearchResultItem> &checkedItems) {
    applyRename(checkedItems);
  });

  requestRename(positionParams, placeholder, search);
}

auto SymbolSupport::handleRenameResponse(Orca::Plugin::Core::SearchResult *search, const RenameRequest::Response &response) -> void
{
  const auto &error = response.error();
  if (error.has_value())
    m_client->log(*error);

  const auto &edits = response.result();
  if (edits.has_value()) {
    search->addResults(generateReplaceItems(*edits), Orca::Plugin::Core::SearchResult::AddOrdered);
    search->additionalReplaceWidget()->setVisible(false);
    search->setReplaceEnabled(true);
    search->setSearchAgainEnabled(false);
    search->finishSearch(false);
  } else {
    search->finishSearch(true);
  }
}

auto SymbolSupport::applyRename(const QList<Orca::Plugin::Core::SearchResultItem> &checkedItems) -> void
{
  QMap<DocumentUri, QList<TextEdit>> editsForDocuments;
  for (const auto &item : checkedItems) {
    auto uri = DocumentUri::fromFilePath(Utils::FilePath::fromString(item.path().value(0)));
    TextEdit edit(item.userData().toJsonObject());
    if (edit.isValid())
      editsForDocuments[uri] << edit;
  }

  for (auto it = editsForDocuments.begin(), end = editsForDocuments.end(); it != end; ++it)
    applyTextEdits(it.key(), it.value());
}

auto SymbolSupport::convertRange(const Range &range) -> Orca::Plugin::Core::TextRange
{
  auto convertPosition = [](const Position &pos) {
    return Orca::Plugin::Core::TextPosition(pos.line() + 1, pos.character());
  };
  return Orca::Plugin::Core::TextRange(convertPosition(range.start()), convertPosition(range.end()));
}

} // namespace LanguageClient
