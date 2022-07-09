// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientmanager.hpp"

#include "languageclientplugin.hpp"
#include "languageclientutils.hpp"

#include <core/editormanager/editormanager.hpp>
#include <core/editormanager/ieditor.hpp>
#include <core/find/searchresultwindow.hpp>
#include <core/icore.hpp>
#include <languageserverprotocol/messages.h>
#include <languageserverprotocol/progresssupport.h>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/session.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/textmark.hpp>
#include <utils/algorithm.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/theme/theme.hpp>
#include <utils/utilsicons.hpp>

#include <QTextBlock>
#include <QTimer>

using namespace LanguageServerProtocol;

namespace LanguageClient {

static Q_LOGGING_CATEGORY(Log, "qtc.languageclient.manager", QtWarningMsg)

static LanguageClientManager *managerInstance = nullptr;

LanguageClientManager::LanguageClientManager(QObject *parent) : QObject(parent)
{
  using namespace Core;
  using namespace ProjectExplorer;
  JsonRpcMessageHandler::registerMessageProvider<PublishDiagnosticsNotification>();
  JsonRpcMessageHandler::registerMessageProvider<ApplyWorkspaceEditRequest>();
  JsonRpcMessageHandler::registerMessageProvider<LogMessageNotification>();
  JsonRpcMessageHandler::registerMessageProvider<ShowMessageRequest>();
  JsonRpcMessageHandler::registerMessageProvider<ShowMessageNotification>();
  JsonRpcMessageHandler::registerMessageProvider<WorkSpaceFolderRequest>();
  JsonRpcMessageHandler::registerMessageProvider<RegisterCapabilityRequest>();
  JsonRpcMessageHandler::registerMessageProvider<UnregisterCapabilityRequest>();
  JsonRpcMessageHandler::registerMessageProvider<WorkDoneProgressCreateRequest>();
  JsonRpcMessageHandler::registerMessageProvider<ProgressNotification>();
  JsonRpcMessageHandler::registerMessageProvider<SemanticTokensRefreshRequest>();
  connect(EditorManager::instance(), &EditorManager::editorOpened, this, &LanguageClientManager::editorOpened);
  connect(EditorManager::instance(), &EditorManager::documentOpened, this, &LanguageClientManager::documentOpened);
  connect(EditorManager::instance(), &EditorManager::documentClosed, this, &LanguageClientManager::documentClosed);
  connect(EditorManager::instance(), &EditorManager::saved, this, &LanguageClientManager::documentContentsSaved);
  connect(EditorManager::instance(), &EditorManager::aboutToSave, this, &LanguageClientManager::documentWillSave);
  connect(SessionManager::instance(), &SessionManager::projectAdded, this, &LanguageClientManager::projectAdded);
  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, [&](Project *project) { project->disconnect(this); });
}

LanguageClientManager::~LanguageClientManager()
{
  QTC_ASSERT(m_clients.isEmpty(), qDeleteAll(m_clients));
  qDeleteAll(m_currentSettings);
  managerInstance = nullptr;
}

auto LanguageClientManager::init() -> void
{
  if (managerInstance)
    return;
  QTC_ASSERT(LanguageClientPlugin::instance(), return);
  managerInstance = new LanguageClientManager(LanguageClientPlugin::instance());
}

auto LanguageClient::LanguageClientManager::addClient(Client *client) -> void
{
  QTC_ASSERT(managerInstance, return);
  QTC_ASSERT(client, return);

  if (managerInstance->m_clients.contains(client))
    return;

  qCDebug(Log) << "add client: " << client->name() << client;
  managerInstance->m_clients << client;
  connect(client, &Client::finished, managerInstance, [client]() { clientFinished(client); });
  connect(client, &Client::initialized, managerInstance, [client](const LanguageServerProtocol::ServerCapabilities &capabilities) {
    managerInstance->m_currentDocumentLocatorFilter.updateCurrentClient();
    managerInstance->m_inspector.clientInitialized(client->name(), capabilities);
  });
  connect(client, &Client::capabilitiesChanged, managerInstance, [client](const DynamicCapabilities &capabilities) {
    managerInstance->m_inspector.updateCapabilities(client->name(), capabilities);
  });
}

