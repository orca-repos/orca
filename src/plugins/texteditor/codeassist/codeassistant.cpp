// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codeassistant.hpp"
#include "completionassistprovider.hpp"
#include "iassistprocessor.hpp"
#include "iassistproposal.hpp"
#include "iassistproposalwidget.hpp"
#include "assistinterface.hpp"
#include "assistproposalitem.hpp"
#include "runner.hpp"
#include "textdocumentmanipulator.hpp"

#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/completionsettings.hpp>

#include <core/editormanager/editormanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/qtcassert.hpp>

#include <QKeyEvent>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>

using namespace TextEditor::Internal;

namespace TextEditor {

class CodeAssistantPrivate : public QObject {
public:
  CodeAssistantPrivate(CodeAssistant *assistant);

  auto configure(TextEditorWidget *editorWidget) -> void;
  auto isConfigured() const -> bool;
  auto invoke(AssistKind kind, IAssistProvider *provider = nullptr) -> void;
  auto process() -> void;
  auto requestProposal(AssistReason reason, AssistKind kind, IAssistProvider *provider = nullptr) -> void;
  auto cancelCurrentRequest() -> void;
  auto invalidateCurrentRequestData() -> void;
  auto displayProposal(IAssistProposal *newProposal, AssistReason reason) -> void;
  auto isDisplayingProposal() const -> bool;
  auto isWaitingForProposal() const -> bool;
  auto notifyChange() -> void;
  auto hasContext() const -> bool;
  auto destroyContext() -> void;
  auto userData() const -> QVariant;
  auto setUserData(const QVariant &data) -> void;
  auto identifyActivationSequence() -> CompletionAssistProvider*;
  auto stopAutomaticProposalTimer() -> void;
  auto startAutomaticProposalTimer() -> void;
  auto automaticProposalTimeout() -> void;
  auto clearAbortedPosition() -> void;
  auto updateFromCompletionSettings(const CompletionSettings &settings) -> void;
  auto eventFilter(QObject *o, QEvent *e) -> bool override;

private:
  auto requestActivationCharProposal() -> bool;
  auto processProposalItem(AssistProposalItemInterface *proposalItem) -> void;
  auto handlePrefixExpansion(const QString &newPrefix) -> void;
  auto finalizeProposal() -> void;
  auto explicitlyAborted() -> void;
  auto isDestroyEvent(int key, const QString &keyText) -> bool;

  CodeAssistant *q = nullptr;
  TextEditorWidget *m_editorWidget = nullptr;
  ProcessorRunner *m_requestRunner = nullptr;
  QMetaObject::Connection m_runnerConnection;
  IAssistProvider *m_requestProvider = nullptr;
  IAssistProcessor *m_asyncProcessor = nullptr;
  AssistKind m_assistKind = Completion;
  IAssistProposalWidget *m_proposalWidget = nullptr;
  QScopedPointer<IAssistProposal> m_proposal;
  bool m_receivedContentWhileWaiting = false;
  QTimer m_automaticProposalTimer;
  CompletionSettings m_settings;
  int m_abortedBasePosition = -1;
  static const QChar m_null;
  QVariant m_userData;
};

const QChar CodeAssistantPrivate::m_null;

CodeAssistantPrivate::CodeAssistantPrivate(CodeAssistant *assistant) : q(assistant)
{
  m_automaticProposalTimer.setSingleShot(true);
  connect(&m_automaticProposalTimer, &QTimer::timeout, this, &CodeAssistantPrivate::automaticProposalTimeout);

  updateFromCompletionSettings(TextEditorSettings::completionSettings());
  connect(TextEditorSettings::instance(), &TextEditorSettings::completionSettingsChanged, this, &CodeAssistantPrivate::updateFromCompletionSettings);

  connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, this, &CodeAssistantPrivate::clearAbortedPosition);
}

auto CodeAssistantPrivate::configure(TextEditorWidget *editorWidget) -> void
{
  m_editorWidget = editorWidget;
  m_editorWidget->installEventFilter(this);
}

auto CodeAssistantPrivate::isConfigured() const -> bool
{
  return m_editorWidget != nullptr;
}

