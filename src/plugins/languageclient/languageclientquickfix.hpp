// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <texteditor/codeassist/iassistprovider.hpp>
#include <texteditor/quickfix.hpp>

#include <languageserverprotocol/languagefeatures.h>

#include <QPointer>

namespace LanguageClient {

class Client;

class LANGUAGECLIENT_EXPORT CodeActionQuickFixOperation : public TextEditor::QuickFixOperation {
public:
  CodeActionQuickFixOperation(const LanguageServerProtocol::CodeAction &action, Client *client);
  auto perform() -> void override;

private:
  LanguageServerProtocol::CodeAction m_action;
  QPointer<Client> m_client;
};

class LANGUAGECLIENT_EXPORT LanguageClientQuickFixProvider : public TextEditor::IAssistProvider {
public:
  explicit LanguageClientQuickFixProvider(Client *client);
  auto runType() const -> IAssistProvider::RunType override;
  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;

private:
  Client *m_client = nullptr; // not owned
};

} // namespace LanguageClient