auto LanguageClientManager::clientStarted(Client *client) -> void
{
  qCDebug(Log) << "client started: " << client->name() << client;
  QTC_ASSERT(managerInstance, return);
  QTC_ASSERT(client, return);
  if (managerInstance->m_shuttingDown) {
    clientFinished(client);
    return;
  }
  client->initialize();
  const auto &clientDocs = managerInstance->m_clientForDocument.keys(client);
  for (const auto document : clientDocs)
    client->openDocument(document);
}

auto LanguageClientManager::clientFinished(Client *client) -> void
{
  QTC_ASSERT(managerInstance, return);
  constexpr auto restartTimeoutS = 5;
  const auto unexpectedFinish = client->state() != Client::Shutdown && client->state() != Client::ShutdownRequested;

  if (unexpectedFinish) {
    if (!managerInstance->m_shuttingDown) {
      const auto &clientDocs = managerInstance->m_clientForDocument.keys(client);
      if (client->reset()) {
        qCDebug(Log) << "restart unexpectedly finished client: " << client->name() << client;
        client->log(tr("Unexpectedly finished. Restarting in %1 seconds.").arg(restartTimeoutS));
        QTimer::singleShot(restartTimeoutS * 1000, client, [client]() { client->start(); });
        for (const auto document : clientDocs)
          client->deactivateDocument(document);
        return;
      }
      qCDebug(Log) << "client finished unexpectedly: " << client->name() << client;
      client->log(tr("Unexpectedly finished."));
      for (auto document : clientDocs)
        managerInstance->m_clientForDocument.remove(document);
    }
  }
  deleteClient(client);
  if (managerInstance->m_shuttingDown && managerInstance->m_clients.isEmpty()) emit managerInstance->shutdownFinished();
}

auto LanguageClientManager::startClient(BaseSettings *setting, ProjectExplorer::Project *project) -> Client*
{
  QTC_ASSERT(managerInstance, return nullptr);
  QTC_ASSERT(setting, return nullptr);
  QTC_ASSERT(setting->isValid(), return nullptr);
  const auto client = setting->createClient(project);
  qCDebug(Log) << "start client: " << client->name() << client;
  QTC_ASSERT(client, return nullptr);
  client->start();
  managerInstance->m_clientsForSetting[setting->m_id].append(client);
  return client;
}

auto LanguageClientManager::clients() -> QVector<Client*>
{
  QTC_ASSERT(managerInstance, return {});
  return managerInstance->m_clients;
}

auto LanguageClientManager::addExclusiveRequest(const MessageId &id, Client *client) -> void
{
  QTC_ASSERT(managerInstance, return);
  managerInstance->m_exclusiveRequests[id] << client;
}

auto LanguageClientManager::reportFinished(const MessageId &id, Client *byClient) -> void
{
  QTC_ASSERT(managerInstance, return);
  for (const auto client : qAsConst(managerInstance->m_exclusiveRequests[id])) {
    if (client != byClient)
      client->cancelRequest(id);
  }
  managerInstance->m_exclusiveRequests.remove(id);
}

auto LanguageClientManager::shutdownClient(Client *client) -> void
{
  if (!client)
    return;
  qCDebug(Log) << "request client shutdown: " << client->name() << client;
  // reset the documents for that client already when requesting the shutdown so they can get
  // reassigned to another server right after this request to another server
  for (auto document : managerInstance->m_clientForDocument.keys(client))
    managerInstance->m_clientForDocument.remove(document);
  if (client->reachable())
    client->shutdown();
  else if (client->state() != Client::Shutdown && client->state() != Client::ShutdownRequested)
    deleteClient(client);
}

auto LanguageClientManager::deleteClient(Client *client) -> void
{
  QTC_ASSERT(managerInstance, return);
  QTC_ASSERT(client, return);
  qCDebug(Log) << "delete client: " << client->name() << client;
  client->disconnect(managerInstance);
  managerInstance->m_clients.removeAll(client);
  for (auto &clients : managerInstance->m_clientsForSetting)
    clients.removeAll(client);
  if (managerInstance->m_shuttingDown) {
    delete client;
  } else {
    client->deleteLater();
    emit instance()->clientRemoved(client);
  }
}

