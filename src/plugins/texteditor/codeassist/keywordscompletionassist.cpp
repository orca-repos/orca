// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "keywordscompletionassist.hpp"

#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/functionhintproposal.hpp>
#include <texteditor/completionsettings.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/texteditor.hpp>

#include <core/core-constants.hpp>

#include <utils/algorithm.hpp>
#include <utils/utilsicons.hpp>

#include <QDir>
#include <QFileInfo>

namespace TextEditor {

// Note: variables and functions must be sorted
Keywords::Keywords(const QStringList &variables, const QStringList &functions, const QMap<QString, QStringList> &functionArgs) : m_variables(variables), m_functions(functions), m_functionArgs(functionArgs)
{
  Utils::sort(m_variables);
  Utils::sort(m_functions);
}

auto Keywords::isVariable(const QString &word) const -> bool
{
  return std::binary_search(m_variables.constBegin(), m_variables.constEnd(), word);
}

auto Keywords::isFunction(const QString &word) const -> bool
{
  return std::binary_search(m_functions.constBegin(), m_functions.constEnd(), word);
}

auto Keywords::variables() const -> QStringList
{
  return m_variables;
}

auto Keywords::functions() const -> QStringList
{
  return m_functions;
}

auto Keywords::argsForFunction(const QString &function) const -> QStringList
{
  return m_functionArgs.value(function);
}

KeywordsAssistProposalItem::KeywordsAssistProposalItem(bool isFunction) : m_isFunction(isFunction) {}

auto KeywordsAssistProposalItem::prematurelyApplies(const QChar &c) const -> bool
{
  // only '(' in case of a function
  return c == QLatin1Char('(') && m_isFunction;
}

auto KeywordsAssistProposalItem::applyContextualContent(TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void
{
  const auto &settings = TextEditorSettings::completionSettings();

  auto replaceLength = manipulator.currentPosition() - basePosition;
  auto toInsert = text();
  auto cursorOffset = 0;
  const auto characterAtCurrentPosition = manipulator.characterAt(manipulator.currentPosition());
  auto setAutoCompleteSkipPosition = false;

  if (m_isFunction && settings.m_autoInsertBrackets) {
    if (settings.m_spaceAfterFunctionName) {
      if (manipulator.textAt(manipulator.currentPosition(), 2) == QLatin1String(" (")) {
        cursorOffset = 2;
      } else if (characterAtCurrentPosition == QLatin1Char('(') || characterAtCurrentPosition == QLatin1Char(' ')) {
        replaceLength += 1;
        toInsert += QLatin1String(" (");
      } else {
        toInsert += QLatin1String(" ()");
        cursorOffset = -1;
        setAutoCompleteSkipPosition = true;
      }
    } else {
      if (characterAtCurrentPosition == QLatin1Char('(')) {
        cursorOffset = 1;
      } else {
        toInsert += QLatin1String("()");
        cursorOffset = -1;
        setAutoCompleteSkipPosition = true;
      }
    }
  }

  manipulator.replace(basePosition, replaceLength, toInsert);
  if (cursorOffset)
    manipulator.setCursorPosition(manipulator.currentPosition() + cursorOffset);
  if (setAutoCompleteSkipPosition)
    manipulator.setAutoCompleteSkipPosition(manipulator.currentPosition());
}

KeywordsFunctionHintModel::KeywordsFunctionHintModel(const QStringList &functionSymbols) : m_functionSymbols(functionSymbols) {}

auto KeywordsFunctionHintModel::reset() -> void {}

auto KeywordsFunctionHintModel::size() const -> int
{
  return m_functionSymbols.size();
}

auto KeywordsFunctionHintModel::text(int index) const -> QString
{
  return m_functionSymbols.at(index);
}

auto KeywordsFunctionHintModel::activeArgument(const QString &prefix) const -> int
{
  Q_UNUSED(prefix)
  return 1;
}

KeywordsCompletionAssistProcessor::KeywordsCompletionAssistProcessor(const Keywords &keywords) : m_snippetCollector(QString(), QIcon(":/texteditor/images/snippet.png")), m_variableIcon(QLatin1String(":/codemodel/images/keyword.png")), m_functionIcon(QLatin1String(":/codemodel/images/member.png")), m_keywords(keywords) {}

auto KeywordsCompletionAssistProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  QScopedPointer assistInterface(interface);
  if (isInComment(interface))
    return nullptr;

  auto pos = interface->position();

  // Find start position
  auto chr = interface->characterAt(pos - 1);
  if (chr == '(')
    --pos;
  // Skip to the start of a name
  do {
    chr = interface->characterAt(--pos);
  } while (chr.isLetterOrNumber() || chr == '_');

  ++pos;

  auto startPosition = pos;

  if (interface->reason() == IdleEditor) {
    const auto characterUnderCursor = interface->characterAt(interface->position());
    if (characterUnderCursor.isLetterOrNumber() || interface->position() - startPosition < TextEditorSettings::completionSettings().m_characterThreshold) {
      QList<AssistProposalItemInterface*> items;
      if (m_dynamicCompletionFunction)
        m_dynamicCompletionFunction(interface, &items, startPosition);
      if (items.isEmpty())
        return nullptr;
      return new GenericProposal(startPosition, items);
    }
  }

  // extract word
  QString word;
  do {
    word += interface->characterAt(pos);
    chr = interface->characterAt(++pos);
  } while ((chr.isLetterOrNumber() || chr == '_') && chr != '(');

  if (m_keywords.isFunction(word) && interface->characterAt(pos) == '(') {
    const auto functionSymbols = m_keywords.argsForFunction(word);
    if (functionSymbols.size() == 0)
      return nullptr;
    const FunctionHintProposalModelPtr model(new KeywordsFunctionHintModel(functionSymbols));
    return new FunctionHintProposal(startPosition, model);
  }
  const auto originalStartPos = startPosition;
  QList<AssistProposalItemInterface*> items;
  if (m_dynamicCompletionFunction)
    m_dynamicCompletionFunction(interface, &items, startPosition);
  if (startPosition == originalStartPos) {
    items.append(m_snippetCollector.collect());
    items.append(generateProposalList(m_keywords.variables(), m_variableIcon));
    items.append(generateProposalList(m_keywords.functions(), m_functionIcon));
  }
  return new GenericProposal(startPosition, items);
}

auto KeywordsCompletionAssistProcessor::setSnippetGroup(const QString &id) -> void
{
  m_snippetCollector.setGroupId(id);
}

auto KeywordsCompletionAssistProcessor::setKeywords(const Keywords &keywords) -> void
{
  m_keywords = keywords;
}

auto KeywordsCompletionAssistProcessor::setDynamicCompletionFunction(DynamicCompletionFunction func) -> void
{
  m_dynamicCompletionFunction = func;
}

auto KeywordsCompletionAssistProcessor::isInComment(const AssistInterface *interface) const -> bool
{
  QTextCursor tc(interface->textDocument());
  tc.setPosition(interface->position());
  tc.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
  return tc.selectedText().contains('#');
}

auto KeywordsCompletionAssistProcessor::generateProposalList(const QStringList &words, const QIcon &icon) -> QList<AssistProposalItemInterface*>
{
  return Utils::transform(words, [this, &icon](const QString &word) -> AssistProposalItemInterface* {
    AssistProposalItem *item = new KeywordsAssistProposalItem(m_keywords.isFunction(word));
    item->setText(word);
    item->setIcon(icon);
    return item;
  });
}

KeywordsCompletionAssistProvider::KeywordsCompletionAssistProvider(const Keywords &keyWords, const QString &snippetGroup) : m_keyWords(keyWords), m_snippetGroup(snippetGroup) { }

auto KeywordsCompletionAssistProvider::setDynamicCompletionFunction(const DynamicCompletionFunction &func) -> void
{
  m_completionFunc = func;
}

auto KeywordsCompletionAssistProvider::runType() const -> RunType
{
  return Synchronous;
}

auto KeywordsCompletionAssistProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  const auto processor = new KeywordsCompletionAssistProcessor(m_keyWords);
  processor->setSnippetGroup(m_snippetGroup);
  processor->setDynamicCompletionFunction(m_completionFunc);
  return processor;
}

