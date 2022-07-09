// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "client.hpp"

#include "languageclientinterface.hpp"
#include "languageclientmanager.hpp"
#include "languageclientutils.hpp"
#include "semantichighlightsupport.hpp"

#include <core/editormanager/documentmodel.hpp>
#include <core/icore.hpp>
#include <core/idocument.hpp>
#include <core/messagemanager.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <languageserverprotocol/completion.h>
#include <languageserverprotocol/diagnostics.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/messages.h>
#include <languageserverprotocol/servercapabilities.h>
#include <languageserverprotocol/workspace.h>
#include <languageserverprotocol/progresssupport.h>

#include <projectexplorer/project.hpp>
#include <projectexplorer/session.hpp>

#include <texteditor/codeassist/documentcontentcompletion.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/ioutlinewidget.hpp>
#include <texteditor/syntaxhighlighter.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditoractionhandler.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcprocess.hpp>


#include <QDebug>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

using namespace LanguageServerProtocol;
using namespace Utils;

namespace LanguageClient {

static Q_LOGGING_CATEGORY(LOGLSPCLIENT, "qtc.languageclient.client", QtWarningMsg);

Client::Client(BaseClientInterface *clientInterface) : m_id(Utils::Id::fromString(QUuid::createUuid().toString())), m_clientInterface(clientInterface), m_diagnosticManager(this), m_documentSymbolCache(this), m_hoverHandler(this), m_symbolSupport(this), m_tokenSupport(this)
{
  using namespace ProjectExplorer;
  m_clientProviders.completionAssistProvider = new LanguageClientCompletionAssistProvider(this);
  m_clientProviders.functionHintProvider = new FunctionHintAssistProvider(this);
  m_clientProviders.quickFixAssistProvider = new LanguageClientQuickFixProvider(this);

  m_documentUpdateTimer.setSingleShot(true);
  m_documentUpdateTimer.setInterval(500);
  connect(&m_documentUpdateTimer, &QTimer::timeout, this, [this] { sendPostponedDocumentUpdates(Schedule::Now); });
  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, &Client::projectClosed);

  m_contentHandler.insert(JsonRpcMessageHandler::jsonRpcMimeType(), &JsonRpcMessageHandler::parseContent);
  QTC_ASSERT(clientInterface, return);
  connect(clientInterface, &BaseClientInterface::messageReceived, this, &Client::handleMessage);
  connect(clientInterface, &BaseClientInterface::error, this, &Client::setError);
  connect(clientInterface, &BaseClientInterface::finished, this, &Client::finished);
  connect(Core::EditorManager::instance(), &Core::EditorManager::documentClosed, this, &Client::documentClosed);

  m_tokenSupport.setTokenTypesMap(SemanticTokens::defaultTokenTypesMap());
  m_tokenSupport.setTokenModifiersMap(SemanticTokens::defaultTokenModifiersMap());

  m_shutdownTimer.setInterval(20 /*seconds*/ * 1000);
  connect(&m_shutdownTimer, &QTimer::timeout, this, [this] {
    LanguageClientManager::deleteClient(this);
  });
}

auto Client::name() const -> QString
{
  if (m_project && !m_project->displayName().isEmpty())
    return tr("%1 for %2").arg(m_displayName, m_project->displayName());
  return m_displayName;
}

static auto updateEditorToolBar(QList<TextEditor::TextDocument*> documents) -> void
{
  for (const auto document : documents) {
    for (const auto editor : Core::DocumentModel::editorsForDocument(document))
      updateEditorToolBar(editor);
  }
}

Client::~Client()
{
  using namespace TextEditor;
  // FIXME: instead of replacing the completion provider in the text document store the
  // completion provider as a prioritised list in the text document
  // temporary container needed since m_resetAssistProvider is changed in resetAssistProviders
  for (const auto document : m_resetAssistProvider.keys())
    resetAssistProviders(document);
  const auto &editors = Core::DocumentModel::editorsForOpenedDocuments();
  for (const auto editor : editors) {
    if (const auto textEditor = qobject_cast<BaseTextEditor*>(editor)) {
      const auto widget = textEditor->editorWidget();
      widget->setRefactorMarkers(RefactorMarker::filterOutType(widget->refactorMarkers(), id()));
      widget->removeHoverHandler(&m_hoverHandler);
    }
  }
  for (const auto processor : qAsConst(m_runningAssistProcessors))
    processor->setAsyncProposalAvailable(nullptr);
  qDeleteAll(m_documentHighlightsTimer);
  m_documentHighlightsTimer.clear();
  updateEditorToolBar(m_openedDocument.keys());
  // do not handle messages while shutting down
  disconnect(m_clientInterface.data(), &BaseClientInterface::messageReceived, this, &Client::handleMessage);
}

