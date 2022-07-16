// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "documentcontentcompletion.hpp"

#include "assistinterface.hpp"
#include "assistproposalitem.hpp"
#include "genericproposal.hpp"
#include "iassistprocessor.hpp"

#include <texteditor/snippets/snippetassistcollector.hpp>"
#include <texteditor/completionsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <utils/algorithm.hpp>
#include <utils/runextensions.hpp>

#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextDocument>

using namespace TextEditor;

class DocumentContentCompletionProcessor final : public IAssistProcessor {
public:
  DocumentContentCompletionProcessor(const QString &snippetGroupId);
  ~DocumentContentCompletionProcessor() final;

  auto perform(const AssistInterface *interface) -> IAssistProposal* override;
  auto running() -> bool override { return m_watcher.isRunning(); }
  auto cancel() -> void override;

private:
  QString m_snippetGroup;
  QFutureWatcher<QStringList> m_watcher;
};

DocumentContentCompletionProvider::DocumentContentCompletionProvider(const QString &snippetGroup) : m_snippetGroup(snippetGroup) { }

auto DocumentContentCompletionProvider::runType() const -> RunType
{
  return Asynchronous;
}

auto DocumentContentCompletionProvider::createProcessor(const AssistInterface *) const -> IAssistProcessor*
{
  return new DocumentContentCompletionProcessor(m_snippetGroup);
}

DocumentContentCompletionProcessor::DocumentContentCompletionProcessor(const QString &snippetGroupId) : m_snippetGroup(snippetGroupId) { }

DocumentContentCompletionProcessor::~DocumentContentCompletionProcessor()
{
  cancel();
}

static auto createProposal(QFutureInterface<QStringList> &future, const QString &text, const QString &wordUnderCursor) -> void
{
  const QRegularExpression wordRE("([a-zA-Z_][a-zA-Z0-9_]{2,})");

  QSet<QString> words;
  auto it = wordRE.globalMatch(text);
  auto wordUnderCursorFound = 0;
  while (it.hasNext()) {
    if (future.isCanceled())
      return;
    auto match = it.next();
    const auto &word = match.captured();
    if (word == wordUnderCursor) {
      // Only add the word under cursor if it
      // already appears elsewhere in the text
      if (++wordUnderCursorFound < 2)
        continue;
    }

    if (!words.contains(word))
      words.insert(word);
  }

  future.reportResult(Utils::toList(words));
}

auto DocumentContentCompletionProcessor::perform(const AssistInterface *interface) -> IAssistProposal*
{
  QScopedPointer assistInterface(interface);
  if (running())
    return nullptr;

  auto pos = interface->position();

  QChar chr;
  // Skip to the start of a name
  do {
    chr = interface->characterAt(--pos);
  } while (chr.isLetterOrNumber() || chr == '_');

  ++pos;
  const auto length = interface->position() - pos;

  if (interface->reason() == IdleEditor) {
    const auto characterUnderCursor = interface->characterAt(interface->position());
    if (characterUnderCursor.isLetterOrNumber() || length < TextEditorSettings::completionSettings().m_characterThreshold) {
      return nullptr;
    }
  }

  const auto wordUnderCursor = interface->textAt(pos, length);
  const auto text = interface->textDocument()->toPlainText();

  m_watcher.setFuture(Utils::runAsync(&createProposal, text, wordUnderCursor));
  QObject::connect(&m_watcher, &QFutureWatcher<QStringList>::resultReadyAt, &m_watcher, [this, pos](int index) {
    const SnippetAssistCollector snippetCollector(m_snippetGroup, QIcon(":/texteditor/images/snippet.png"));
    auto items = snippetCollector.collect();
    for (const auto &word : m_watcher.resultAt(index)) {
      const auto item = new AssistProposalItem();
      item->setText(word);
      items.append(item);
    }
    setAsyncProposalAvailable(new GenericProposal(pos, items));
  });
  return nullptr;
}

auto DocumentContentCompletionProcessor::cancel() -> void
{
  if (running())
    m_watcher.cancel();
}
