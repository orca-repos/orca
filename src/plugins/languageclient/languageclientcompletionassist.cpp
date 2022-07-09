// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientcompletionassist.hpp"

#include "client.hpp"
#include "languageclientutils.hpp"
#include "snippet.hpp"

#include <languageserverprotocol/completion.h>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/genericproposalmodel.hpp>
#include <texteditor/snippets/snippet.hpp>
#include <texteditor/snippets/snippetassistcollector.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <utils/algorithm.hpp>
#include <utils/textutils.hpp>
#include <utils/utilsicons.hpp>

#include <QDebug>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>
#include <QTime>

static Q_LOGGING_CATEGORY(LOGLSPCOMPLETION, "qtc.languageclient.completion", QtWarningMsg);

using namespace LanguageServerProtocol;
using namespace TextEditor;

namespace LanguageClient {

LanguageClientCompletionItem::LanguageClientCompletionItem(CompletionItem item) : m_item(std::move(item)) { }

auto LanguageClientCompletionItem::text() const -> QString { return m_item.label(); }
auto LanguageClientCompletionItem::implicitlyApplies() const -> bool { return false; }

auto LanguageClientCompletionItem::prematurelyApplies(const QChar &typedCharacter) const -> bool
{
  if (m_item.commitCharacters().has_value() && m_item.commitCharacters().value().contains(typedCharacter)) {
    m_triggeredCommitCharacter = typedCharacter;
    return true;
  }
  return false;
}

auto LanguageClientCompletionItem::apply(TextDocumentManipulatorInterface &manipulator, int /*basePosition*/) const -> void
{
  if (const auto edit = m_item.textEdit()) {
    applyTextEdit(manipulator, *edit, isSnippet());
  } else {
    const auto pos = manipulator.currentPosition();
    const auto textToInsert(m_item.insertText().value_or(text()));
    auto length = 0;
    for (auto it = textToInsert.crbegin(), end = textToInsert.crend(); it != end; ++it) {
      if (it->toLower() != manipulator.characterAt(pos - length - 1).toLower()) {
        length = 0;
        break;
      }
      ++length;
    }
    auto cursor = manipulator.textCursorAt(pos);
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    const auto blockTextUntilPosition = cursor.selectedText();
    static QRegularExpression identifier("[a-zA-Z_][a-zA-Z0-9_]*$");
    const auto match = identifier.match(blockTextUntilPosition);
    const int matchLength = match.hasMatch() ? match.capturedLength(0) : 0;
    length = qMax(length, matchLength);
    if (isSnippet()) {
      manipulator.replace(pos - length, length, {});
      manipulator.insertCodeSnippet(pos - length, textToInsert, &parseSnippet);
    } else {
      manipulator.replace(pos - length, length, textToInsert);
    }
  }

  if (auto additionalEdits = m_item.additionalTextEdits()) {
    for (const auto &edit : *additionalEdits)
      applyTextEdit(manipulator, edit);
  }
  if (!m_triggeredCommitCharacter.isNull())
    manipulator.insertCodeSnippet(manipulator.currentPosition(), m_triggeredCommitCharacter, &Snippet::parse);
}

auto LanguageClientCompletionItem::icon() const -> QIcon
{
  QIcon icon;
  using namespace Utils::CodeModelIcon;
  const auto kind = m_item.kind().value_or(CompletionItemKind::Text);
  switch (kind) {
  case CompletionItemKind::Method:
  case CompletionItemKind::Function:
  case CompletionItemKind::Constructor:
    icon = iconForType(FuncPublic);
    break;
  case CompletionItemKind::Field:
  case CompletionItemKind::Variable:
    icon = iconForType(VarPublic);
    break;
  case CompletionItemKind::Class:
    icon = iconForType(Class);
    break;
  case CompletionItemKind::Module:
    icon = iconForType(Namespace);
    break;
  case CompletionItemKind::Property:
    icon = iconForType(Property);
    break;
  case CompletionItemKind::Enum:
    icon = iconForType(Enum);
    break;
  case CompletionItemKind::Keyword:
    icon = iconForType(Keyword);
    break;
  case CompletionItemKind::Snippet:
    icon = QIcon(":/texteditor/images/snippet.png");
    break;
  case CompletionItemKind::EnumMember:
    icon = iconForType(Enumerator);
    break;
  case CompletionItemKind::Struct:
    icon = iconForType(Struct);
    break;
  default:
    icon = iconForType(Unknown);
    break;
  }
  return icon;
}

auto LanguageClientCompletionItem::detail() const -> QString
{
  if (const auto _doc = m_item.documentation()) {
    const auto doc = *_doc;
    QString detailDocText;
    if (Utils::holds_alternative<QString>(doc)) {
      detailDocText = Utils::get<QString>(doc);
    } else if (Utils::holds_alternative<MarkupContent>(doc)) {
      // TODO markdown parser?
      detailDocText = Utils::get<MarkupContent>(doc).content();
    }
    if (!detailDocText.isEmpty())
      return detailDocText;
  }
  return m_item.detail().value_or(text());
}

auto LanguageClientCompletionItem::isSnippet() const -> bool
{
  return m_item.insertTextFormat() == CompletionItem::Snippet;
}

auto LanguageClientCompletionItem::isValid() const -> bool
{
  return m_item.isValid();
}

auto LanguageClientCompletionItem::hash() const -> quint64
{
  return qHash(m_item.label()); // TODO: naaaa
}

auto LanguageClientCompletionItem::item() const -> CompletionItem
{
  return m_item;
}

auto LanguageClientCompletionItem::triggeredCommitCharacter() const -> QChar
{
  return m_triggeredCommitCharacter;
}

auto LanguageClientCompletionItem::sortText() const -> const QString&
{
  if (m_sortText.isEmpty())
    m_sortText = m_item.sortText().has_value() ? *m_item.sortText() : m_item.label();
  return m_sortText;
}

auto LanguageClientCompletionItem::hasSortText() const -> bool
{
  return m_item.sortText().has_value();
}

auto LanguageClientCompletionItem::filterText() const -> QString
{
  if (m_filterText.isEmpty()) {
    const auto filterText = m_item.filterText();
    m_filterText = filterText.has_value() ? filterText.value() : m_item.label();
  }
  return m_filterText;
}

auto LanguageClientCompletionItem::operator <(const LanguageClientCompletionItem &other) const -> bool
{
  return sortText() < other.sortText();
}

auto LanguageClientCompletionItem::isPerfectMatch(int pos, QTextDocument *doc) const -> bool
{
  QTC_ASSERT(doc, return false);
  using namespace Utils::Text;
  if (const auto additionalEdits = m_item.additionalTextEdits()) {
    if (!additionalEdits.value().isEmpty())
      return false;
  }
  if (isSnippet())
    return false;
  if (const auto edit = m_item.textEdit()) {
    const auto range = edit->range();
    const auto start = positionInText(doc, range.start().line() + 1, range.start().character() + 1);
    const auto end = positionInText(doc, range.end().line() + 1, range.end().character() + 1);
    const auto text = textAt(QTextCursor(doc), start, end - start);
    return text == edit->newText();
  }
  const auto textToInsert(m_item.insertText().value_or(text()));
  const int length = textToInsert.length();
  return textToInsert == textAt(QTextCursor(doc), pos - length, length);
}

class LanguageClientCompletionModel : public GenericProposalModel {
public:
  // GenericProposalModel interface
  auto containsDuplicates() const -> bool override { return false; }
  auto isSortable(const QString &/*prefix*/) const -> bool override;
  auto sort(const QString &/*prefix*/) -> void override;
  auto supportsPrefixExpansion() const -> bool override { return false; }

