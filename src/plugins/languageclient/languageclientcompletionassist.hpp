// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/completion.h>
#include <texteditor/codeassist/assistproposaliteminterface.hpp>
#include <texteditor/codeassist/completionassistprovider.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>

#include <utils/optional.hpp>

#include <QPointer>

#include <functional>

namespace TextEditor {
class IAssistProposal;
class TextDocumentManipulatorInterface;
}

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT LanguageClientCompletionAssistProvider : public TextEditor::CompletionAssistProvider {
  Q_OBJECT

public:
  LanguageClientCompletionAssistProvider(Client *client);

  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;
  auto runType() const -> RunType override;
  auto activationCharSequenceLength() const -> int override;
  auto isActivationCharSequence(const QString &sequence) const -> bool override;
  auto isContinuationChar(const QChar &) const -> bool override { return true; }
  auto setTriggerCharacters(const Utils::optional<QList<QString>> triggerChars) -> void;
  auto setSnippetsGroup(const QString &group) -> void { m_snippetsGroup = group; }

protected:
  auto client() const -> Client* { return m_client; }

private:
  QList<QString> m_triggerChars;
  QString m_snippetsGroup;
  int m_activationCharSequenceLength = 0;
  Client *m_client = nullptr; // not owned
};

class LANGUAGECLIENT_EXPORT LanguageClientCompletionAssistProcessor : public TextEditor::IAssistProcessor {
public:
  LanguageClientCompletionAssistProcessor(Client *client, const QString &snippetsGroup);
  ~LanguageClientCompletionAssistProcessor() override;

  auto perform(const TextEditor::AssistInterface *interface) -> TextEditor::IAssistProposal* override;
  auto running() -> bool override;
  auto needsRestart() const -> bool override { return true; }
  auto cancel() -> void override;

protected:
  auto document() const -> QTextDocument*;
  auto filePath() const -> Utils::FilePath { return m_filePath; }
  auto basePos() const -> int { return m_basePos; }

  virtual auto generateCompletionItems(const QList<LanguageServerProtocol::CompletionItem> &items) const -> QList<TextEditor::AssistProposalItemInterface*>;

private:
  auto handleCompletionResponse(const LanguageServerProtocol::CompletionRequest::Response &response) -> void;

  QPointer<QTextDocument> m_document;
  Utils::FilePath m_filePath;
  QPointer<Client> m_client;
  Utils::optional<LanguageServerProtocol::MessageId> m_currentRequest;
  QMetaObject::Connection m_postponedUpdateConnection;
  const QString m_snippetsGroup;
  int m_pos = -1;
  int m_basePos = -1;
};

class LANGUAGECLIENT_EXPORT LanguageClientCompletionItem : public TextEditor::AssistProposalItemInterface {
public:
  LanguageClientCompletionItem(LanguageServerProtocol::CompletionItem item);

  // AssistProposalItemInterface interface
  auto text() const -> QString override;
  auto filterText() const -> QString override;
  auto implicitlyApplies() const -> bool override;
  auto prematurelyApplies(const QChar &typedCharacter) const -> bool override;
  auto apply(TextEditor::TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void override;
  auto icon() const -> QIcon override;
  auto detail() const -> QString override;
  auto isSnippet() const -> bool override;
  auto isValid() const -> bool override;
  auto hash() const -> quint64 override;
  auto item() const -> LanguageServerProtocol::CompletionItem;
  auto triggeredCommitCharacter() const -> QChar;
  auto sortText() const -> const QString&;
  auto hasSortText() const -> bool;
  auto operator <(const LanguageClientCompletionItem &other) const -> bool;
  auto isPerfectMatch(int pos, QTextDocument *doc) const -> bool;

private:
  LanguageServerProtocol::CompletionItem m_item;
  mutable QChar m_triggeredCommitCharacter;
  mutable QString m_sortText;
  mutable QString m_filterText;
};

} // namespace LanguageClient
