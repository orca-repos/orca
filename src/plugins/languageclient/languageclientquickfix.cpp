// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientquickfix.hpp"

#include "client.hpp"
#include "languageclientutils.hpp"

#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/quickfix.hpp>


using namespace LanguageServerProtocol;
using namespace TextEditor;

namespace LanguageClient {

CodeActionQuickFixOperation::CodeActionQuickFixOperation(const CodeAction &action, Client *client) : m_action(action), m_client(client)
{
  setDescription(action.title());
}

auto CodeActionQuickFixOperation::perform() -> void
{
  if (!m_client)
    return;
  if (const auto edit = m_action.edit())
    applyWorkspaceEdit(m_client, *edit);
  else if (const auto command = m_action.command())
    m_client->executeCommand(*command);
}

class CommandQuickFixOperation : public QuickFixOperation {
public:
  CommandQuickFixOperation(const Command &command, Client *client) : m_command(command), m_client(client)
  {
    setDescription(command.title());
  }

  auto perform() -> void override
  {
    if (m_client)
      m_client->executeCommand(m_command);
  }

private:
  Command m_command;
  QPointer<Client> m_client;
};

class LanguageClientQuickFixAssistProcessor : public IAssistProcessor {
public:
  explicit LanguageClientQuickFixAssistProcessor(Client *client) : m_client(client) {}
  auto running() -> bool override { return m_currentRequest.has_value(); }
  auto perform(const AssistInterface *interface) -> IAssistProposal* override;
  auto cancel() -> void override;

private:
  auto handleCodeActionResponse(const CodeActionRequest::Response &response) -> void;

  QSharedPointer<const AssistInterface> m_assistInterface;
  Client *m_client = nullptr; // not owned
  Utils::optional<MessageId> m_currentRequest;
};

auto LanguageClientQuickFixAssistProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  m_assistInterface = QSharedPointer<const AssistInterface>(interface);

  CodeActionParams params;
  params.setContext({});
  QTextCursor cursor(interface->textDocument());
  cursor.setPosition(interface->position());
  if (cursor.atBlockEnd() || cursor.atBlockStart())
    cursor.select(QTextCursor::LineUnderCursor);
  else
    cursor.select(QTextCursor::WordUnderCursor);
  if (!cursor.hasSelection())
    cursor.select(QTextCursor::LineUnderCursor);
  const Range range(cursor);
  params.setRange(range);
  const auto uri = DocumentUri::fromFilePath(interface->filePath());
  params.setTextDocument(TextDocumentIdentifier(uri));
  CodeActionParams::CodeActionContext context;
  context.setDiagnostics(m_client->diagnosticsAt(uri, cursor));
  params.setContext(context);

  CodeActionRequest request(params);
  request.setResponseCallback([this](const CodeActionRequest::Response &response) {
    handleCodeActionResponse(response);
  });

  m_client->addAssistProcessor(this);
  m_client->requestCodeActions(request);
  m_currentRequest = request.id();
  return nullptr;
}

auto LanguageClientQuickFixAssistProcessor::cancel() -> void
{
  if (running()) {
    m_client->cancelRequest(m_currentRequest.value());
    m_client->removeAssistProcessor(this);
    m_currentRequest.reset();
  }
}

auto LanguageClientQuickFixAssistProcessor::handleCodeActionResponse(const CodeActionRequest::Response &response) -> void
{
  m_currentRequest.reset();
  if (const auto &error = response.error())
    m_client->log(*error);
  QuickFixOperations ops;
  if (const auto &_result = response.result()) {
    const auto &result = _result.value();
    if (const auto list = Utils::get_if<QList<Utils::variant<Command, CodeAction>>>(&result)) {
      for (const auto &item : *list) {
        if (const auto action = Utils::get_if<CodeAction>(&item))
          ops << new CodeActionQuickFixOperation(*action, m_client);
        else if (const auto command = Utils::get_if<Command>(&item))
          ops << new CommandQuickFixOperation(*command, m_client);
      }
    }
  }
  m_client->removeAssistProcessor(this);
  setAsyncProposalAvailable(GenericProposal::createProposal(m_assistInterface.data(), ops));
}

LanguageClientQuickFixProvider::LanguageClientQuickFixProvider(Client *client) : IAssistProvider(client), m_client(client)
{
  QTC_CHECK(client);
}

auto LanguageClientQuickFixProvider::runType() const -> IAssistProvider::RunType
{
  return Asynchronous;
}

auto LanguageClientQuickFixProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new LanguageClientQuickFixAssistProcessor(m_client);
}

} // namespace LanguageClient