static auto generateClientCapabilities() -> ClientCapabilities
{
  ClientCapabilities capabilities;
  WorkspaceClientCapabilities workspaceCapabilities;
  workspaceCapabilities.setWorkspaceFolders(true);
  workspaceCapabilities.setApplyEdit(true);
  DynamicRegistrationCapabilities allowDynamicRegistration;
  allowDynamicRegistration.setDynamicRegistration(true);
  workspaceCapabilities.setDidChangeConfiguration(allowDynamicRegistration);
  workspaceCapabilities.setExecuteCommand(allowDynamicRegistration);
  workspaceCapabilities.setConfiguration(true);
  SemanticTokensWorkspaceClientCapabilities semanticTokensWorkspaceClientCapabilities;
  semanticTokensWorkspaceClientCapabilities.setRefreshSupport(true);
  workspaceCapabilities.setSemanticTokens(semanticTokensWorkspaceClientCapabilities);
  capabilities.setWorkspace(workspaceCapabilities);

  TextDocumentClientCapabilities documentCapabilities;
  TextDocumentClientCapabilities::SynchronizationCapabilities syncCapabilities;
  syncCapabilities.setDynamicRegistration(true);
  syncCapabilities.setWillSave(true);
  syncCapabilities.setWillSaveWaitUntil(false);
  syncCapabilities.setDidSave(true);
  documentCapabilities.setSynchronization(syncCapabilities);

  SymbolCapabilities symbolCapabilities;
  SymbolCapabilities::SymbolKindCapabilities symbolKindCapabilities;
  symbolKindCapabilities.setValueSet({SymbolKind::File, SymbolKind::Module, SymbolKind::Namespace, SymbolKind::Package, SymbolKind::Class, SymbolKind::Method, SymbolKind::Property, SymbolKind::Field, SymbolKind::Constructor, SymbolKind::Enum, SymbolKind::Interface, SymbolKind::Function, SymbolKind::Variable, SymbolKind::Constant, SymbolKind::String, SymbolKind::Number, SymbolKind::Boolean, SymbolKind::Array, SymbolKind::Object, SymbolKind::Key, SymbolKind::Null, SymbolKind::EnumMember, SymbolKind::Struct, SymbolKind::Event, SymbolKind::Operator, SymbolKind::TypeParameter});
  symbolCapabilities.setSymbolKind(symbolKindCapabilities);
  symbolCapabilities.setHierarchicalDocumentSymbolSupport(true);
  documentCapabilities.setDocumentSymbol(symbolCapabilities);

  TextDocumentClientCapabilities::CompletionCapabilities completionCapabilities;
  completionCapabilities.setDynamicRegistration(true);
  TextDocumentClientCapabilities::CompletionCapabilities::CompletionItemKindCapabilities completionItemKindCapabilities;
  completionItemKindCapabilities.setValueSet({CompletionItemKind::Text, CompletionItemKind::Method, CompletionItemKind::Function, CompletionItemKind::Constructor, CompletionItemKind::Field, CompletionItemKind::Variable, CompletionItemKind::Class, CompletionItemKind::Interface, CompletionItemKind::Module, CompletionItemKind::Property, CompletionItemKind::Unit, CompletionItemKind::Value, CompletionItemKind::Enum, CompletionItemKind::Keyword, CompletionItemKind::Snippet, CompletionItemKind::Color, CompletionItemKind::File, CompletionItemKind::Reference, CompletionItemKind::Folder, CompletionItemKind::EnumMember, CompletionItemKind::Constant, CompletionItemKind::Struct, CompletionItemKind::Event, CompletionItemKind::Operator, CompletionItemKind::TypeParameter});
  completionCapabilities.setCompletionItemKind(completionItemKindCapabilities);
  TextDocumentClientCapabilities::CompletionCapabilities::CompletionItemCapbilities completionItemCapbilities;
  completionItemCapbilities.setSnippetSupport(true);
  completionItemCapbilities.setCommitCharacterSupport(true);
  completionCapabilities.setCompletionItem(completionItemCapbilities);
  documentCapabilities.setCompletion(completionCapabilities);

  TextDocumentClientCapabilities::CodeActionCapabilities codeActionCapabilities;
  TextDocumentClientCapabilities::CodeActionCapabilities::CodeActionLiteralSupport literalSupport;
  literalSupport.setCodeActionKind(TextDocumentClientCapabilities::CodeActionCapabilities::CodeActionLiteralSupport::CodeActionKind(QList<QString>{"*"}));
  codeActionCapabilities.setCodeActionLiteralSupport(literalSupport);
  documentCapabilities.setCodeAction(codeActionCapabilities);

  TextDocumentClientCapabilities::HoverCapabilities hover;
  hover.setContentFormat({MarkupKind::markdown, MarkupKind::plaintext});
  hover.setDynamicRegistration(true);
  documentCapabilities.setHover(hover);

  TextDocumentClientCapabilities::RenameClientCapabilities rename;
  rename.setPrepareSupport(true);
  rename.setDynamicRegistration(true);
  documentCapabilities.setRename(rename);

  TextDocumentClientCapabilities::SignatureHelpCapabilities signatureHelp;
  signatureHelp.setDynamicRegistration(true);
  TextDocumentClientCapabilities::SignatureHelpCapabilities::SignatureInformationCapabilities info;
  info.setDocumentationFormat({MarkupKind::markdown, MarkupKind::plaintext});
  info.setActiveParameterSupport(true);
  signatureHelp.setSignatureInformation(info);
  documentCapabilities.setSignatureHelp(signatureHelp);

  documentCapabilities.setReferences(allowDynamicRegistration);
  documentCapabilities.setDocumentHighlight(allowDynamicRegistration);
  documentCapabilities.setDefinition(allowDynamicRegistration);
  documentCapabilities.setTypeDefinition(allowDynamicRegistration);
  documentCapabilities.setImplementation(allowDynamicRegistration);
  documentCapabilities.setFormatting(allowDynamicRegistration);
  documentCapabilities.setRangeFormatting(allowDynamicRegistration);
  documentCapabilities.setOnTypeFormatting(allowDynamicRegistration);
  SemanticTokensClientCapabilities tokens;
  tokens.setDynamicRegistration(true);
  FullSemanticTokenOptions tokenOptions;
  tokenOptions.setDelta(true);
  SemanticTokensClientCapabilities::Requests tokenRequests;
  tokenRequests.setFull(tokenOptions);
  tokens.setRequests(tokenRequests);
  tokens.setTokenTypes({"type", "class", "enumMember", "typeParameter", "parameter", "variable", "function", "macro", "keyword", "comment", "string", "number", "operator"});
  tokens.setTokenModifiers({"declaration", "definition"});
  tokens.setFormats({"relative"});
  documentCapabilities.setSemanticTokens(tokens);
  capabilities.setTextDocument(documentCapabilities);

  WindowClientClientCapabilities window;
  window.setWorkDoneProgress(true);
  capabilities.setWindow(window);

  return capabilities;
}

auto Client::initialize() -> void
{
  using namespace ProjectExplorer;
  QTC_ASSERT(m_clientInterface, return);
  QTC_ASSERT(m_state == Uninitialized, return);
  qCDebug(LOGLSPCLIENT) << "initializing language server " << m_displayName;
  InitializeParams params;
  params.setCapabilities(m_clientCapabilities);
  params.setInitializationOptions(m_initializationOptions);
  if (m_project) {
    params.setRootUri(DocumentUri::fromFilePath(m_project->projectDirectory()));
    params.setWorkSpaceFolders(Utils::transform(SessionManager::projects(), [](Project *pro) {
      return WorkSpaceFolder(DocumentUri::fromFilePath(pro->projectDirectory()), pro->displayName());
    }));
  }
  InitializeRequest initRequest(params);
  initRequest.setResponseCallback([this](const InitializeRequest::Response &initResponse) {
    initializeCallback(initResponse);
  });
  if (const auto responseHandler = initRequest.responseHandler())
    m_responseHandlers[responseHandler->id] = responseHandler->callback;

  // directly send message otherwise the state check of sendContent would fail
  sendMessage(initRequest.toBaseMessage());
  m_state = InitializeRequested;
}

auto Client::shutdown() -> void
{
  QTC_ASSERT(m_state == Initialized, emit finished(); return);
  qCDebug(LOGLSPCLIENT) << "shutdown language server " << m_displayName;
  ShutdownRequest shutdown;
  shutdown.setResponseCallback([this](const ShutdownRequest::Response &shutdownResponse) {
    shutDownCallback(shutdownResponse);
  });
  sendContent(shutdown);
  m_state = ShutdownRequested;
  m_shutdownTimer.start();
}

auto Client::state() const -> Client::State
{
  return m_state;
}

auto Client::stateString() const -> QString
{
  switch (m_state) {
  case Uninitialized:
    return tr("uninitialized");
  case InitializeRequested:
    return tr("initialize requested");
  case Initialized:
    return tr("initialized");
  case ShutdownRequested:
    return tr("shutdown requested");
  case Shutdown:
    return tr("shutdown");
  case Error:
    return tr("error");
  }
  return {};
}

auto Client::defaultClientCapabilities() -> ClientCapabilities
{
  return generateClientCapabilities();
}

auto Client::setClientCapabilities(const LanguageServerProtocol::ClientCapabilities &caps) -> void
{
  m_clientCapabilities = caps;
}

auto Client::openDocument(TextEditor::TextDocument *document) -> void
{
  using namespace TextEditor;
  if (m_openedDocument.contains(document) || !isSupportedDocument(document))
    return;

  if (m_state != Initialized) {
    m_postponedDocuments << document;
    return;
  }

  const auto &filePath = document->filePath();
  const QString method(DidOpenTextDocumentNotification::methodName);
  if (const auto registered = m_dynamicCapabilities.isRegistered(method)) {
    if (!registered.value())
      return;
    const TextDocumentRegistrationOptions option(m_dynamicCapabilities.option(method).toObject());
    if (option.isValid() && !option.filterApplies(filePath, Utils::mimeTypeForName(document->mimeType()))) {
      return;
    }
  } else if (const auto _sync = m_serverCapabilities.textDocumentSync()) {
    if (const auto options = Utils::get_if<TextDocumentSyncOptions>(&_sync.value())) {
      if (!options->openClose().value_or(true))
        return;
    }
  }

  m_openedDocument[document] = document->plainText();
  connect(document, &TextDocument::contentsChangedWithPosition, this, [this, document](int position, int charsRemoved, int charsAdded) {
    documentContentsChanged(document, position, charsRemoved, charsAdded);
  });
  TextDocumentItem item;
  item.setLanguageId(TextDocumentItem::mimeTypeToLanguageId(document->mimeType()));
  item.setUri(DocumentUri::fromFilePath(filePath));
  item.setText(document->plainText());
  if (!m_documentVersions.contains(filePath))
    m_documentVersions[filePath] = 0;
  item.setVersion(m_documentVersions[filePath]);
  sendContent(DidOpenTextDocumentNotification(DidOpenTextDocumentParams(item)));
  handleDocumentOpened(document);

  const Client *currentClient = LanguageClientManager::clientForDocument(document);
  if (currentClient == this) {
    // this is the active client for the document so directly activate it
    activateDocument(document);
  } else if (m_activateDocAutomatically && currentClient == nullptr) {
    // there is no client for this document so assign it to this server
    LanguageClientManager::openDocumentWithClient(document, this);
  }
}

