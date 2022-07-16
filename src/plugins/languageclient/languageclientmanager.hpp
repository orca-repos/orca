// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "client.hpp"
#include "languageclient_global.hpp"
#include "languageclientsettings.hpp"
#include "locatorfilter.hpp"
#include "lspinspector.hpp"

#include <utils/algorithm.hpp>
#include <utils/id.hpp>

#include <languageserverprotocol/diagnostics.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/textsynchronization.h>

namespace Orca::Plugin::Core {
class IEditor;
class IDocument;
}

namespace ProjectExplorer {
class Project;
}

namespace LanguageClient {

class LanguageClientMark;

class LANGUAGECLIENT_EXPORT LanguageClientManager : public QObject {
  Q_OBJECT

public:
  LanguageClientManager(const LanguageClientManager &other) = delete;
  LanguageClientManager(LanguageClientManager &&other) = delete;
  ~LanguageClientManager() override;

  static auto init() -> void;
  static auto clientStarted(Client *client) -> void;
  static auto clientFinished(Client *client) -> void;
  static auto startClient(BaseSettings *setting, ProjectExplorer::Project *project = nullptr) -> Client*;
  static auto clients() -> QVector<Client*>;
  static auto addClient(Client *client) -> void;
  static auto addExclusiveRequest(const LanguageServerProtocol::MessageId &id, Client *client) -> void;
  static auto reportFinished(const LanguageServerProtocol::MessageId &id, Client *byClient) -> void;
  static auto shutdownClient(Client *client) -> void;
  static auto deleteClient(Client *client) -> void;
  static auto shutdown() -> void;
  static auto instance() -> LanguageClientManager*;
  static auto clientsSupportingDocument(const TextEditor::TextDocument *doc) -> QList<Client*>;
  static auto applySettings() -> void;
  static auto currentSettings() -> QList<BaseSettings*>;
  static auto registerClientSettings(BaseSettings *settings) -> void;
  static auto enableClientSettings(const QString &settingsId) -> void;
  static auto clientForSetting(const BaseSettings *setting) -> QVector<Client*>;
  static auto settingForClient(Client *setting) -> const BaseSettings*;
  static auto clientForDocument(TextEditor::TextDocument *document) -> Client*;
  static auto clientForFilePath(const Utils::FilePath &filePath) -> Client*;
  static auto clientForUri(const LanguageServerProtocol::DocumentUri &uri) -> Client*;
  static auto clientsForProject(const ProjectExplorer::Project *project) -> const QList<Client*>;

  template <typename T>
  static auto hasClients() -> bool;

  ///
  /// \brief openDocumentWithClient
  /// makes sure the document is opened and activated with the client and
  /// deactivates the document for a potential previous active client
  ///
  static auto openDocumentWithClient(TextEditor::TextDocument *document, Client *client) -> void;
  static auto logBaseMessage(const LspLogMessage::MessageSender sender, const QString &clientName, const LanguageServerProtocol::BaseMessage &message) -> void;
  static auto showInspector() -> void;

signals:
  auto clientRemoved(Client *client) -> void;
  auto shutdownFinished() -> void;

private:
  LanguageClientManager(QObject *parent);

  auto editorOpened(Orca::Plugin::Core::IEditor *editor) -> void;
  auto documentOpened(Orca::Plugin::Core::IDocument *document) -> void;
  auto documentClosed(Orca::Plugin::Core::IDocument *document) -> void;
  auto documentContentsSaved(Orca::Plugin::Core::IDocument *document) -> void;
  auto documentWillSave(Orca::Plugin::Core::IDocument *document) -> void;
  auto updateProject(ProjectExplorer::Project *project) -> void;
  auto projectAdded(ProjectExplorer::Project *project) -> void;
  auto reachableClients() -> QVector<Client*>;

  bool m_shuttingDown = false;
  QVector<Client*> m_clients;
  QList<BaseSettings*> m_currentSettings; // owned
  QMap<QString, QVector<Client*>> m_clientsForSetting;
  QHash<TextEditor::TextDocument*, QPointer<Client>> m_clientForDocument;
  QHash<LanguageServerProtocol::MessageId, QList<Client*>> m_exclusiveRequests;
  DocumentLocatorFilter m_currentDocumentLocatorFilter;
  WorkspaceLocatorFilter m_workspaceLocatorFilter;
  WorkspaceClassLocatorFilter m_workspaceClassLocatorFilter;
  WorkspaceMethodLocatorFilter m_workspaceMethodLocatorFilter;
  LspInspector m_inspector;
};

template <typename T>
auto LanguageClientManager::hasClients() -> bool
{
  return Utils::contains(instance()->m_clients, [](const Client *c) {
    return qobject_cast<const T*>(c);
  });
}

} // namespace LanguageClient
