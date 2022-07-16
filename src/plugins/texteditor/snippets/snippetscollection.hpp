// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "snippet.hpp"

#include <utils/filepath.hpp>

#include <QVector>
#include <QHash>
#include <QXmlStreamWriter>

namespace TextEditor {
namespace Internal {

// Characteristics of this collection:
// - Store snippets by group.
// - Keep groups of snippets sorted.
// - Allow snippet insertion/replacement based on a hint.
// - Allow modification of snippet members that are not sorting keys.
// - Track removed/modified built-in snippets.
// - Provide fast index access.
// - Not thread-safe.

class SnippetsCollection : public QObject {
  Q_OBJECT

public:
  ~SnippetsCollection() override;

  static auto instance() -> SnippetsCollection*;

  class Hint {
    friend class SnippetsCollection;
  public:
    auto index() const -> int;
  private:
    explicit Hint(int index);
    Hint(int index, QVector<Snippet>::iterator it);
    int m_index;
    QVector<Snippet>::iterator m_it;
  };

  auto insertSnippet(const Snippet &snippet) -> void;
  auto insertSnippet(const Snippet &snippet, const Hint &hint) -> void;
  auto computeInsertionHint(const Snippet &snippet) -> Hint;

  // Replace snippets only within the same group.
  auto replaceSnippet(int index, const Snippet &snippet) -> void;
  auto replaceSnippet(int index, const Snippet &snippet, const Hint &hint) -> void;
  auto computeReplacementHint(int index, const Snippet &snippet) -> Hint;
  auto removeSnippet(int index, const QString &groupId) -> void;
  auto restoreRemovedSnippets(const QString &groupId) -> void;
  auto setSnippetContent(int index, const QString &groupId, const QString &content) -> void;
  auto snippet(int index, const QString &groupId) const -> const Snippet&;
  auto revertedSnippet(int index, const QString &groupId) const -> Snippet;
  auto reset(const QString &groupId) -> void;
  auto totalActiveSnippets(const QString &groupId) const -> int;
  auto totalSnippets(const QString &groupId) const -> int;
  auto groupIds() const -> QList<QString>;
  auto reload() -> void;
  auto synchronize(QString *errorString) -> bool;

private:
  auto identifyGroups() -> void;

  SnippetsCollection();

  auto groupIndex(const QString &groupId) const -> int;
  auto isGroupKnown(const QString &groupId) const -> bool;
  auto clearSnippets() -> void;
  auto clearSnippets(int groupIndex) -> void;
  auto updateActiveSnippetsEnd(int groupIndex) -> void;
  auto readXML(const Utils::FilePath &fileName, const QString &snippetId = {}) const -> QList<Snippet>;
  auto writeSnippetXML(const Snippet &snippet, QXmlStreamWriter *writer) const -> void;
  auto allBuiltInSnippets() const -> QList<Snippet>;

  // Built-in snippets are specified in XMLs distributed in a system's folder. Snippets
  // created or modified/removed (if they are built-ins) by the user are stored in user's
  // folder.
  const Utils::FilePath m_userSnippetsFile;
  const Utils::FilePaths m_builtInSnippetsFiles;

  // Snippets for each group are kept in a list. However, not all of them are necessarily
  // active. Specifically, removed built-in snippets are kept as the last ones (for each
  // group there is a iterator that marks the logical end).
  QVector<QVector<Snippet>> m_snippets;
  QVector<int> m_activeSnippetsCount;
  QHash<QString, int> m_groupIndexById;
};

} // Internal
} // TextEditor