auto Client::sendContent(const IContent &content, SendDocUpdates sendUpdates) -> void
{
  QTC_ASSERT(m_clientInterface, return);
  QTC_ASSERT(m_state == Initialized, return);
  if (sendUpdates == SendDocUpdates::Send)
    sendPostponedDocumentUpdates(Schedule::Delayed);
  if (const auto responseHandler = content.responseHandler())
    m_responseHandlers[responseHandler->id] = responseHandler->callback;
  QString error;
  if (!QTC_GUARD(content.isValid(&error)))
    Core::MessageManager::writeFlashing(error);
  sendMessage(content.toBaseMessage());
}

auto Client::cancelRequest(const MessageId &id) -> void
{
  m_responseHandlers.remove(id);
  sendContent(CancelRequest(CancelParameter(id)), SendDocUpdates::Ignore);
}

auto Client::closeDocument(TextEditor::TextDocument *document) -> void
{
  deactivateDocument(document);
  const auto &uri = DocumentUri::fromFilePath(document->filePath());
  m_postponedDocuments.remove(document);
  if (m_openedDocument.remove(document) != 0) {
    handleDocumentClosed(document);
    if (m_state == Initialized) {
      const DidCloseTextDocumentParams params(TextDocumentIdentifier{uri});
      sendContent(DidCloseTextDocumentNotification(params));
    }
  }
}

auto Client::updateCompletionProvider(TextEditor::TextDocument *document) -> void
{
  auto useLanguageServer = m_serverCapabilities.completionProvider().has_value();
  const auto clientCompletionProvider = static_cast<LanguageClientCompletionAssistProvider*>(m_clientProviders.completionAssistProvider.data());
  if (m_dynamicCapabilities.isRegistered(CompletionRequest::methodName).value_or(false)) {
    const auto &options = m_dynamicCapabilities.option(CompletionRequest::methodName);
    const TextDocumentRegistrationOptions docOptions(options);
    useLanguageServer = docOptions.filterApplies(document->filePath(), Utils::mimeTypeForName(document->mimeType()));

    const ServerCapabilities::CompletionOptions completionOptions(options);
    if (completionOptions.isValid())
      clientCompletionProvider->setTriggerCharacters(completionOptions.triggerCharacters());
  }

  if (document->completionAssistProvider() != clientCompletionProvider) {
    if (useLanguageServer) {
      m_resetAssistProvider[document].completionAssistProvider = document->completionAssistProvider();
      document->setCompletionAssistProvider(clientCompletionProvider);
    }
  } else if (!useLanguageServer) {
    document->setCompletionAssistProvider(m_resetAssistProvider[document].completionAssistProvider);
  }
}

auto Client::updateFunctionHintProvider(TextEditor::TextDocument *document) -> void
{
  auto useLanguageServer = m_serverCapabilities.signatureHelpProvider().has_value();
  const auto clientFunctionHintProvider = static_cast<FunctionHintAssistProvider*>(m_clientProviders.functionHintProvider.data());
  if (m_dynamicCapabilities.isRegistered(SignatureHelpRequest::methodName).value_or(false)) {
    const auto &options = m_dynamicCapabilities.option(SignatureHelpRequest::methodName);
    const TextDocumentRegistrationOptions docOptions(options);
    useLanguageServer = docOptions.filterApplies(document->filePath(), Utils::mimeTypeForName(document->mimeType()));

    const ServerCapabilities::SignatureHelpOptions signatureOptions(options);
    if (signatureOptions.isValid())
      clientFunctionHintProvider->setTriggerCharacters(signatureOptions.triggerCharacters());
  }

  if (document->functionHintAssistProvider() != clientFunctionHintProvider) {
    if (useLanguageServer) {
      m_resetAssistProvider[document].functionHintProvider = document->functionHintAssistProvider();
      document->setFunctionHintAssistProvider(clientFunctionHintProvider);
    }
  } else if (!useLanguageServer) {
    document->setFunctionHintAssistProvider(m_resetAssistProvider[document].functionHintProvider);
  }
}

auto Client::requestDocumentHighlights(TextEditor::TextEditorWidget *widget) -> void
{
  auto timer = m_documentHighlightsTimer[widget];
  if (!timer) {
    const auto uri = DocumentUri::fromFilePath(widget->textDocument()->filePath());
    if (m_highlightRequests.contains(widget))
      cancelRequest(m_highlightRequests.take(widget));
    timer = new QTimer;
    timer->setSingleShot(true);
    m_documentHighlightsTimer.insert(widget, timer);
    auto connection = connect(widget, &QWidget::destroyed, this, [widget, this]() {
      delete m_documentHighlightsTimer.take(widget);
    });
    connect(timer, &QTimer::timeout, this, [this, widget, connection]() {
      disconnect(connection);
      requestDocumentHighlightsNow(widget);
      m_documentHighlightsTimer.take(widget)->deleteLater();
    });
  }
  timer->start(250);
}

auto Client::requestDocumentHighlightsNow(TextEditor::TextEditorWidget *widget) -> void
{
  const auto uri = DocumentUri::fromFilePath(widget->textDocument()->filePath());
  if (m_dynamicCapabilities.isRegistered(DocumentHighlightsRequest::methodName).value_or(false)) {
    const TextDocumentRegistrationOptions option(m_dynamicCapabilities.option(DocumentHighlightsRequest::methodName));
    if (!option.filterApplies(widget->textDocument()->filePath()))
      return;
  } else {
    const auto provider = m_serverCapabilities.documentHighlightProvider();
    if (!provider.has_value())
      return;
    if (Utils::holds_alternative<bool>(*provider) && !Utils::get<bool>(*provider))
      return;
  }

  if (m_highlightRequests.contains(widget))
    cancelRequest(m_highlightRequests.take(widget));

  const auto adjustedCursor = adjustedCursorForHighlighting(widget->textCursor(), widget->textDocument());
  DocumentHighlightsRequest request(TextDocumentPositionParams(TextDocumentIdentifier(uri), Position{adjustedCursor}));
  auto connection = connect(widget, &QObject::destroyed, this, [this, widget]() {
    if (m_highlightRequests.contains(widget))
      cancelRequest(m_highlightRequests.take(widget));
  });
  request.setResponseCallback([widget, this, uri, connection](const DocumentHighlightsRequest::Response &response) {
    m_highlightRequests.remove(widget);
    disconnect(connection);
    const auto &id = TextEditor::TextEditorWidget::CodeSemanticsSelection;
    QList<QTextEdit::ExtraSelection> selections;
    const auto &result = response.result();
    if (!result.has_value() || holds_alternative<std::nullptr_t>(result.value())) {
      widget->setExtraSelections(id, selections);
      return;
    }

    const auto &format = widget->textDocument()->fontSettings().toTextCharFormat(TextEditor::C_OCCURRENCES);
    const auto document = widget->document();
    for (const auto &highlight : get<QList<DocumentHighlight>>(result.value())) {
      QTextEdit::ExtraSelection selection{widget->textCursor(), format};
      const auto &start = highlight.range().start().toPositionInDocument(document);
      const auto &end = highlight.range().end().toPositionInDocument(document);
      if (start < 0 || end < 0)
        continue;
      selection.cursor.setPosition(start);
      selection.cursor.setPosition(end, QTextCursor::KeepAnchor);
      selections << selection;
    }
    widget->setExtraSelections(id, selections);
  });
  m_highlightRequests[widget] = request.id();
  sendContent(request);
}