auto LanguageClientManager::shutdown() -> void
{
  QTC_ASSERT(managerInstance, return);
  if (managerInstance->m_shuttingDown)
    return;
  qCDebug(Log) << "shutdown manager";
  managerInstance->m_shuttingDown = true;
  const auto clients = managerInstance->clients();
  for (const auto client : clients)
    shutdownClient(client);
  QTimer::singleShot(3000, managerInstance, [] {
    const auto clients = managerInstance->clients();
    for (const auto client : clients)
      deleteClient(client);
    emit managerInstance->shutdownFinished();
  });
}

auto LanguageClientManager::instance() -> LanguageClientManager*
{
  return managerInstance;
}

auto LanguageClientManager::clientsSupportingDocument(const TextEditor::TextDocument *doc) -> QList<Client*>
{
  QTC_ASSERT(managerInstance, return {});
  QTC_ASSERT(doc, return {};);
  return Utils::filtered(managerInstance->reachableClients(), [doc](Client *client) {
    return client->isSupportedDocument(doc);
  }).toList();
}

auto LanguageClientManager::applySettings() -> void
{
  QTC_ASSERT(managerInstance, return);
  qDeleteAll(managerInstance->m_currentSettings);
  managerInstance->m_currentSettings = Utils::transform(LanguageClientSettings::pageSettings(), &BaseSettings::copy);
  const auto restarts = LanguageClientSettings::changedSettings();
  LanguageClientSettings::toSettings(Core::ICore::settings(), managerInstance->m_currentSettings);

  for (const auto setting : restarts) {
    QList<TextEditor::TextDocument*> documents;
    const auto currentClients = clientForSetting(setting);
    for (const auto client : currentClients) {
      documents << managerInstance->m_clientForDocument.keys(client);
      shutdownClient(client);
    }
    for (auto document : qAsConst(documents))
      managerInstance->m_clientForDocument.remove(document);
    if (!setting->isValid() || !setting->m_enabled)
      continue;
    switch (setting->m_startBehavior) {
    case BaseSettings::AlwaysOn: {
      const auto client = startClient(setting);
      for (auto document : qAsConst(documents))
        managerInstance->m_clientForDocument[document] = client;
      break;
    }
    case BaseSettings::RequiresFile: {
      const auto &openedDocuments = Core::DocumentModel::openedDocuments();
      for (const auto document : openedDocuments) {
        if (const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document)) {
          if (setting->m_languageFilter.isSupported(document))
            documents << textDocument;
        }
      }
      if (!documents.isEmpty()) {
        const auto client = startClient(setting);
        for (const auto document : qAsConst(documents))
          client->openDocument(document);
      }
      break;
    }
    case BaseSettings::RequiresProject: {
      const auto &openedDocuments = Core::DocumentModel::openedDocuments();
      QHash<ProjectExplorer::Project*, Client*> clientForProject;
      for (const auto document : openedDocuments) {
        const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document);
        if (!textDocument || !setting->m_languageFilter.isSupported(textDocument))
          continue;
        const auto filePath = textDocument->filePath();
        for (auto project : ProjectExplorer::SessionManager::projects()) {
          if (project->isKnownFile(filePath)) {
            auto client = clientForProject[project];
            if (!client) {
              client = startClient(setting, project);
              if (!client)
                continue;
              clientForProject[project] = client;
            }
            client->openDocument(textDocument);
          }
        }
      }
      break;
    }
    default:
      break;
    }
  }
}

auto LanguageClientManager::currentSettings() -> QList<BaseSettings*>
{
  QTC_ASSERT(managerInstance, return {});
  return managerInstance->m_currentSettings;
}

auto LanguageClientManager::registerClientSettings(BaseSettings *settings) -> void
{
  QTC_ASSERT(managerInstance, return);
  LanguageClientSettings::addSettings(settings);
  managerInstance->applySettings();
}

auto LanguageClientManager::enableClientSettings(const QString &settingsId) -> void
{
  QTC_ASSERT(managerInstance, return);
  LanguageClientSettings::enableSettings(settingsId);
  managerInstance->applySettings();
}

