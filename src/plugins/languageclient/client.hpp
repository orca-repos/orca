// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "diagnosticmanager.hpp"
#include "documentsymbolcache.hpp"
#include "dynamiccapabilities.hpp"
#include "languageclient_global.hpp"
#include "languageclientcompletionassist.hpp"
#include "languageclientformatter.hpp"
#include "languageclientfunctionhint.hpp"
#include "languageclienthoverhandler.hpp"
#include "languageclientquickfix.hpp"
#include "languageclientsettings.hpp"
#include "languageclientsymbolsupport.hpp"
#include "progressmanager.hpp"
#include "semantichighlightsupport.hpp"

#include <core/messagemanager.hpp>

#include <utils/id.hpp>
#include <utils/link.hpp>

#include <languageserverprotocol/client.h>
#include <languageserverprotocol/diagnostics.h>
#include <languageserverprotocol/initializemessages.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/messages.h>
#include <languageserverprotocol/progresssupport.h>
#include <languageserverprotocol/semantictokens.h>
#include <languageserverprotocol/shutdownmessages.h>
#include <languageserverprotocol/textsynchronization.h>

#include <texteditor/semantichighlighter.hpp>

#include <QBuffer>
#include <QHash>
#include <QJsonDocument>
#include <QTextCursor>

#include <unordered_map>
#include <utility>

namespace Core {
class IDocument;
}

namespace ProjectExplorer {
class Project;
}