auto Client::activateDocument(TextEditor::TextDocument *document) -> void
{
  const auto &filePath = document->filePath();
  const auto uri = DocumentUri::fromFilePath(filePath);
  m_diagnosticManager.showDiagnostics(uri, m_documentVersions.value(filePath));
  m_tokenSupport.updateSemanticTokens(document);
  // only replace the assist provider if the language server support it
  updateCompletionProvider(document);
  updateFunctionHintProvider(document);
  if (m_serverCapabilities.codeActionProvider()) {
    m_resetAssistProvider[document].quickFixAssistProvider = document->quickFixAssistProvider();
    document->setQuickFixAssistProvider(m_clientProviders.quickFixAssistProvider);
  }
  document->setFormatter(new LanguageClientFormatter(document, this));
  for (const auto editor : Core::DocumentModel::editorsForDocument(document)) {
    updateEditorToolBar(editor);
    if (const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor)) {
      const auto widget = textEditor->editorWidget();
      widget->addHoverHandler(&m_hoverHandler);
      requestDocumentHighlights(widget);
      if (symbolSupport().supportsRename(document))
        widget->addOptionalActions(TextEditor::TextEditorActionHandler::RenameSymbol);
    }
  }
}

auto Client::deactivateDocument(TextEditor::TextDocument *document) -> void
{
  m_diagnosticManager.hideDiagnostics(document->filePath());
  resetAssistProviders(document);
  document->setFormatter(nullptr);
  m_tokenSupport.clearHighlight(document);
  for (const auto editor : Core::DocumentModel::editorsForDocument(document)) {
    if (const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor)) {
      const auto widget = textEditor->editorWidget();
      widget->removeHoverHandler(&m_hoverHandler);
      widget->setExtraSelections(TextEditor::TextEditorWidget::CodeSemanticsSelection, {});
    }
  }
}

auto Client::documentClosed(Core::IDocument *document) -> void
{
  if (const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document))
    closeDocument(textDocument);
}

auto Client::documentOpen(const TextEditor::TextDocument *document) const -> bool
{
  return m_openedDocument.contains(const_cast<TextEditor::TextDocument*>(document));
}

auto Client::documentForFilePath(const Utils::FilePath &file) const -> TextEditor::TextDocument*
{
  for (auto it = m_openedDocument.cbegin(); it != m_openedDocument.cend(); ++it) {
    if (it.key()->filePath() == file)
      return it.key();
  }
  return nullptr;
}

auto Client::documentContentsSaved(TextEditor::TextDocument *document) -> void
{
  if (!m_openedDocument.contains(document))
    return;
  auto sendMessage = true;
  auto includeText = false;
  const QString method(DidSaveTextDocumentNotification::methodName);
  if (const auto registered = m_dynamicCapabilities.isRegistered(method)) {
    sendMessage = registered.value();
    if (sendMessage) {
      const TextDocumentSaveRegistrationOptions option(m_dynamicCapabilities.option(method).toObject());
      if (option.isValid()) {
        sendMessage = option.filterApplies(document->filePath(), Utils::mimeTypeForName(document->mimeType()));
        includeText = option.includeText().value_or(includeText);
      }
    }
  } else if (const auto _sync = m_serverCapabilities.textDocumentSync()) {
    if (const auto options = Utils::get_if<TextDocumentSyncOptions>(&_sync.value())) {
      if (const auto saveOptions = options->save())
        includeText = saveOptions.value().includeText().value_or(includeText);
    }
  }
  if (!sendMessage)
    return;
  DidSaveTextDocumentParams params(TextDocumentIdentifier(DocumentUri::fromFilePath(document->filePath())));
  if (includeText)
    params.setText(document->plainText());
  sendContent(DidSaveTextDocumentNotification(params));
}

auto Client::documentWillSave(Core::IDocument *document) -> void
{
  const auto &filePath = document->filePath();
  const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document);
  if (!m_openedDocument.contains(textDocument))
    return;
  auto sendMessage = false;
  const QString method(WillSaveTextDocumentNotification::methodName);
  if (const auto registered = m_dynamicCapabilities.isRegistered(method)) {
    sendMessage = registered.value();
    if (sendMessage) {
      const TextDocumentRegistrationOptions option(m_dynamicCapabilities.option(method));
      if (option.isValid()) {
        sendMessage = option.filterApplies(filePath, Utils::mimeTypeForName(document->mimeType()));
      }
    }
  } else if (const auto _sync = m_serverCapabilities.textDocumentSync()) {
    if (const auto options = Utils::get_if<TextDocumentSyncOptions>(&_sync.value()))
      sendMessage = options->willSave().value_or(sendMessage);
  }
  if (!sendMessage)
    return;
  const WillSaveTextDocumentParams params(TextDocumentIdentifier(DocumentUri::fromFilePath(filePath)));
  sendContent(WillSaveTextDocumentNotification(params));
}

auto Client::documentContentsChanged(TextEditor::TextDocument *document, int position, int charsRemoved, int charsAdded) -> void
{
  if (!m_openedDocument.contains(document) || !reachable())
    return;
  const QString method(DidChangeTextDocumentNotification::methodName);
  auto syncKind = m_serverCapabilities.textDocumentSyncKindHelper();
  if (const auto registered = m_dynamicCapabilities.isRegistered(method)) {
    syncKind = registered.value() ? TextDocumentSyncKind::Full : TextDocumentSyncKind::None;
    if (syncKind != TextDocumentSyncKind::None) {
      const TextDocumentChangeRegistrationOptions option(m_dynamicCapabilities.option(method).toObject());
      syncKind = option.isValid() ? option.syncKind() : syncKind;
    }
  }

  if (syncKind != TextDocumentSyncKind::None) {
    if (syncKind == TextDocumentSyncKind::Incremental) {
      // If the new change is a pure insertion and its range is adjacent to the range of the
      // previous change, we can trivially merge the two changes.
      // For the typical case of the user typing a continuous sequence of characters,
      // this will save a lot of TextDocumentContentChangeEvent elements in the data stream,
      // as otherwise we'd send tons of single-character changes.
      const auto &text = document->textAt(position, charsAdded);
      auto &queue = m_documentsToUpdate[document];
      auto append = true;
      if (!queue.isEmpty() && charsRemoved == 0) {
        auto &prev = queue.last();
        const auto prevStart = prev.range()->start().toPositionInDocument(document->document());
        if (prevStart + prev.text().length() == position) {
          prev.setText(prev.text() + text);
          append = false;
        }
      }
      if (append) {
        QTextDocument oldDoc(m_openedDocument[document]);
        QTextCursor cursor(&oldDoc);
        // Workaround https://bugreports.qt.io/browse/QTBUG-80662
        // The contentsChanged gives a character count that can be wrong for QTextCursor
        // when there are special characters removed/added (like formating characters).
        // Also, characterCount return the number of characters + 1 because of the hidden
        // paragraph separator character.
        // This implementation is based on QWidgetTextControlPrivate::_q_contentsChanged.
        // For charsAdded, textAt handles the case itself.
        cursor.setPosition(qMin(oldDoc.characterCount() - 1, position + charsRemoved));
        cursor.setPosition(position, QTextCursor::KeepAnchor);
        DidChangeTextDocumentParams::TextDocumentContentChangeEvent change;
        change.setRange(Range(cursor));
        change.setRangeLength(cursor.selectionEnd() - cursor.selectionStart());
        change.setText(text);
        queue << change;
      }
    } else {
      m_documentsToUpdate[document] = {DidChangeTextDocumentParams::TextDocumentContentChangeEvent(document->plainText())};
    }
    m_openedDocument[document] = document->plainText();
  }

  ++m_documentVersions[document->filePath()];
  using namespace TextEditor;
  for (const auto editor : BaseTextEditor::textEditorsForDocument(document)) {
    auto widget = editor->editorWidget();
    QTC_ASSERT(widget, continue);
    delete m_documentHighlightsTimer.take(widget);
    widget->setRefactorMarkers(RefactorMarker::filterOutType(widget->refactorMarkers(), id()));
  }
  m_documentUpdateTimer.start();
}

