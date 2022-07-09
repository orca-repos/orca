// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <texteditor/basehoverhandler.hpp>

#include <functional>

namespace LanguageClient {

class Client;

using HelpItemProvider = std::function<void(const LanguageServerProtocol::HoverRequest::Response &, const LanguageServerProtocol::DocumentUri &uri)>;

class LANGUAGECLIENT_EXPORT HoverHandler final : public TextEditor::BaseHoverHandler {
  Q_DECLARE_TR_FUNCTIONS(HoverHandler)

public:
  explicit HoverHandler(Client *client);
  ~HoverHandler() override;

  auto abort() -> void override;
  auto setHelpItemProvider(const HelpItemProvider &provider) -> void { m_helpItemProvider = provider; }
  auto setHelpItem(const LanguageServerProtocol::MessageId &msgId, const Core::HelpItem &help) -> void;

protected:
  auto identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report) -> void override;

private:
  auto handleResponse(const LanguageServerProtocol::HoverRequest::Response &response) -> void;
  auto setContent(const LanguageServerProtocol::HoverContent &content) -> void;

  QPointer<Client> m_client;
  Utils::optional<LanguageServerProtocol::MessageId> m_currentRequest;
  LanguageServerProtocol::DocumentUri m_uri;
  LanguageServerProtocol::HoverRequest::Response m_response;
  TextEditor::BaseHoverHandler::ReportPriority m_report;
  HelpItemProvider m_helpItemProvider;
};

} // namespace LanguageClient
