// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclienthoverhandler.hpp"

#include "client.hpp"

#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/tooltip/tooltip.hpp>

using namespace LanguageServerProtocol;

namespace LanguageClient {

HoverHandler::HoverHandler(Client *client) : m_client(client) {}

HoverHandler::~HoverHandler()
{
  abort();
}

auto HoverHandler::abort() -> void
{
  if (m_client && m_client->reachable() && m_currentRequest.has_value())
    m_client->cancelRequest(*m_currentRequest);
  m_currentRequest.reset();
  m_response = {};
}

auto HoverHandler::setHelpItem(const LanguageServerProtocol::MessageId &msgId, const Orca::Plugin::Core::HelpItem &help) -> void
{
  if (msgId == m_response.id()) {
    setContent(m_response.result().value().content());
    m_response = {};
    setLastHelpItemIdentified(help);
    m_report(priority());
  }
}

auto HoverHandler::identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, TextEditor::BaseHoverHandler::ReportPriority report) -> void
{
  if (m_currentRequest.has_value())
    abort();
  if (m_client.isNull() || !m_client->documentOpen(editorWidget->textDocument()) || !m_client->reachable()) {
    report(Priority_None);
    return;
  }
  m_uri = DocumentUri::fromFilePath(editorWidget->textDocument()->filePath());
  m_response = {};
  auto tc = editorWidget->textCursor();
  tc.setPosition(pos);
  const auto &diagnostics = m_client->diagnosticsAt(m_uri, tc);
  if (!diagnostics.isEmpty()) {
    const auto messages = Utils::transform(diagnostics, &Diagnostic::message);
    setToolTip(messages.join('\n'));
    report(Priority_Diagnostic);
    return;
  }

  const auto &provider = m_client->capabilities().hoverProvider();
  auto sendMessage = provider.has_value();
  if (sendMessage && Utils::holds_alternative<bool>(*provider))
    sendMessage = Utils::get<bool>(*provider);
  if (const auto registered = m_client->dynamicCapabilities().isRegistered(HoverRequest::methodName)) {
    sendMessage = registered.value();
    if (sendMessage) {
      const TextDocumentRegistrationOptions option(m_client->dynamicCapabilities().option(HoverRequest::methodName).toObject());
      if (option.isValid()) {
        sendMessage = option.filterApplies(editorWidget->textDocument()->filePath(), Utils::mimeTypeForName(editorWidget->textDocument()->mimeType()));
      }
    }
  }
  if (!sendMessage) {
    report(Priority_None);
    return;
  }

  m_report = report;
  auto cursor = editorWidget->textCursor();
  cursor.setPosition(pos);
  HoverRequest request((TextDocumentPositionParams(TextDocumentIdentifier(m_uri), Position(cursor))));
  m_currentRequest = request.id();
  request.setResponseCallback([this](const HoverRequest::Response &response) { handleResponse(response); });
  m_client->sendContent(request);
}

auto HoverHandler::handleResponse(const HoverRequest::Response &response) -> void
{
  m_currentRequest.reset();
  if (const auto error = response.error()) {
    if (m_client)
      m_client->log(error.value());
  }
  if (const auto result = response.result()) {
    if (m_helpItemProvider) {
      m_response = response;
      m_helpItemProvider(response, m_uri);
      return;
    }
    setContent(result.value().content());
  }
  m_report(priority());
}

static auto toolTipForMarkedStrings(const QList<MarkedString> &markedStrings) -> QString
{
  QString tooltip;
  for (const auto &markedString : markedStrings) {
    if (!tooltip.isEmpty())
      tooltip += '\n';
    if (const auto string = Utils::get_if<QString>(&markedString))
      tooltip += *string;
    else if (const auto string = Utils::get_if<MarkedLanguageString>(&markedString))
      tooltip += string->value() + " [" + string->language() + ']';
  }
  return tooltip;
}

auto HoverHandler::setContent(const HoverContent &hoverContent) -> void
{
  if (const auto markupContent = Utils::get_if<MarkupContent>(&hoverContent))
    setToolTip(markupContent->content(), markupContent->textFormat());
  else if (auto markedString = Utils::get_if<MarkedString>(&hoverContent))
    setToolTip(toolTipForMarkedStrings({*markedString}));
  else if (const auto markedStrings = Utils::get_if<QList<MarkedString>>(&hoverContent))
    setToolTip(toolTipForMarkedStrings(*markedStrings));
}

} // namespace LanguageClient
