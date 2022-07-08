// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippetassistcollector.hpp"
#include "snippetscollection.hpp"

#include <texteditor/texteditorconstants.hpp>
#include <texteditor/codeassist/assistproposalitem.hpp>

using namespace TextEditor;
using namespace Internal;

static auto appendSnippets(QList<AssistProposalItemInterface*> *items, const QString &groupId, const QIcon &icon, int order) -> void
{
  const auto collection = SnippetsCollection::instance();
  const auto size = collection->totalActiveSnippets(groupId);
  for (auto i = 0; i < size; ++i) {
    const auto &snippet = collection->snippet(i, groupId);
    const auto item = new AssistProposalItem;
    item->setText(snippet.trigger() + QLatin1Char(' ') + snippet.complement());
    item->setData(snippet.content());
    item->setDetail(snippet.generateTip());
    item->setIcon(icon);
    item->setOrder(order);
    items->append(item);
  }
}

SnippetAssistCollector::SnippetAssistCollector(const QString &groupId, const QIcon &icon, int order) : m_groupId(groupId), m_icon(icon), m_order(order) {}

auto SnippetAssistCollector::setGroupId(const QString &gid) -> void
{
  m_groupId = gid;
}

auto SnippetAssistCollector::groupId() const -> QString
{
  return m_groupId;
}

auto SnippetAssistCollector::collect() const -> QList<AssistProposalItemInterface*>
{
  QList<AssistProposalItemInterface*> snippets;
  if (m_groupId.isEmpty())
    return snippets;
  appendSnippets(&snippets, m_groupId, m_icon, m_order);
  if (m_groupId != Constants::TEXT_SNIPPET_GROUP_ID)
    appendSnippets(&snippets, Constants::TEXT_SNIPPET_GROUP_ID, m_icon, m_order);
  return snippets;
}