auto Client::registerCapabilities(const QList<Registration> &registrations) -> void
{
  m_dynamicCapabilities.registerCapability(registrations);
  for (const auto &registration : registrations) {
    if (registration.method() == CompletionRequest::methodName) {
      for (const auto document : m_openedDocument.keys())
        updateCompletionProvider(document);
    }
    if (registration.method() == SignatureHelpRequest::methodName) {
      for (const auto document : m_openedDocument.keys())
        updateFunctionHintProvider(document);
    }
    if (registration.method() == "textDocument/semanticTokens") {
      SemanticTokensOptions options(registration.registerOptions());
      if (options.isValid())
        m_tokenSupport.setLegend(options.legend());
      for (const auto document : m_openedDocument.keys())
        m_tokenSupport.updateSemanticTokens(document);
    }
  }
  emit capabilitiesChanged(m_dynamicCapabilities);
}

auto Client::unregisterCapabilities(const QList<Unregistration> &unregistrations) -> void
{
  m_dynamicCapabilities.unregisterCapability(unregistrations);
  for (const auto &unregistration : unregistrations) {
    if (unregistration.method() == CompletionRequest::methodName) {
      for (const auto document : m_openedDocument.keys())
        updateCompletionProvider(document);
    }
    if (unregistration.method() == SignatureHelpRequest::methodName) {
      for (const auto document : m_openedDocument.keys())
        updateFunctionHintProvider(document);
    }
    if (unregistration.method() == "textDocument/semanticTokens") {
      for (const auto document : m_openedDocument.keys())
        m_tokenSupport.updateSemanticTokens(document);
    }
  }
  emit capabilitiesChanged(m_dynamicCapabilities);
}

auto createHighlightingResult(const SymbolInformation &info) -> TextEditor::HighlightingResult
{
  if (!info.isValid())
    return {};
  const auto &start = info.location().range().start();
  return TextEditor::HighlightingResult(start.line() + 1, start.character() + 1, info.name().length(), info.kind());
}

auto Client::cursorPositionChanged(TextEditor::TextEditorWidget *widget) -> void
{
  const auto document = widget->textDocument();
  if (m_documentsToUpdate.find(document) != m_documentsToUpdate.end())
    return; // we are currently changing this document so postpone the DocumentHighlightsRequest
  requestDocumentHighlights(widget);
  const auto selectionsId(TextEditor::TextEditorWidget::CodeSemanticsSelection);
  const auto semanticSelections = widget->extraSelections(selectionsId);
  if (!semanticSelections.isEmpty()) {
    auto selectionContainsPos = [pos = widget->position()](const QTextEdit::ExtraSelection &selection) {
      const auto cursor = selection.cursor;
      return cursor.selectionStart() <= pos && cursor.selectionEnd() >= pos;
    };
    if (!Utils::anyOf(semanticSelections, selectionContainsPos))
      widget->setExtraSelections(selectionsId, {});
  }
}

auto Client::symbolSupport() -> SymbolSupport&
{
  return m_symbolSupport;
}

auto Client::requestCodeActions(const DocumentUri &uri, const QList<Diagnostic> &diagnostics) -> void
{
  const auto fileName = uri.toFilePath();
  const auto doc = TextEditor::TextDocument::textDocumentForFilePath(fileName);
  if (!doc)
    return;

  CodeActionParams codeActionParams;
  CodeActionParams::CodeActionContext context;
  context.setDiagnostics(diagnostics);
  codeActionParams.setContext(context);
  codeActionParams.setTextDocument(TextDocumentIdentifier(uri));
  const Position start(0, 0);
  const auto &lastBlock = doc->document()->lastBlock();
  const Position end(lastBlock.blockNumber(), lastBlock.length() - 1);
  codeActionParams.setRange(Range(start, end));
  CodeActionRequest request(codeActionParams);
  request.setResponseCallback([uri, self = QPointer<Client>(this)](const CodeActionRequest::Response &response) {
    if (self)
      self->handleCodeActionResponse(response, uri);
  });
  requestCodeActions(request);
}

auto Client::requestCodeActions(const CodeActionRequest &request) -> void
{
  if (!request.isValid(nullptr))
    return;

  const auto fileName = request.params().value_or(CodeActionParams()).textDocument().uri().toFilePath();

  const QString method(CodeActionRequest::methodName);
  if (const auto registered = m_dynamicCapabilities.isRegistered(method)) {
    if (!registered.value())
      return;
    const TextDocumentRegistrationOptions option(m_dynamicCapabilities.option(method).toObject());
    if (option.isValid() && !option.filterApplies(fileName))
      return;
  } else {
    const auto provider = m_serverCapabilities.codeActionProvider().value_or(false);
    if (!(Utils::holds_alternative<CodeActionOptions>(provider) || Utils::get<bool>(provider)))
      return;
  }

  sendContent(request);
}

auto Client::handleCodeActionResponse(const CodeActionRequest::Response &response, const DocumentUri &uri) -> void
{
  if (const auto &error = response.error())
    log(*error);
  if (const auto &_result = response.result()) {
    const auto &result = _result.value();
    if (const auto list = Utils::get_if<QList<Utils::variant<Command, CodeAction>>>(&result)) {
      for (const auto &item : *list) {
        if (const auto action = Utils::get_if<CodeAction>(&item))
          updateCodeActionRefactoringMarker(this, *action, uri);
        else if (const auto command = Utils::get_if<Command>(&item)) {
          Q_UNUSED(command) // todo
        }
      }
    }
  }
}

auto Client::executeCommand(const Command &command) -> void
{
  auto serverSupportsExecuteCommand = m_serverCapabilities.executeCommandProvider().has_value();
  serverSupportsExecuteCommand = m_dynamicCapabilities.isRegistered(ExecuteCommandRequest::methodName).value_or(serverSupportsExecuteCommand);
  if (serverSupportsExecuteCommand)
    sendContent(ExecuteCommandRequest(ExecuteCommandParams(command)));
}

auto Client::project() const -> ProjectExplorer::Project*
{
  return m_project;
}

auto Client::setCurrentProject(ProjectExplorer::Project *project) -> void
{
  if (m_project == project)
    return;
  if (m_project)
    m_project->disconnect(this);
  m_project = project;
  if (m_project) {
    connect(m_project, &ProjectExplorer::Project::destroyed, this, [this]() {
      // the project of the client should already be null since we expect the session and
      // the language client manager to reset it before it gets deleted.
      QTC_ASSERT(m_project == nullptr, projectClosed(m_project));
    });
  }
}