  auto items() const -> QList<AssistProposalItemInterface*> { return m_currentItems; }
};

auto LanguageClientCompletionModel::isSortable(const QString &) const -> bool
{
  return Utils::anyOf(items(), [](AssistProposalItemInterface *i) {
    const auto item = dynamic_cast<LanguageClientCompletionItem*>(i);
    return !item || item->hasSortText();
  });
}

auto LanguageClientCompletionModel::sort(const QString &/*prefix*/) -> void
{
  std::sort(m_currentItems.begin(), m_currentItems.end(), [](AssistProposalItemInterface *a, AssistProposalItemInterface *b) {
    const auto lca = dynamic_cast<LanguageClientCompletionItem*>(a);
    const auto lcb = dynamic_cast<LanguageClientCompletionItem*>(b);
    if (!lca && !lcb)
      return a->text() < b->text();
    if (lca && lcb)
      return *lca < *lcb;
    if (lca && !lcb)
      return true;
    return false;
  });
}

class LanguageClientCompletionProposal : public GenericProposal {
public:
  LanguageClientCompletionProposal(int cursorPos, LanguageClientCompletionModel *model) : GenericProposal(cursorPos, GenericProposalModelPtr(model)), m_model(model) { }

  // IAssistProposal interface
  auto hasItemsToPropose(const QString &/*text*/, AssistReason reason) const -> bool override
  {
    if (m_model->size() <= 0 || m_document.isNull())
      return false;

    return m_model->keepPerfectMatch(reason) || !Utils::anyOf(m_model->items(), [this](AssistProposalItemInterface *item) {
      if (const auto lcItem = dynamic_cast<LanguageClientCompletionItem*>(item))
        return lcItem->isPerfectMatch(m_pos, m_document);
      return false;
    });
  }

