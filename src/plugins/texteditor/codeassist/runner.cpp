// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runner.hpp"
#include "iassistprocessor.hpp"
#include "iassistproposal.hpp"
#include "assistinterface.hpp"

using namespace TextEditor;
using namespace Internal;

ProcessorRunner::ProcessorRunner() = default;

ProcessorRunner::~ProcessorRunner()
{
  delete m_processor;
  if (m_discardProposal && m_proposal)
    delete m_proposal;
}

auto ProcessorRunner::setProcessor(IAssistProcessor *computer) -> void
{
  m_processor = computer;
}

auto ProcessorRunner::run() -> void
{
  m_interface->recreateTextDocument();
  m_proposal = m_processor->perform(m_interface);
}

auto ProcessorRunner::proposal() const -> IAssistProposal*
{
  return m_proposal;
}

auto ProcessorRunner::setDiscardProposal(bool discard) -> void
{
  m_discardProposal = discard;
}

auto ProcessorRunner::setAssistInterface(AssistInterface *interface) -> void
{
  m_interface = interface;
}