auto Client::projectOpened(ProjectExplorer::Project *project) -> void
{
  if (!sendWorkspceFolderChanges())
    return;
  WorkspaceFoldersChangeEvent event;
  event.setAdded({WorkSpaceFolder(DocumentUri::fromFilePath(project->projectDirectory()), project->displayName())});
  DidChangeWorkspaceFoldersParams params;
  params.setEvent(event);
  const DidChangeWorkspaceFoldersNotification change(params);
  sendContent(change);
}

auto Client::projectClosed(ProjectExplorer::Project *project) -> void
{
  if (sendWorkspceFolderChanges()) {
    WorkspaceFoldersChangeEvent event;
    event.setRemoved({WorkSpaceFolder(DocumentUri::fromFilePath(project->projectDirectory()), project->displayName())});
    DidChangeWorkspaceFoldersParams params;
    params.setEvent(event);
    const DidChangeWorkspaceFoldersNotification change(params);
    sendContent(change);
  }
  if (project == m_project) {
    if (m_state == Initialized) {
      shutdown();
    } else {
      m_state = Shutdown; // otherwise the manager would try to restart this server
      emit finished();
    }
    m_project = nullptr;
  }
}

auto Client::setSupportedLanguage(const LanguageFilter &filter) -> void
{
  m_languagFilter = filter;
}

auto Client::setActivateDocumentAutomatically(bool enabled) -> void
{
  m_activateDocAutomatically = enabled;
}

auto Client::setInitializationOptions(const QJsonObject &initializationOptions) -> void
{
  m_initializationOptions = initializationOptions;
}

auto Client::isSupportedDocument(const TextEditor::TextDocument *document) const -> bool
{
  QTC_ASSERT(document, return false);
  return m_languagFilter.isSupported(document);
}

auto Client::isSupportedFile(const Utils::FilePath &filePath, const QString &mimeType) const -> bool
{
  return m_languagFilter.isSupported(filePath, mimeType);
}

auto Client::isSupportedUri(const DocumentUri &uri) const -> bool
{
  const auto &filePath = uri.toFilePath();
  return m_languagFilter.isSupported(filePath, Utils::mimeTypeForFile(filePath).name());
}

auto Client::addAssistProcessor(TextEditor::IAssistProcessor *processor) -> void
{
  m_runningAssistProcessors.insert(processor);
}

auto Client::removeAssistProcessor(TextEditor::IAssistProcessor *processor) -> void
{
  m_runningAssistProcessors.remove(processor);
}

auto Client::diagnosticsAt(const DocumentUri &uri, const QTextCursor &cursor) const -> QList<Diagnostic>
{
  return m_diagnosticManager.diagnosticsAt(uri, cursor);
}

auto Client::hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri, const LanguageServerProtocol::Diagnostic &diag) const -> bool
{
  return m_diagnosticManager.hasDiagnostic(uri, documentForFilePath(uri.toFilePath()), diag);
}

auto Client::setDiagnosticsHandlers(const TextMarkCreator &textMarkCreator, const HideDiagnosticsHandler &hideHandler, const DiagnosticsFilter &filter) -> void
{
  m_diagnosticManager.setDiagnosticsHandlers(textMarkCreator, hideHandler, filter);
}

auto Client::setSemanticTokensHandler(const SemanticTokensHandler &handler) -> void
{
  m_tokenSupport.setTokensHandler(handler);
}

auto Client::setSymbolStringifier(const LanguageServerProtocol::SymbolStringifier &stringifier) -> void
{
  m_symbolStringifier = stringifier;
}

auto Client::symbolStringifier() const -> SymbolStringifier
{
  return m_symbolStringifier;
}

auto Client::setSnippetsGroup(const QString &group) -> void
{
  if (const auto provider = qobject_cast<LanguageClientCompletionAssistProvider*>(m_clientProviders.completionAssistProvider)) {
    provider->setSnippetsGroup(group);
  }
}

auto Client::setCompletionAssistProvider(LanguageClientCompletionAssistProvider *provider) -> void
{
  delete m_clientProviders.completionAssistProvider;
  m_clientProviders.completionAssistProvider = provider;
}

auto Client::start() -> void
{
  LanguageClientManager::addClient(this);
  if (m_clientInterface->start())
    LanguageClientManager::clientStarted(this);
  else
    LanguageClientManager::clientFinished(this);
}

auto Client::reset() -> bool
{
  if (!m_restartsLeft)
    return false;
  --m_restartsLeft;
  m_state = Uninitialized;
  m_responseHandlers.clear();
  m_clientInterface->resetBuffer();
  updateEditorToolBar(m_openedDocument.keys());
  m_serverCapabilities = ServerCapabilities();
  m_dynamicCapabilities.reset();
  m_diagnosticManager.clearDiagnostics();
  for (auto it = m_openedDocument.cbegin(); it != m_openedDocument.cend(); ++it)
    it.key()->disconnect(this);
  m_openedDocument.clear();
  // temporary container needed since m_resetAssistProvider is changed in resetAssistProviders
  for (const auto document : m_resetAssistProvider.keys())
    resetAssistProviders(document);
  for (const auto processor : qAsConst(m_runningAssistProcessors))
    processor->setAsyncProposalAvailable(nullptr);
  m_runningAssistProcessors.clear();
  qDeleteAll(m_documentHighlightsTimer);
  m_documentHighlightsTimer.clear();
  m_progressManager.reset();
  m_documentVersions.clear();
  return true;
}

auto Client::setError(const QString &message) -> void
{
  log(message);
  m_state = Error;
}

auto Client::setProgressTitleForToken(const LanguageServerProtocol::ProgressToken &token, const QString &message) -> void
{
  m_progressManager.setTitleForToken(token, message);
}

auto Client::handleMessage(const BaseMessage &message) -> void
{
  LanguageClientManager::logBaseMessage(LspLogMessage::ServerMessage, name(), message);
  if (const auto handler = m_contentHandler[message.mimeType]) {
    QString parseError;
    handler(message.content, message.codec, parseError, [this](const MessageId &id, const QByteArray &content, QTextCodec *codec) {
              this->handleResponse(id, content, codec);
            }, [this](const QString &method, const MessageId &id, const IContent *content) {
              this->handleMethod(method, id, content);
            });
    if (!parseError.isEmpty())
      log(parseError);
  } else {
    log(tr("Cannot handle content of type: %1").arg(QLatin1String(message.mimeType)));
  }
}

auto Client::log(const QString &message) const -> void
{
  switch (m_logTarget) {
  case LogTarget::Ui:
    Core::MessageManager::writeFlashing(QString("LanguageClient %1: %2").arg(name(), message));
    break;
  case LogTarget::Console: qCDebug(LOGLSPCLIENT) << message;
    break;
  }
}

auto Client::capabilities() const -> const ServerCapabilities&
{
  return m_serverCapabilities;
}

auto Client::dynamicCapabilities() const -> const DynamicCapabilities&
{
  return m_dynamicCapabilities;
}

auto Client::documentSymbolCache() -> DocumentSymbolCache*
{
  return &m_documentSymbolCache;
}

auto Client::hoverHandler() -> HoverHandler*
{
  return &m_hoverHandler;
}

auto Client::log(const ShowMessageParams &message) -> void
{
  log(message.toString());
}