  LanguageClientCompletionModel *m_model;
  QPointer<QTextDocument> m_document;
  int m_pos = -1;
};

LanguageClientCompletionAssistProcessor::LanguageClientCompletionAssistProcessor(Client *client, const QString &snippetsGroup) : m_client(client), m_snippetsGroup(snippetsGroup) {}

LanguageClientCompletionAssistProcessor::~LanguageClientCompletionAssistProcessor()
{
  QTC_ASSERT(!running(), cancel());
}

auto LanguageClientCompletionAssistProcessor::document() const -> QTextDocument*
{
  return m_document;
}

auto LanguageClientCompletionAssistProcessor::generateCompletionItems(const QList<LanguageServerProtocol::CompletionItem> &items) const -> QList<AssistProposalItemInterface*>
{
  return Utils::transform<QList<AssistProposalItemInterface*>>(items, [](const CompletionItem &item) { return new LanguageClientCompletionItem(item); });
}

static auto assistReasonString(AssistReason reason) -> QString
{
  switch (reason) {
  case IdleEditor:
    return QString("idle editor");
  case ActivationCharacter:
    return QString("activation character");
  case ExplicitlyInvoked:
    return QString("explicitly invoking");
  }
  return QString("unknown reason");
}

auto LanguageClientCompletionAssistProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  QTC_ASSERT(m_client, return nullptr);
  m_pos = interface->position();
  m_basePos = m_pos;
  auto isIdentifierChar = [](const QChar &c) { return c.isLetterOrNumber() || c == '_'; };
  while (m_basePos > 0 && isIdentifierChar(interface->characterAt(m_basePos - 1)))
    --m_basePos;
  if (interface->reason() == IdleEditor) {
    // Trigger an automatic completion request only when we are on a word with at least n "identifier" characters
    if (m_pos - m_basePos < TextEditorSettings::completionSettings().m_characterThreshold)
      return nullptr;
    if (m_client->documentUpdatePostponed(interface->filePath())) {
      m_postponedUpdateConnection = QObject::connect(m_client, &Client::documentUpdated, [this, interface](TextEditor::TextDocument *document) {
        if (document->filePath() == interface->filePath())
          perform(interface);
      });
      return nullptr;
    }
  }
  if (m_postponedUpdateConnection)
    QObject::disconnect(m_postponedUpdateConnection);
  CompletionParams::CompletionContext context;
  if (interface->reason() == ActivationCharacter) {
    context.setTriggerKind(CompletionParams::TriggerCharacter);
    const auto triggerCharacter = interface->characterAt(interface->position() - 1);
    if (!triggerCharacter.isNull())
      context.setTriggerCharacter(triggerCharacter);
  } else {
    context.setTriggerKind(CompletionParams::Invoked);
  }
  CompletionParams params;
  int line;
  int column;
  if (!Utils::Text::convertPosition(interface->textDocument(), m_pos, &line, &column))
    return nullptr;
  --line;   // line is 0 based in the protocol
  --column; // column is 0 based in the protocol
  params.setPosition({line, column});
  params.setContext(context);
  params.setTextDocument(TextDocumentIdentifier(DocumentUri::fromFilePath(interface->filePath())));
  CompletionRequest completionRequest(params);
  completionRequest.setResponseCallback([this](auto response) {
    this->handleCompletionResponse(response);
  });
  m_client->sendContent(completionRequest);
  m_client->addAssistProcessor(this);
  m_currentRequest = completionRequest.id();
  m_document = interface->textDocument();
  m_filePath = interface->filePath();
  qCDebug(LOGLSPCOMPLETION) << QTime::currentTime() << " : request completions at " << m_pos << " by " << assistReasonString(interface->reason());
  return nullptr;
}

