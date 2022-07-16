// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <QString>
#include <QList>
#include <QIcon>

namespace TextEditor {

class AssistProposalItemInterface;

class TEXTEDITOR_EXPORT SnippetAssistCollector {
public:
  SnippetAssistCollector(const QString &groupId, const QIcon &icon, int order = 0);

  auto setGroupId(const QString &gid) -> void;
  auto groupId() const -> QString;
  auto collect() const -> QList<AssistProposalItemInterface*>;

private:
  QString m_groupId;
  QIcon m_icon;
  int m_order;
};

} // TextEditor