auto Client::showMessageBox(const ShowMessageRequestParams &message) -> LanguageClientValue<MessageActionItem>
{
  const auto box = new QMessageBox();
  box->setText(message.toString());
  box->setAttribute(Qt::WA_DeleteOnClose);
  switch (message.type()) {
  case Error:
    box->setIcon(QMessageBox::Critical);
    break;
  case Warning:
    box->setIcon(QMessageBox::Warning);
    break;
  case Info:
    box->setIcon(QMessageBox::Information);
    break;
  case Log:
    box->setIcon(QMessageBox::NoIcon);
    break;
  }
  QHash<QAbstractButton*, MessageActionItem> itemForButton;
  if (const auto actions = message.actions()) {
    for (const auto &action : actions.value())
      itemForButton.insert(box->addButton(action.title(), QMessageBox::InvalidRole), action);
  }
  box->exec();
  const auto &item = itemForButton.value(box->clickedButton());
  return item.isValid() ? LanguageClientValue<MessageActionItem>(item) : LanguageClientValue<MessageActionItem>();
}

auto Client::resetAssistProviders(TextEditor::TextDocument *document) -> void
{
  const auto providers = m_resetAssistProvider.take(document);

  if (document->completionAssistProvider() == m_clientProviders.completionAssistProvider)
    document->setCompletionAssistProvider(providers.completionAssistProvider);

  if (document->functionHintAssistProvider() == m_clientProviders.functionHintProvider)
    document->setFunctionHintAssistProvider(providers.functionHintProvider);

  if (document->quickFixAssistProvider() == m_clientProviders.quickFixAssistProvider)
    document->setQuickFixAssistProvider(providers.quickFixAssistProvider);
}

auto Client::sendPostponedDocumentUpdates(Schedule semanticTokensSchedule) -> void
{
  m_documentUpdateTimer.stop();
  if (m_documentsToUpdate.empty())
    return;
  const auto currentWidget = TextEditor::TextEditorWidget::currentTextEditorWidget();

  struct DocumentUpdate {
    TextEditor::TextDocument *document;
    DidChangeTextDocumentNotification notification;
  };
  const auto updates = Utils::transform<QList<DocumentUpdate>>(m_documentsToUpdate, [this](const auto &elem) {
    TextEditor::TextDocument *const document = elem.first;
    const auto &filePath = document->filePath();
    const auto uri = DocumentUri::fromFilePath(filePath);
    VersionedTextDocumentIdentifier docId(uri);
    docId.setVersion(m_documentVersions[filePath]);
    DidChangeTextDocumentParams params;
    params.setTextDocument(docId);
    params.setContentChanges(elem.second);
    return DocumentUpdate{document, DidChangeTextDocumentNotification(params)};
  });
  m_documentsToUpdate.clear();

  for (const auto &update : updates) {
    sendContent(update.notification, SendDocUpdates::Ignore);
    emit documentUpdated(update.document);

    if (currentWidget && currentWidget->textDocument() == update.document)
      requestDocumentHighlights(currentWidget);

    switch (semanticTokensSchedule) {
    case Schedule::Now:
      m_tokenSupport.updateSemanticTokens(update.document);
      break;
    case Schedule::Delayed:
      QTimer::singleShot(m_documentUpdateTimer.interval(), this, [this, doc = QPointer(update.document)] {
        if (doc && m_documentsToUpdate.find(doc) == m_documentsToUpdate.end())
          m_tokenSupport.updateSemanticTokens(doc);
      });
      break;
    }
  }
}

auto Client::handleResponse(const MessageId &id, const QByteArray &content, QTextCodec *codec) -> void
{
  if (const auto handler = m_responseHandlers[id])
    handler(content, codec);
}

template <typename T>
static auto createInvalidParamsError(const QString &message) -> ResponseError<T>
{
  ResponseError<T> error;
  error.setMessage(message);
  error.setCode(ResponseError<T>::InvalidParams);
  return error;
}

auto Client::handleMethod(const QString &method, const MessageId &id, const IContent *content) -> void
{
  auto invalidParamsErrorMessage = [&](const JsonObject &params) {
    return tr("Invalid parameter in \"%1\":\n%2").arg(method, QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Indented)));
  };

  auto createDefaultResponse = [&]() -> IContent* {
    Response<std::nullptr_t, JsonObject> *response = nullptr;
    if (id.isValid()) {
      response = new Response<std::nullptr_t, JsonObject>(id);
      response->setResult(nullptr);
    }
    return response;
  };

  const auto isRequest = id.isValid();
  IContent *response = nullptr;

  if (method == PublishDiagnosticsNotification::methodName) {
    auto params = dynamic_cast<const PublishDiagnosticsNotification*>(content)->params().value_or(PublishDiagnosticsParams());
    if (params.isValid())
      handleDiagnostics(params);
    else
      log(invalidParamsErrorMessage(params));
  } else if (method == LogMessageNotification::methodName) {
    auto params = dynamic_cast<const LogMessageNotification*>(content)->params().value_or(LogMessageParams());
    if (params.isValid())
      log(params);
    else
      log(invalidParamsErrorMessage(params));
  } else if (method == ShowMessageNotification::methodName) {
    auto params = dynamic_cast<const ShowMessageNotification*>(content)->params().value_or(ShowMessageParams());
    if (params.isValid())
      log(params);
    else
      log(invalidParamsErrorMessage(params));
  } else if (method == ShowMessageRequest::methodName) {
    auto request = dynamic_cast<const ShowMessageRequest*>(content);
    auto showMessageResponse = new ShowMessageRequest::Response(id);
    auto params = request->params().value_or(ShowMessageRequestParams());
    if (params.isValid()) {
      showMessageResponse->setResult(showMessageBox(params));
    } else {
      const auto errorMessage = invalidParamsErrorMessage(params);
      log(errorMessage);
      showMessageResponse->setError(createInvalidParamsError<std::nullptr_t>(errorMessage));
    }
    response = showMessageResponse;
  } else if (method == RegisterCapabilityRequest::methodName) {
    auto params = dynamic_cast<const RegisterCapabilityRequest*>(content)->params().value_or(RegistrationParams());
    if (params.isValid()) {
      registerCapabilities(params.registrations());
      response = createDefaultResponse();
    } else {
      const auto errorMessage = invalidParamsErrorMessage(params);
      log(invalidParamsErrorMessage(params));
      auto registerResponse = new RegisterCapabilityRequest::Response(id);
      registerResponse->setError(createInvalidParamsError<std::nullptr_t>(errorMessage));
      response = registerResponse;
    }
  } else if (method == UnregisterCapabilityRequest::methodName) {
    auto params = dynamic_cast<const UnregisterCapabilityRequest*>(content)->params().value_or(UnregistrationParams());
    if (params.isValid()) {
      unregisterCapabilities(params.unregistrations());
      response = createDefaultResponse();
    } else {
      const auto errorMessage = invalidParamsErrorMessage(params);
      log(invalidParamsErrorMessage(params));
      auto registerResponse = new UnregisterCapabilityRequest::Response(id);
      registerResponse->setError(createInvalidParamsError<std::nullptr_t>(errorMessage));
      response = registerResponse;
    }
  } else if (method == ApplyWorkspaceEditRequest::methodName) {
    auto editResponse = new ApplyWorkspaceEditRequest::Response(id);
    auto params = dynamic_cast<const ApplyWorkspaceEditRequest*>(content)->params().value_or(ApplyWorkspaceEditParams());
    if (params.isValid()) {
      ApplyWorkspaceEditResult result;
      result.setApplied(applyWorkspaceEdit(this, params.edit()));
      editResponse->setResult(result);
    } else {
      const auto errorMessage = invalidParamsErrorMessage(params);
      log(errorMessage);
      editResponse->setError(createInvalidParamsError<std::nullptr_t>(errorMessage));
    }
    response = editResponse;
  } else if (method == WorkSpaceFolderRequest::methodName) {
    auto workSpaceFolderResponse = new WorkSpaceFolderRequest::Response(id);
    const auto projects = ProjectExplorer::SessionManager::projects();
    WorkSpaceFolderResult result;
    if (projects.isEmpty()) {
      result = nullptr;
    } else {
      result = Utils::transform(projects, [](ProjectExplorer::Project *project) {
        return WorkSpaceFolder(DocumentUri::fromFilePath(project->projectDirectory()), project->displayName());
      });
    }
    workSpaceFolderResponse->setResult(result);
    response = workSpaceFolderResponse;
  } else if (method == WorkDoneProgressCreateRequest::methodName) {
    response = createDefaultResponse();
  } else if (method == SemanticTokensRefreshRequest::methodName) {
    m_tokenSupport.refresh();
    response = createDefaultResponse();
  } else if (method == ProgressNotification::methodName) {
    if (auto params = dynamic_cast<const ProgressNotification*>(content)->params()) {
      if (!params->isValid())
        log(invalidParamsErrorMessage(*params));
      m_progressManager.handleProgress(*params);
      if (ProgressManager::isProgressEndMessage(*params)) emit workDone(params->token());
    }
  } else if (isRequest) {
    auto methodNotFoundResponse = new Response<JsonObject, JsonObject>(id);
    ResponseError<JsonObject> error;
    error.setCode(ResponseError<JsonObject>::MethodNotFound);
    methodNotFoundResponse->setError(error);
    response = methodNotFoundResponse;
  }

  // we got a request and handled it somewhere above but we missed to generate a response for it
  QTC_ASSERT(!isRequest || response, response = createDefaultResponse());

  if (response) {
    if (reachable()) {
      sendContent(*response);
    } else {
      qCDebug(LOGLSPCLIENT) << QString("Dropped response to request %1 id %2 for unreachable server %3").arg(method, id.toString(), name());
    }
    delete response;
  }
  delete content;
}