auto pathComplete(const AssistInterface *interface, QList<AssistProposalItemInterface*> *items, int &startPosition) -> void
{
  if (!items)
    return;

  if (interface->filePath().isEmpty())
    return;

  // For pragmatic reasons, we don't support spaces in file names here.
  static const auto canOccurInFilePath = [](const QChar &c) {
    return c.isLetterOrNumber() || c == '.' || c == '/' || c == '_' || c == '-';
  };

  auto pos = interface->position();
  QChar chr;
  // Skip to the start of a name
  do {
    chr = interface->characterAt(--pos);
  } while (canOccurInFilePath(chr));

  const auto startPos = ++pos;

  if (interface->reason() == IdleEditor && interface->position() - startPos < 3)
    return;

  const auto word = interface->textAt(startPos, interface->position() - startPos);
  auto baseDir = interface->filePath().toFileInfo().absoluteDir();
  const int lastSlashPos = word.lastIndexOf(QLatin1Char('/'));

  auto prefix = word;
  if (lastSlashPos != -1) {
    prefix = word.mid(lastSlashPos + 1);
    if (!baseDir.cd(word.left(lastSlashPos)))
      return;
  }

  const auto entryInfoList = baseDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
  for (const auto &entry : entryInfoList) {
    const auto &fileName = entry.fileName();
    if (fileName.startsWith(prefix)) {
      const auto item = new AssistProposalItem;
      if (entry.isDir()) {
        item->setText(fileName + QLatin1String("/"));
        item->setIcon(Utils::Icons::DIR.icon());
      } else {
        item->setText(fileName);
        item->setIcon(Utils::Icons::UNKNOWN_FILE.icon());
      }
      *items << item;
    }
  }
  if (!items->empty())
    startPosition = startPos;
}

} // namespace TextEditor