auto CodeAssistantPrivate::invoke(AssistKind kind, IAssistProvider *provider) -> void
{
  if (!isConfigured())
    return;

  stopAutomaticProposalTimer();

  if (isDisplayingProposal() && m_assistKind == kind && !m_proposal->isFragile()) {
    m_proposalWidget->setReason(ExplicitlyInvoked);
    m_proposalWidget->updateProposal(m_editorWidget->textAt(m_proposal->basePosition(), m_editorWidget->position() - m_proposal->basePosition()));
  } else {
    requestProposal(ExplicitlyInvoked, kind, provider);
  }
}

auto CodeAssistantPrivate::requestActivationCharProposal() -> bool
{
  if (m_editorWidget->multiTextCursor().hasMultipleCursors())
    return false;
  if (m_assistKind == Completion && m_settings.m_completionTrigger != ManualCompletion) {
    if (const auto provider = identifyActivationSequence()) {
      requestProposal(ActivationCharacter, Completion, provider);
      return true;
    }
  }
  return false;
}

auto CodeAssistantPrivate::process() -> void
{
  if (!isConfigured())
    return;

  stopAutomaticProposalTimer();

  if (m_assistKind == Completion) {
    if (!requestActivationCharProposal())
      startAutomaticProposalTimer();
  } else if (m_assistKind != FunctionHint) {
    m_assistKind = Completion;
  }
}

auto CodeAssistantPrivate::requestProposal(AssistReason reason, AssistKind kind, IAssistProvider *provider) -> void
{
  // make sure to cleanup old proposals if we cannot find a new assistant
  Utils::ExecuteOnDestruction earlyReturnContextClear([this]() { destroyContext(); });
  if (isWaitingForProposal())
    cancelCurrentRequest();

  if (!provider) {
    if (kind == Completion)
      provider = m_editorWidget->textDocument()->completionAssistProvider();
    else if (kind == FunctionHint)
      provider = m_editorWidget->textDocument()->functionHintAssistProvider();
    else
      provider = m_editorWidget->textDocument()->quickFixAssistProvider();

    if (!provider)
      return;
  }

  const auto assistInterface = m_editorWidget->createAssistInterface(kind, reason);
  if (!assistInterface)
    return;

  // We got an assist provider and interface so no need to reset the current context anymore
  earlyReturnContextClear.reset({});

  m_assistKind = kind;
  m_requestProvider = provider;
  auto processor = provider->createProcessor(assistInterface);

  switch (provider->runType()) {
  case IAssistProvider::Synchronous: {
    if (const auto newProposal = processor->perform(assistInterface))
      displayProposal(newProposal, reason);
    delete processor;
    break;
  }
  case IAssistProvider::AsynchronousWithThread: {
    if (const auto newProposal = processor->immediateProposal(assistInterface))
      displayProposal(newProposal, reason);

    m_requestRunner = new ProcessorRunner;
    m_runnerConnection = connect(m_requestRunner, &ProcessorRunner::finished, this, [this, reason]() {
      // Since the request runner is a different thread, there's still a gap in which the
      // queued signal could be processed after an invalidation of the current request.
      if (!m_requestRunner || m_requestRunner != sender())
        return;

      const auto proposal = m_requestRunner->proposal();
      invalidateCurrentRequestData();
      displayProposal(proposal, reason);
      emit q->finished();
    });
    connect(m_requestRunner, &ProcessorRunner::finished, m_requestRunner, &ProcessorRunner::deleteLater);
    assistInterface->prepareForAsyncUse();
    m_requestRunner->setProcessor(processor);
    m_requestRunner->setAssistInterface(assistInterface);
    m_requestRunner->start();
    break;
  }
  case IAssistProvider::Asynchronous: {
    processor->setAsyncCompletionAvailableHandler([this, reason, processor](IAssistProposal *newProposal) {
      if (!processor->running()) {
        // do not delete this processor directly since this function is called from within the processor
        QMetaObject::invokeMethod(QCoreApplication::instance(), [processor]() {
          delete processor;
        }, Qt::QueuedConnection);
      }
      if (processor != m_asyncProcessor)
        return;
      invalidateCurrentRequestData();
      if (processor && processor->needsRestart() && m_receivedContentWhileWaiting) {
        delete newProposal;
        m_receivedContentWhileWaiting = false;
        requestProposal(reason, m_assistKind, m_requestProvider);
      } else {
        displayProposal(newProposal, reason);
        if (processor && processor->running())
          m_asyncProcessor = processor;
        else emit q->finished();
      }
    });

    // If there is a proposal, nothing asynchronous happened...
    if (const auto newProposal = processor->perform(assistInterface)) {
      displayProposal(newProposal, reason);
      delete processor;
    } else if (!processor->running()) {
      delete processor;
    } else {
      // ...async request was triggered
      if (const auto newProposal = processor->immediateProposal(assistInterface))
        displayProposal(newProposal, reason);
      QTC_CHECK(!m_asyncProcessor);
      m_asyncProcessor = processor;
    }

    break;
  }
  } // switch
}

