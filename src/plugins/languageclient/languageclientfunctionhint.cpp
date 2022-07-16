// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientfunctionhint.hpp"
#include "client.hpp"

#include <languageserverprotocol/languagefeatures.h>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/functionhintproposal.hpp>
#include <texteditor/codeassist/iassistprocessor.hpp>
#include <texteditor/codeassist/ifunctionhintproposalmodel.hpp>

using namespace TextEditor;
using namespace LanguageServerProtocol;

namespace LanguageClient {

class FunctionHintProposalModel : public IFunctionHintProposalModel {
public:
  explicit FunctionHintProposalModel(SignatureHelp signature) : m_sigis(signature) {}
  auto reset() -> void override {}

  auto size() const -> int override
  {
    return m_sigis.signatures().size();
  }

  auto text(int index) const -> QString override;

  auto activeArgument(const QString &/*prefix*/) const -> int override
  {
    return m_sigis.activeParameter().value_or(0);
  }

private:
  LanguageServerProtocol::SignatureHelp m_sigis;
};

auto FunctionHintProposalModel::text(int index) const -> QString
{
  using Parameters = QList<ParameterInformation>;
  if (index < 0 || m_sigis.signatures().size() <= index)
    return {};
  const auto signature = m_sigis.signatures().at(index);
  auto parametersIndex = signature.activeParameter().value_or(-1);
  if (parametersIndex < 0) {
    if (index == m_sigis.activeSignature().value_or(-1))
      parametersIndex = m_sigis.activeParameter().value_or(-1);
  }
  auto label = signature.label();
  if (parametersIndex < 0)
    return label;

  const auto parameters = Utils::transform(signature.parameters().value_or(Parameters()), &ParameterInformation::label);
  if (parameters.size() <= parametersIndex)
    return label;

  const auto &parameterText = parameters.at(parametersIndex);
  const int start = label.indexOf(parameterText);
  const int end = start + parameterText.length();
  return label.mid(0, start).toHtmlEscaped() + "<b>" + parameterText.toHtmlEscaped() + "</b>" + label.mid(end).toHtmlEscaped();
}

FunctionHintProcessor::FunctionHintProcessor(Client *client) : m_client(client) {}

auto FunctionHintProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  QTC_ASSERT(m_client, return nullptr);
  m_pos = interface->position();
  QTextCursor cursor(interface->textDocument());
  cursor.setPosition(m_pos);
  const auto uri = DocumentUri::fromFilePath(interface->filePath());
  SignatureHelpRequest request((TextDocumentPositionParams(TextDocumentIdentifier(uri), Position(cursor))));
  request.setResponseCallback([this](auto response) { this->handleSignatureResponse(response); });
  m_client->addAssistProcessor(this);
  m_client->sendContent(request);
  m_currentRequest = request.id();
  return nullptr;
}

auto FunctionHintProcessor::cancel() -> void
{
  if (running()) {
    m_client->cancelRequest(m_currentRequest.value());
    m_client->removeAssistProcessor(this);
    m_currentRequest.reset();
  }
}

auto FunctionHintProcessor::handleSignatureResponse(const SignatureHelpRequest::Response &response) -> void
{
  m_currentRequest.reset();
  if (const auto error = response.error())
    m_client->log(error.value());
  m_client->removeAssistProcessor(this);
  const auto result = response.result().value_or(LanguageClientValue<SignatureHelp>());
  if (result.isNull()) {
    setAsyncProposalAvailable(nullptr);
    return;
  }
  const auto &signatureHelp = result.value();
  if (signatureHelp.signatures().isEmpty()) {
    setAsyncProposalAvailable(nullptr);
  } else {
    const FunctionHintProposalModelPtr model(new FunctionHintProposalModel(signatureHelp));
    setAsyncProposalAvailable(new FunctionHintProposal(m_pos, model));
  }
}

FunctionHintAssistProvider::FunctionHintAssistProvider(Client *client) : CompletionAssistProvider(client), m_client(client) {}

auto FunctionHintAssistProvider::createProcessor(const AssistInterface *) const -> TextEditor::IAssistProcessor*
{
  return new FunctionHintProcessor(m_client);
}

auto FunctionHintAssistProvider::runType() const -> IAssistProvider::RunType
{
  return Asynchronous;
}

auto FunctionHintAssistProvider::activationCharSequenceLength() const -> int
{
  return m_activationCharSequenceLength;
}

auto FunctionHintAssistProvider::isActivationCharSequence(const QString &sequence) const -> bool
{
  return Utils::anyOf(m_triggerChars, [sequence](const QString &trigger) { return trigger.endsWith(sequence); });
}

auto FunctionHintAssistProvider::isContinuationChar(const QChar &/*c*/) const -> bool
{
  return true;
}

auto FunctionHintAssistProvider::setTriggerCharacters(const Utils::optional<QList<QString>> &triggerChars) -> void
{
  m_triggerChars = triggerChars.value_or(QList<QString>());
  for (const auto &trigger : qAsConst(m_triggerChars)) {
    if (trigger.length() > m_activationCharSequenceLength)
      m_activationCharSequenceLength = trigger.length();
  }
}

} // namespace LanguageClient