auto LanguageClientManager::clientForSetting(const BaseSettings *setting) -> QVector<Client*>
{
  QTC_ASSERT(managerInstance, return {});
  const auto instance = managerInstance;
  return instance->m_clientsForSetting.value(setting->m_id);
}

auto LanguageClientManager::settingForClient(Client *client) -> const BaseSettings*
{
  QTC_ASSERT(managerInstance, return nullptr);
  for (auto it = managerInstance->m_clientsForSetting.cbegin(); it != managerInstance->m_clientsForSetting.cend(); ++it) {
    const auto &id = it.key();
    for (const Client *settingClient : it.value()) {
      if (settingClient == client) {
        return Utils::findOrDefault(managerInstance->m_currentSettings, [id](BaseSettings *setting) {
          return setting->m_id == id;
        });
      }
    }
  }
  return nullptr;
}

auto LanguageClientManager::clientForDocument(TextEditor::TextDocument *document) -> Client*
{
  QTC_ASSERT(managerInstance, return nullptr);
  return document == nullptr ? nullptr : managerInstance->m_clientForDocument.value(document).data();
}

auto LanguageClientManager::clientForFilePath(const Utils::FilePath &filePath) -> Client*
{
  return clientForDocument(TextEditor::TextDocument::textDocumentForFilePath(filePath));
}

auto LanguageClientManager::clientForUri(const DocumentUri &uri) -> Client*
{
  return clientForFilePath(uri.toFilePath());
}

auto LanguageClientManager::clientsForProject(const ProjectExplorer::Project *project) -> const QList<Client*>
{
  return Utils::filtered(managerInstance->m_clients, [project](const Client *c) {
    return c->project() == project;
  }).toList();
}

auto LanguageClientManager::openDocumentWithClient(TextEditor::TextDocument *document, Client *client) -> void
{
  if (!document)
    return;
  const auto currentClient = clientForDocument(document);
  if (client == currentClient)
    return;
  if (currentClient)
    currentClient->deactivateDocument(document);
  managerInstance->m_clientForDocument[document] = client;
  if (client) {
    qCDebug(Log) << "open" << document->filePath() << "with" << client->name() << client;
    if (!client->documentOpen(document))
      client->openDocument(document);
    else
      client->activateDocument(document);
  }
  TextEditor::IOutlineWidgetFactory::updateOutline();
}

auto LanguageClientManager::logBaseMessage(const LspLogMessage::MessageSender sender, const QString &clientName, const BaseMessage &message) -> void
{
  instance()->m_inspector.log(sender, clientName, message);
}

auto LanguageClientManager::showInspector() -> void
{
  QString clientName;
  if (const auto client = clientForDocument(TextEditor::TextDocument::currentTextDocument()))
    clientName = client->name();
  const auto inspectorWidget = instance()->m_inspector.createWidget(clientName);
  inspectorWidget->setAttribute(Qt::WA_DeleteOnClose);
  Core::ICore::registerWindow(inspectorWidget, Core::Context("LanguageClient.Inspector"));
  inspectorWidget->show();
}

auto LanguageClientManager::reachableClients() -> QVector<Client*>
{
  return Utils::filtered(m_clients, &Client::reachable);
}

auto LanguageClientManager::editorOpened(Core::IEditor *editor) -> void
{
  using namespace TextEditor;
  if (const auto *textEditor = qobject_cast<BaseTextEditor*>(editor)) {
    if (auto widget = textEditor->editorWidget()) {
      connect(widget, &TextEditorWidget::requestLinkAt, this, [document = textEditor->textDocument()](const QTextCursor &cursor, Utils::ProcessLinkCallback &callback, bool resolveTarget) {
        if (const auto client = clientForDocument(document))
          client->symbolSupport().findLinkAt(document, cursor, callback, resolveTarget);
      });
      connect(widget, &TextEditorWidget::requestUsages, this, [document = textEditor->textDocument()](const QTextCursor &cursor) {
        if (const auto client = clientForDocument(document))
          client->symbolSupport().findUsages(document, cursor);
      });
      connect(widget, &TextEditorWidget::requestRename, this, [document = textEditor->textDocument()](const QTextCursor &cursor) {
        if (const auto client = clientForDocument(document))
          client->symbolSupport().renameSymbol(document, cursor);
      });
      connect(widget, &TextEditorWidget::cursorPositionChanged, this, [widget]() {
        if (const auto client = clientForDocument(widget->textDocument()))
          if (client->reachable())
            client->cursorPositionChanged(widget);
      });
      updateEditorToolBar(editor);
      if (const auto document = textEditor->textDocument()) {
        if (Client *client = m_clientForDocument[document])
          widget->addHoverHandler(client->hoverHandler());
      }
    }
  }
}