auto CodeAssistantPrivate::cancelCurrentRequest() -> void
{
  if (m_requestRunner) {
    m_requestRunner->setDiscardProposal(true);
    disconnect(m_runnerConnection);
  }
  if (m_asyncProcessor) {
    m_asyncProcessor->cancel();
    delete m_asyncProcessor;
  }
  invalidateCurrentRequestData();
}

auto CodeAssistantPrivate::displayProposal(IAssistProposal *newProposal, AssistReason reason) -> void
{
  if (!newProposal)
    return;

  // TODO: The proposal should own the model until someone takes it explicitly away.
  QScopedPointer proposalCandidate(newProposal);

  if (isDisplayingProposal() && !m_proposal->isFragile())
    return;

  auto basePosition = proposalCandidate->basePosition();
  if (m_editorWidget->position() < basePosition) {
    destroyContext();
    return;
  }

  if (m_abortedBasePosition == basePosition && reason != ExplicitlyInvoked) {
    destroyContext();
    return;
  }

  const auto prefix = m_editorWidget->textAt(basePosition, m_editorWidget->position() - basePosition);
  if (!newProposal->hasItemsToPropose(prefix, reason)) {
    if (newProposal->isCorrective(m_editorWidget))
      newProposal->makeCorrection(m_editorWidget);
    destroyContext();
    return;
  }

  if (m_proposalWidget && basePosition == proposalCandidate->basePosition() && m_proposalWidget->supportsModelUpdate(proposalCandidate->id())) {
    m_proposal.reset(proposalCandidate.take());
    m_proposalWidget->updateModel(m_proposal->model());
    m_proposalWidget->updateProposal(prefix);
    return;
  }

  destroyContext();

  clearAbortedPosition();
  m_proposal.reset(proposalCandidate.take());

  if (m_proposal->isCorrective(m_editorWidget))
    m_proposal->makeCorrection(m_editorWidget);

  m_editorWidget->keepAutoCompletionHighlight(true);
  basePosition = m_proposal->basePosition();
  m_proposalWidget = m_proposal->createWidget();
  connect(m_proposalWidget, &QObject::destroyed, this, &CodeAssistantPrivate::finalizeProposal);
  connect(m_proposalWidget, &IAssistProposalWidget::prefixExpanded, this, &CodeAssistantPrivate::handlePrefixExpansion);
  connect(m_proposalWidget, &IAssistProposalWidget::proposalItemActivated, this, &CodeAssistantPrivate::processProposalItem);
  connect(m_proposalWidget, &IAssistProposalWidget::explicitlyAborted, this, &CodeAssistantPrivate::explicitlyAborted);
  m_proposalWidget->setAssistant(q);
  m_proposalWidget->setReason(reason);
  m_proposalWidget->setKind(m_assistKind);
  m_proposalWidget->setBasePosition(basePosition);
  m_proposalWidget->setUnderlyingWidget(m_editorWidget);
  m_proposalWidget->setModel(m_proposal->model());
  m_proposalWidget->setDisplayRect(m_editorWidget->cursorRect(basePosition));
  m_proposalWidget->setIsSynchronized(!m_receivedContentWhileWaiting);
  m_proposalWidget->showProposal(prefix);
}

