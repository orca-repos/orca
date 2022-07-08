// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "iassistprocessor.hpp"

using namespace TextEditor;

/*!
    \class TextEditor::IAssistProcessor
    \brief The IAssistProcessor class acts as an interface that actually computes an assist
    proposal.
    \ingroup CodeAssist

    \sa IAssistProposal, IAssistProvider
*/

IAssistProcessor::IAssistProcessor() = default;
IAssistProcessor::~IAssistProcessor() = default;

auto IAssistProcessor::setAsyncProposalAvailable(IAssistProposal *proposal) -> void
{
  if (m_asyncCompletionsAvailableHandler)
    m_asyncCompletionsAvailableHandler(proposal);
}

auto IAssistProcessor::setAsyncCompletionAvailableHandler(const AsyncCompletionsAvailableHandler &handler) -> void
{
  m_asyncCompletionsAvailableHandler = handler;
}

/*!
    \fn IAssistProposal *TextEditor::IAssistProcessor::perform(const AssistInterface *interface)

    Computes a proposal and returns it. Access to the document is made through the \a interface.
    If this is an asynchronous processor the \a interface will be detached.

    The processor takes ownership of the interface. Also, one should be careful in the case of
    sharing data across asynchronous processors since there might be more than one instance of
    them computing a proposal at a particular time.

    \sa AssistInterface::detach()
*/