auto LanguageClientCompletionAssistProcessor::running() -> bool
{
  return m_currentRequest.has_value() || m_postponedUpdateConnection;
}

auto LanguageClientCompletionAssistProcessor::cancel() -> void
{
  if (m_currentRequest.has_value()) {
    if (m_client) {
      m_client->cancelRequest(m_currentRequest.value());
      m_client->removeAssistProcessor(this);
    }
    m_currentRequest.reset();
  } else if (m_postponedUpdateConnection) {
    QObject::disconnect(m_postponedUpdateConnection);
  }
}

auto LanguageClientCompletionAssistProcessor::handleCompletionResponse(const CompletionRequest::Response &response) -> void
{
  // We must report back to the code assistant under all circumstances
  qCDebug(LOGLSPCOMPLETION) << QTime::currentTime() << " : got completions";
  m_currentRequest.reset();
  QTC_ASSERT(m_client, setAsyncProposalAvailable(nullptr); return);
  if (const auto error = response.error())
    m_client->log(error.value());

  const auto &result = response.result();
  if (!result || Utils::holds_alternative<std::nullptr_t>(*result)) {
    setAsyncProposalAvailable(nullptr);
    m_client->removeAssistProcessor(this);
    return;
  }

  QList<CompletionItem> items;
  if (Utils::holds_alternative<CompletionList>(*result)) {
    const auto &list = Utils::get<CompletionList>(*result);
    items = list.items().value_or(QList<CompletionItem>());
  } else if (Utils::holds_alternative<QList<CompletionItem>>(*result)) {
    items = Utils::get<QList<CompletionItem>>(*result);
  }
  auto proposalItems = generateCompletionItems(items);
  if (!m_snippetsGroup.isEmpty()) {
    proposalItems << TextEditor::SnippetAssistCollector(m_snippetsGroup, QIcon(":/texteditor/images/snippet.png")).collect();
  }
  const auto model = new LanguageClientCompletionModel();
  model->loadContent(proposalItems);
  const auto proposal = new LanguageClientCompletionProposal(m_basePos, model);
  proposal->m_document = m_document;
  proposal->m_pos = m_pos;
  proposal->setFragile(true);
  proposal->setSupportsPrefix(false);
  setAsyncProposalAvailable(proposal);
  m_client->removeAssistProcessor(this);
  qCDebug(LOGLSPCOMPLETION) << QTime::currentTime() << " : " << items.count() << " completions handled";
}

LanguageClientCompletionAssistProvider::LanguageClientCompletionAssistProvider(Client *client) : CompletionAssistProvider(client), m_client(client) { }

auto LanguageClientCompletionAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new LanguageClientCompletionAssistProcessor(m_client, m_snippetsGroup);
}

auto LanguageClientCompletionAssistProvider::runType() const -> IAssistProvider::RunType
{
  return IAssistProvider::Asynchronous;
}

auto LanguageClientCompletionAssistProvider::activationCharSequenceLength() const -> int
{
  return m_activationCharSequenceLength;
}

auto LanguageClientCompletionAssistProvider::isActivationCharSequence(const QString &sequence) const -> bool
{
  return Utils::anyOf(m_triggerChars, [sequence](const QString &trigger) {
    return trigger.endsWith(sequence);
  });
}

auto LanguageClientCompletionAssistProvider::setTriggerCharacters(const Utils::optional<QList<QString>> triggerChars) -> void
{
  m_activationCharSequenceLength = 0;
  m_triggerChars = triggerChars.value_or(QList<QString>());
  for (const auto &trigger : qAsConst(m_triggerChars)) {
    if (trigger.length() > m_activationCharSequenceLength)
      m_activationCharSequenceLength = trigger.length();
  }
}

} // namespace LanguageClient