auto CodeAssistantPrivate::processProposalItem(AssistProposalItemInterface *proposalItem) -> void
{
  QTC_ASSERT(m_proposal, return);
  TextDocumentManipulator manipulator(m_editorWidget);
  proposalItem->apply(manipulator, m_proposal->basePosition());
  destroyContext();
  m_editorWidget->encourageApply();
  if (!proposalItem->isSnippet())
    requestActivationCharProposal();
}

auto CodeAssistantPrivate::handlePrefixExpansion(const QString &newPrefix) -> void
{
  QTC_ASSERT(m_proposal, return);

  QTextCursor cursor(m_editorWidget->document());
  cursor.setPosition(m_proposal->basePosition());
  cursor.movePosition(QTextCursor::EndOfWord);

  auto currentPosition = m_editorWidget->position();
  const auto textAfterCursor = m_editorWidget->textAt(currentPosition, cursor.position() - currentPosition);
  if (!textAfterCursor.startsWith(newPrefix)) {
    if (newPrefix.indexOf(textAfterCursor, currentPosition - m_proposal->basePosition()) >= 0)
      currentPosition = cursor.position();
    const auto prefixAddition = QStringView(newPrefix).mid(currentPosition - m_proposal->basePosition());
    // If remaining string starts with the prefix addition
    if (textAfterCursor.startsWith(prefixAddition))
      currentPosition += prefixAddition.size();
  }

  m_editorWidget->setCursorPosition(m_proposal->basePosition());
  m_editorWidget->replace(currentPosition - m_proposal->basePosition(), newPrefix);
  notifyChange();
}

auto CodeAssistantPrivate::finalizeProposal() -> void
{
  stopAutomaticProposalTimer();
  m_proposal.reset();
  m_proposalWidget = nullptr;
  if (m_receivedContentWhileWaiting)
    m_receivedContentWhileWaiting = false;
}

auto CodeAssistantPrivate::isDisplayingProposal() const -> bool
{
  return m_proposalWidget != nullptr && m_proposalWidget->proposalIsVisible();
}

auto CodeAssistantPrivate::isWaitingForProposal() const -> bool
{
  return m_requestRunner != nullptr || m_asyncProcessor != nullptr;
}

auto CodeAssistantPrivate::invalidateCurrentRequestData() -> void
{
  m_asyncProcessor = nullptr;
  m_requestRunner = nullptr;
  m_requestProvider = nullptr;
  m_receivedContentWhileWaiting = false;
}

auto CodeAssistantPrivate::identifyActivationSequence() -> CompletionAssistProvider*
{
  auto checkActivationSequence = [this](CompletionAssistProvider *provider) {
    if (!provider)
      return false;
    const auto length = provider->activationCharSequenceLength();
    if (!length)
      return false;
    auto sequence = m_editorWidget->textAt(m_editorWidget->position() - length, length);
    // In pretty much all cases the sequence will have the appropriate length. Only in the
    // case of typing the very first characters in the document for providers that request a
    // length greater than 1 (currently only C++, which specifies 3), the sequence needs to
    // be prepended so it has the expected length.
    const int lengthDiff = length - sequence.length();
    for (auto j = 0; j < lengthDiff; ++j)
      sequence.prepend(m_null);
    return provider->isActivationCharSequence(sequence);
  };

  const auto provider = {m_editorWidget->textDocument()->completionAssistProvider(), m_editorWidget->textDocument()->functionHintAssistProvider()};
  return Utils::findOrDefault(provider, checkActivationSequence);
}

auto CodeAssistantPrivate::notifyChange() -> void
{
  stopAutomaticProposalTimer();

  if (isDisplayingProposal()) {
    QTC_ASSERT(m_proposal, return);
    if (m_editorWidget->position() < m_proposal->basePosition()) {
      destroyContext();
    } else if (m_proposal->supportsPrefix()) {
      m_proposalWidget->updateProposal(m_editorWidget->textAt(m_proposal->basePosition(), m_editorWidget->position() - m_proposal->basePosition()));
      if (!isDisplayingProposal())
        requestActivationCharProposal();
    } else {
      requestProposal(ExplicitlyInvoked, m_assistKind, m_requestProvider);
    }
  }
}

auto CodeAssistantPrivate::hasContext() const -> bool
{
  return m_requestRunner || m_asyncProcessor || m_proposalWidget;
}