auto LanguageClientManager::documentOpened(Core::IDocument *document) -> void
{
  const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document);
  if (!textDocument)
    return;

  // check whether we have to start servers for this document
  const auto settings = currentSettings();
  for (const auto setting : settings) {
    if (setting->isValid() && setting->m_enabled && setting->m_languageFilter.isSupported(document)) {
      auto clients = clientForSetting(setting);
      if (setting->m_startBehavior == BaseSettings::RequiresProject) {
        const auto &filePath = document->filePath();
        for (auto project : ProjectExplorer::SessionManager::projects()) {
          // check whether file is part of this project
          if (!project->isKnownFile(filePath))
            continue;

          // check whether we already have a client running for this project
          auto clientForProject = Utils::findOrDefault(clients, [project](Client *client) {
            return client->project() == project;
          });
          if (!clientForProject)
            clientForProject = startClient(setting, project);

          QTC_ASSERT(clientForProject, continue);
          openDocumentWithClient(textDocument, clientForProject);
          // Since we already opened the document in this client we remove the client
          // from the list of clients that receive the openDocument call
          clients.removeAll(clientForProject);
        }
      } else if (setting->m_startBehavior == BaseSettings::RequiresFile && clients.isEmpty()) {
        clients << startClient(setting);
      }
      for (const auto client : qAsConst(clients))
        client->openDocument(textDocument);
    }
  }
}

auto LanguageClientManager::documentClosed(Core::IDocument *document) -> void
{
  if (const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document))
    m_clientForDocument.remove(textDocument);
}

auto LanguageClientManager::documentContentsSaved(Core::IDocument *document) -> void
{
  if (const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document)) {
    const auto &clients = reachableClients();
    for (const auto client : clients)
      client->documentContentsSaved(textDocument);
  }
}

auto LanguageClientManager::documentWillSave(Core::IDocument *document) -> void
{
  if (const auto textDocument = qobject_cast<TextEditor::TextDocument*>(document)) {
    const auto &clients = reachableClients();
    for (const auto client : clients)
      client->documentWillSave(textDocument);
  }
}

auto LanguageClientManager::updateProject(ProjectExplorer::Project *project) -> void
{
  for (const auto setting : qAsConst(m_currentSettings)) {
    if (setting->isValid() && setting->m_enabled && setting->m_startBehavior == BaseSettings::RequiresProject) {
      if (Utils::findOrDefault(clientForSetting(setting), [project](const QPointer<Client> &client) {
        return client->project() == project;
      }) == nullptr) {
        Client *newClient = nullptr;
        const auto &openedDocuments = Core::DocumentModel::openedDocuments();
        for (const auto doc : openedDocuments) {
          if (setting->m_languageFilter.isSupported(doc) && project->isKnownFile(doc->filePath())) {
            if (const auto textDoc = qobject_cast<TextEditor::TextDocument*>(doc)) {
              if (!newClient)
                newClient = startClient(setting, project);
              if (!newClient)
                break;
              newClient->openDocument(textDoc);
            }
          }
        }
      }
    }
  }
}

auto LanguageClientManager::projectAdded(ProjectExplorer::Project *project) -> void
{
  connect(project, &ProjectExplorer::Project::fileListChanged, this, [this, project]() {
    updateProject(project);
  });
  const auto &clients = reachableClients();
  for (const auto client : clients)
    client->projectOpened(project);
}

} // namespace LanguageClient