namespace TextEditor
{
class IAssistProcessor;
class TextDocument;
class TextEditorWidget;
}

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace LanguageClient {

class BaseClientInterface;

class LANGUAGECLIENT_EXPORT Client : public QObject {
  Q_OBJECT

public:
  explicit Client(BaseClientInterface *clientInterface); // takes ownership
  ~Client() override;
  Client(const Client &) = delete;
  Client(Client &&) = delete;

  auto operator=(const Client &) -> Client& = delete;
  auto operator=(Client &&) -> Client& = delete;

  // basic properties
  auto id() const -> Utils::Id { return m_id; }
  auto setName(const QString &name) -> void { m_displayName = name; }
  auto name() const -> QString;

  enum class SendDocUpdates {
    Send,
    Ignore
  };

  auto sendContent(const LanguageServerProtocol::IContent &content, SendDocUpdates sendUpdates = SendDocUpdates::Send) -> void;
  auto cancelRequest(const LanguageServerProtocol::MessageId &id) -> void;

  // server state handling
  auto start() -> void;
  auto setInitializationOptions(const QJsonObject &initializationOptions) -> void;
  auto initialize() -> void;
  auto reset() -> bool;
  auto shutdown() -> void;

  enum State {
    Uninitialized,
    InitializeRequested,
    Initialized,
    ShutdownRequested,
    Shutdown,
    Error
  };

  auto state() const -> State;
  auto stateString() const -> QString;
  auto reachable() const -> bool { return m_state == Initialized; }

  // capabilities
  static auto defaultClientCapabilities() -> LanguageServerProtocol::ClientCapabilities;
  auto setClientCapabilities(const LanguageServerProtocol::ClientCapabilities &caps) -> void;
  auto capabilities() const -> const LanguageServerProtocol::ServerCapabilities&;
  auto serverName() const -> QString { return m_serverName; }
  auto serverVersion() const -> QString { return m_serverVersion; }
  auto dynamicCapabilities() const -> const DynamicCapabilities&;
  auto registerCapabilities(const QList<LanguageServerProtocol::Registration> &registrations) -> void;
  auto unregisterCapabilities(const QList<LanguageServerProtocol::Unregistration> &unregistrations) -> void;
  auto setLocatorsEnabled(bool enabled) -> void { m_locatorsEnabled = enabled; }
  auto locatorsEnabled() const -> bool { return m_locatorsEnabled; }
  auto setAutoRequestCodeActions(bool enabled) -> void { m_autoRequestCodeActions = enabled; }

  // document synchronization
  auto setSupportedLanguage(const LanguageFilter &filter) -> void;
  auto setActivateDocumentAutomatically(bool enabled) -> void;
  auto isSupportedDocument(const TextEditor::TextDocument *document) const -> bool;
  auto isSupportedFile(const Utils::FilePath &filePath, const QString &mimeType) const -> bool;
  auto isSupportedUri(const LanguageServerProtocol::DocumentUri &uri) const -> bool;
  auto openDocument(TextEditor::TextDocument *document) -> void;
  auto closeDocument(TextEditor::TextDocument *document) -> void;
  auto activateDocument(TextEditor::TextDocument *document) -> void;
  auto deactivateDocument(TextEditor::TextDocument *document) -> void;
  auto documentOpen(const TextEditor::TextDocument *document) const -> bool;
  auto documentForFilePath(const Utils::FilePath &file) const -> TextEditor::TextDocument*;
  auto documentContentsSaved(TextEditor::TextDocument *document) -> void;
  auto documentWillSave(Core::IDocument *document) -> void;
  auto documentContentsChanged(TextEditor::TextDocument *document, int position, int charsRemoved, int charsAdded) -> void;
  auto cursorPositionChanged(TextEditor::TextEditorWidget *widget) -> void;
  auto documentUpdatePostponed(const Utils::FilePath &fileName) const -> bool;
  auto documentVersion(const Utils::FilePath &filePath) const -> int;
  auto setDocumentChangeUpdateThreshold(int msecs) -> void;

  // workspace control
  virtual auto setCurrentProject(ProjectExplorer::Project *project) -> void;
  auto project() const -> ProjectExplorer::Project*;
  virtual auto projectOpened(ProjectExplorer::Project *project) -> void;
  virtual auto projectClosed(ProjectExplorer::Project *project) -> void;

  // commands
  auto requestCodeActions(const LanguageServerProtocol::DocumentUri &uri, const QList<LanguageServerProtocol::Diagnostic> &diagnostics) -> void;
  auto requestCodeActions(const LanguageServerProtocol::CodeActionRequest &request) -> void;
  auto handleCodeActionResponse(const LanguageServerProtocol::CodeActionRequest::Response &response, const LanguageServerProtocol::DocumentUri &uri) -> void;
  virtual auto executeCommand(const LanguageServerProtocol::Command &command) -> void;

  // language support
  auto addAssistProcessor(TextEditor::IAssistProcessor *processor) -> void;
  auto removeAssistProcessor(TextEditor::IAssistProcessor *processor) -> void;
  auto symbolSupport() -> SymbolSupport&;
  auto documentSymbolCache() -> DocumentSymbolCache*;
  auto hoverHandler() -> HoverHandler*;
  auto diagnosticsAt(const LanguageServerProtocol::DocumentUri &uri, const QTextCursor &cursor) const -> QList<LanguageServerProtocol::Diagnostic>;
  auto hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri, const LanguageServerProtocol::Diagnostic &diag) const -> bool;
  auto setDiagnosticsHandlers(const TextMarkCreator &textMarkCreator, const HideDiagnosticsHandler &hideHandler, const DiagnosticsFilter &filter) -> void;
  auto setSemanticTokensHandler(const SemanticTokensHandler &handler) -> void;
  auto setSymbolStringifier(const LanguageServerProtocol::SymbolStringifier &stringifier) -> void;
  auto symbolStringifier() const -> LanguageServerProtocol::SymbolStringifier;
  auto setSnippetsGroup(const QString &group) -> void;
  auto setCompletionAssistProvider(LanguageClientCompletionAssistProvider *provider) -> void;

  // logging
  enum class LogTarget {
    Console,
    Ui
  };

  auto setLogTarget(LogTarget target) -> void { m_logTarget = target; }
  auto log(const QString &message) const -> void;

  template <typename Error>
  auto log(const LanguageServerProtocol::ResponseError<Error> &responseError) const -> void { log(responseError.toString()); }

  // Caller takes ownership.
  using CustomInspectorTab = std::pair<QWidget*, QString>;
  using CustomInspectorTabs = QList<CustomInspectorTab>;
  virtual auto createCustomInspectorTabs() -> const CustomInspectorTabs { return {}; }

signals:
  auto initialized(const LanguageServerProtocol::ServerCapabilities &capabilities) -> void;
  auto capabilitiesChanged(const DynamicCapabilities &capabilities) -> void;
  auto documentUpdated(TextEditor::TextDocument *document) -> void;
  auto workDone(const LanguageServerProtocol::ProgressToken &token) -> void;
  auto finished() -> void;

protected:
  auto setError(const QString &message) -> void;
  auto setProgressTitleForToken(const LanguageServerProtocol::ProgressToken &token, const QString &message) -> void;
  auto handleMessage(const LanguageServerProtocol::BaseMessage &message) -> void;
  virtual auto handleDiagnostics(const LanguageServerProtocol::PublishDiagnosticsParams &params) -> void;

private:
  auto sendMessage(const LanguageServerProtocol::BaseMessage &message) -> void;
  auto handleResponse(const LanguageServerProtocol::MessageId &id, const QByteArray &content, QTextCodec *codec) -> void;
  auto handleMethod(const QString &method, const LanguageServerProtocol::MessageId &id, const LanguageServerProtocol::IContent *content) -> void;
  auto initializeCallback(const LanguageServerProtocol::InitializeRequest::Response &initResponse) -> void;
  auto shutDownCallback(const LanguageServerProtocol::ShutdownRequest::Response &shutdownResponse) -> void;
  auto sendWorkspceFolderChanges() const -> bool;
  auto log(const LanguageServerProtocol::ShowMessageParams &message) -> void;
  auto showMessageBox(const LanguageServerProtocol::ShowMessageRequestParams &message) -> LanguageServerProtocol::LanguageClientValue<LanguageServerProtocol::MessageActionItem>;
  auto removeDiagnostics(const LanguageServerProtocol::DocumentUri &uri) -> void;
  auto resetAssistProviders(TextEditor::TextDocument *document) -> void;
  auto sendPostponedDocumentUpdates(Schedule semanticTokensSchedule) -> void;
  auto updateCompletionProvider(TextEditor::TextDocument *document) -> void;
  auto updateFunctionHintProvider(TextEditor::TextDocument *document) -> void;
  auto requestDocumentHighlights(TextEditor::TextEditorWidget *widget) -> void;
  auto requestDocumentHighlightsNow(TextEditor::TextEditorWidget *widget) -> void;
  auto supportedSemanticRequests(TextEditor::TextDocument *document) const -> LanguageServerProtocol::SemanticRequestTypes;
  auto handleSemanticTokens(const LanguageServerProtocol::SemanticTokens &tokens) -> void;
  auto documentClosed(Core::IDocument *document) -> void;

  virtual auto handleDocumentClosed(TextEditor::TextDocument *) -> void {}
  virtual auto handleDocumentOpened(TextEditor::TextDocument *) -> void {}
  virtual auto adjustedCursorForHighlighting(const QTextCursor &cursor, TextEditor::TextDocument *doc) -> QTextCursor;

  using ContentHandler = std::function<void(const QByteArray &, QTextCodec *, QString &, LanguageServerProtocol::ResponseHandlers, LanguageServerProtocol::MethodHandler)>;

  State m_state = Uninitialized;
  QHash<LanguageServerProtocol::MessageId, LanguageServerProtocol::ResponseHandler::Callback> m_responseHandlers;
  QHash<QByteArray, ContentHandler> m_contentHandler;
  QString m_displayName;
  LanguageFilter m_languagFilter;
  QJsonObject m_initializationOptions;
  QMap<TextEditor::TextDocument*, QString> m_openedDocument;
  QSet<TextEditor::TextDocument*> m_postponedDocuments;
  QMap<Utils::FilePath, int> m_documentVersions;
  std::unordered_map<TextEditor::TextDocument*, QList<LanguageServerProtocol::DidChangeTextDocumentParams::TextDocumentContentChangeEvent>> m_documentsToUpdate;
  QMap<TextEditor::TextEditorWidget*, QTimer*> m_documentHighlightsTimer;
  QTimer m_documentUpdateTimer;
  Utils::Id m_id;
  LanguageServerProtocol::ClientCapabilities m_clientCapabilities = defaultClientCapabilities();
  LanguageServerProtocol::ServerCapabilities m_serverCapabilities;
  DynamicCapabilities m_dynamicCapabilities;

  struct AssistProviders {
    QPointer<TextEditor::CompletionAssistProvider> completionAssistProvider;
    QPointer<TextEditor::CompletionAssistProvider> functionHintProvider;
    QPointer<TextEditor::IAssistProvider> quickFixAssistProvider;
  };

  AssistProviders m_clientProviders;
  QMap<TextEditor::TextDocument*, AssistProviders> m_resetAssistProvider;
  QHash<TextEditor::TextEditorWidget*, LanguageServerProtocol::MessageId> m_highlightRequests;
  int m_restartsLeft = 5;
  QScopedPointer<BaseClientInterface> m_clientInterface;
  DiagnosticManager m_diagnosticManager;
  DocumentSymbolCache m_documentSymbolCache;
  HoverHandler m_hoverHandler;
  QHash<LanguageServerProtocol::DocumentUri, TextEditor::HighlightingResults> m_highlights;
  ProjectExplorer::Project *m_project = nullptr;
  QSet<TextEditor::IAssistProcessor*> m_runningAssistProcessors;
  SymbolSupport m_symbolSupport;
  ProgressManager m_progressManager;
  bool m_activateDocAutomatically = false;
  SemanticTokenSupport m_tokenSupport;
  QString m_serverName;
  QString m_serverVersion;
  LanguageServerProtocol::SymbolStringifier m_symbolStringifier;
  LogTarget m_logTarget = LogTarget::Ui;
  bool m_locatorsEnabled = true;
  bool m_autoRequestCodeActions = true;
  QTimer m_shutdownTimer;
};

} // namespace LanguageClient