auto CodeAssistantPrivate::destroyContext() -> void
{
  stopAutomaticProposalTimer();

  if (isWaitingForProposal()) {
    cancelCurrentRequest();
  } else if (m_proposalWidget) {
    m_editorWidget->keepAutoCompletionHighlight(false);
    if (m_proposalWidget->proposalIsVisible())
      m_proposalWidget->closeProposal();
    disconnect(m_proposalWidget, &QObject::destroyed, this, &CodeAssistantPrivate::finalizeProposal);
    finalizeProposal();
  }
}

auto CodeAssistantPrivate::userData() const -> QVariant
{
  return m_userData;
}

auto CodeAssistantPrivate::setUserData(const QVariant &data) -> void
{
  m_userData = data;
}

auto CodeAssistantPrivate::startAutomaticProposalTimer() -> void
{
  if (m_settings.m_completionTrigger == AutomaticCompletion)
    m_automaticProposalTimer.start();
}

auto CodeAssistantPrivate::automaticProposalTimeout() -> void
{
  if (isWaitingForProposal() || m_editorWidget->multiTextCursor().hasMultipleCursors() || isDisplayingProposal() && !m_proposal->isFragile()) {
    return;
  }

  requestProposal(IdleEditor, Completion);
}

auto CodeAssistantPrivate::stopAutomaticProposalTimer() -> void
{
  if (m_automaticProposalTimer.isActive())
    m_automaticProposalTimer.stop();
}

auto CodeAssistantPrivate::updateFromCompletionSettings(const CompletionSettings &settings) -> void
{
  m_settings = settings;
  m_automaticProposalTimer.setInterval(m_settings.m_automaticProposalTimeoutInMs);
}

auto CodeAssistantPrivate::explicitlyAborted() -> void
{
  QTC_ASSERT(m_proposal, return);
  m_abortedBasePosition = m_proposal->basePosition();
}

auto CodeAssistantPrivate::clearAbortedPosition() -> void
{
  m_abortedBasePosition = -1;
}

auto CodeAssistantPrivate::isDestroyEvent(int key, const QString &keyText) -> bool
{
  if (keyText.isEmpty())
    return key != Qt::LeftArrow && key != Qt::RightArrow && key != Qt::Key_Shift;
  if (const auto provider = qobject_cast<CompletionAssistProvider*>(m_requestProvider))
    return !provider->isContinuationChar(keyText.at(0));
  return false;
}

auto CodeAssistantPrivate::eventFilter(QObject *o, QEvent *e) -> bool
{
  Q_UNUSED(o)

  if (isWaitingForProposal()) {
    const auto type = e->type();
    if (type == QEvent::FocusOut) {
      destroyContext();
    } else if (type == QEvent::KeyPress) {
      const auto keyEvent = static_cast<QKeyEvent*>(e);
      const auto &keyText = keyEvent->text();

      if (isDestroyEvent(keyEvent->key(), keyText))
        destroyContext();
      else if (!keyText.isEmpty() && !m_receivedContentWhileWaiting)
        m_receivedContentWhileWaiting = true;
    } else if (type == QEvent::KeyRelease && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
      destroyContext();
    }
  }

  return false;
}

CodeAssistant::CodeAssistant() : d(new CodeAssistantPrivate(this)) {}

CodeAssistant::~CodeAssistant()
{
  destroyContext();
  delete d;
}

auto CodeAssistant::configure(TextEditorWidget *editorWidget) -> void
{
  d->configure(editorWidget);
}

auto CodeAssistant::process() -> void
{
  d->process();
}

auto CodeAssistant::notifyChange() -> void
{
  d->notifyChange();
}

auto CodeAssistant::hasContext() const -> bool
{
  return d->hasContext();
}

auto CodeAssistant::destroyContext() -> void
{
  d->destroyContext();
}

auto CodeAssistant::userData() const -> QVariant
{
  return d->userData();
}

auto CodeAssistant::setUserData(const QVariant &data) -> void
{
  d->setUserData(data);
}

auto CodeAssistant::invoke(AssistKind kind, IAssistProvider *provider) -> void
{
  d->invoke(kind, provider);
}

} // namespace TextEditor
