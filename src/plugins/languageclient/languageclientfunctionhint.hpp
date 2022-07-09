// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <texteditor/codeassist/completionassistprovider.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <utils/optional.hpp>

#include <QPointer>

namespace TextEditor {
class IAssistProposal;
}

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT FunctionHintAssistProvider : public TextEditor::CompletionAssistProvider {
  Q_OBJECT

public:
  explicit FunctionHintAssistProvider(Client *client);

  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;
  auto runType() const -> RunType override;
  auto activationCharSequenceLength() const -> int override;
  auto isActivationCharSequence(const QString &sequence) const -> bool override;
  auto isContinuationChar(const QChar &c) const -> bool override;
  auto setTriggerCharacters(const Utils::optional<QList<QString>> &triggerChars) -> void;

private:
  QList<QString> m_triggerChars;
  int m_activationCharSequenceLength = 0;
  Client *m_client = nullptr; // not owned
};

class LANGUAGECLIENT_EXPORT FunctionHintProcessor : public TextEditor::IAssistProcessor {
public:
  explicit FunctionHintProcessor(Client *client);
  auto perform(const TextEditor::AssistInterface *interface) -> TextEditor::IAssistProposal* override;
  auto running() -> bool override { return m_currentRequest.has_value(); }
  auto needsRestart() const -> bool override { return true; }
  auto cancel() -> void override;

private:
  auto handleSignatureResponse(const LanguageServerProtocol::SignatureHelpRequest::Response &response) -> void;

  QPointer<Client> m_client;
  Utils::optional<LanguageServerProtocol::MessageId> m_currentRequest;
  int m_pos = -1;
};

} // namespace LanguageClient