auto Client::handleDiagnostics(const PublishDiagnosticsParams &params) -> void
{
  const auto &uri = params.uri();

  const auto &diagnostics = params.diagnostics();
  m_diagnosticManager.setDiagnostics(uri, diagnostics, params.version());
  if (LanguageClientManager::clientForUri(uri) == this) {
    m_diagnosticManager.showDiagnostics(uri, m_documentVersions.value(uri.toFilePath()));
    if (m_autoRequestCodeActions)
      requestCodeActions(uri, diagnostics);
  }
}

auto Client::sendMessage(const BaseMessage &message) -> void
{
  LanguageClientManager::logBaseMessage(LspLogMessage::ClientMessage, name(), message);
  m_clientInterface->sendMessage(message);
}

auto Client::documentUpdatePostponed(const Utils::FilePath &fileName) const -> bool
{
  return Utils::contains(m_documentsToUpdate, [fileName](const auto &elem) {
    return elem.first->filePath() == fileName;
  });
}

auto Client::documentVersion(const Utils::FilePath &filePath) const -> int
{
  return m_documentVersions.value(filePath);
}

auto Client::setDocumentChangeUpdateThreshold(int msecs) -> void
{
  m_documentUpdateTimer.setInterval(msecs);
}

auto Client::initializeCallback(const InitializeRequest::Response &initResponse) -> void
{
  QTC_ASSERT(m_state == InitializeRequested, return);
  if (const auto error = initResponse.error()) {
    if (error.value().data().has_value() && error.value().data().value().retry()) {
      const auto title(tr("Language Server \"%1\" Initialize Error").arg(m_displayName));
      const auto result = QMessageBox::warning(Core::ICore::dialogParent(), title, error.value().message(), QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry);
      if (result == QMessageBox::Retry) {
        m_state = Uninitialized;
        initialize();
        return;
      }
    }
    setError(tr("Initialize error: ") + error.value().message());
    emit finished();
    return;
  }
  const auto &_result = initResponse.result();
  if (!_result.has_value()) {
    // continue on ill formed result
    log(tr("No initialize result."));
  } else {
    const auto &result = _result.value();
    if (!result.isValid()) {
      // continue on ill formed result
      log(QJsonDocument(result).toJson(QJsonDocument::Indented) + '\n' + tr("Initialize result is not valid"));
    }
    const auto serverInfo = result.serverInfo();
    if (serverInfo) {
      if (!serverInfo->isValid()) {
        log(QJsonDocument(result).toJson(QJsonDocument::Indented) + '\n' + tr("Server Info is not valid"));
      } else {
        m_serverName = serverInfo->name();
        if (const auto version = serverInfo->version())
          m_serverVersion = version.value();
      }
    }

    m_serverCapabilities = result.capabilities();
  }

  if (const auto completionProvider = qobject_cast<LanguageClientCompletionAssistProvider*>(m_clientProviders.completionAssistProvider)) {
    completionProvider->setTriggerCharacters(m_serverCapabilities.completionProvider().value_or(ServerCapabilities::CompletionOptions()).triggerCharacters());
  }
  if (const auto functionHintAssistProvider = qobject_cast<FunctionHintAssistProvider*>(m_clientProviders.functionHintProvider)) {
    functionHintAssistProvider->setTriggerCharacters(m_serverCapabilities.signatureHelpProvider().value_or(ServerCapabilities::SignatureHelpOptions()).triggerCharacters());
  }
  const auto tokenProvider = m_serverCapabilities.semanticTokensProvider().value_or(SemanticTokensOptions());
  if (tokenProvider.isValid())
    m_tokenSupport.setLegend(tokenProvider.legend());

  qCDebug(LOGLSPCLIENT) << "language server " << m_displayName << " initialized";
  m_state = Initialized;
  sendContent(InitializeNotification(InitializedParams()));
  const auto documentSymbolProvider = capabilities().documentSymbolProvider();
  if (documentSymbolProvider.has_value()) {
    if (!Utils::holds_alternative<bool>(*documentSymbolProvider) || Utils::get<bool>(*documentSymbolProvider)) {
      TextEditor::IOutlineWidgetFactory::updateOutline();
    }
  }

  for (const auto doc : m_postponedDocuments)
    openDocument(doc);
  m_postponedDocuments.clear();

  emit initialized(m_serverCapabilities);
}

auto Client::shutDownCallback(const ShutdownRequest::Response &shutdownResponse) -> void
{
  m_shutdownTimer.stop();
  QTC_ASSERT(m_state == ShutdownRequested, return);
  QTC_ASSERT(m_clientInterface, return);
  if (const auto error = shutdownResponse.error())
    log(*error);
  // directly send message otherwise the state check of sendContent would fail
  sendMessage(ExitNotification().toBaseMessage());
  qCDebug(LOGLSPCLIENT) << "language server " << m_displayName << " shutdown";
  m_state = Shutdown;
  m_shutdownTimer.start();
}

auto Client::sendWorkspceFolderChanges() const -> bool
{
  if (!reachable())
    return false;
  if (m_dynamicCapabilities.isRegistered(DidChangeWorkspaceFoldersNotification::methodName).value_or(false)) {
    return true;
  }
  if (const auto workspace = m_serverCapabilities.workspace()) {
    if (const auto folder = workspace.value().workspaceFolders()) {
      if (folder.value().supported().value_or(false)) {
        // holds either the Id for deregistration or whether it is registered
        const auto notification = folder.value().changeNotifications().value_or(false);
        return holds_alternative<QString>(notification) || (holds_alternative<bool>(notification) && get<bool>(notification));
      }
    }
  }
  return false;
}

auto Client::adjustedCursorForHighlighting(const QTextCursor &cursor, TextEditor::TextDocument *doc) -> QTextCursor
{
  Q_UNUSED(doc)
  return cursor;
}

} // namespace LanguageClient
