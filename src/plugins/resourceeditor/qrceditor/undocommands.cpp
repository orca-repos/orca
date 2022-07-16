// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "undocommands_p.hpp"

#include <QModelIndex>

using namespace ResourceEditor;
using namespace ResourceEditor::Internal;

ViewCommand::ViewCommand(ResourceView *view) : m_view(view) { }
ViewCommand::~ViewCommand() = default;

ModelIndexViewCommand::ModelIndexViewCommand(ResourceView *view) : ViewCommand(view) { }
ModelIndexViewCommand::~ModelIndexViewCommand() = default;

auto ModelIndexViewCommand::storeIndex(const QModelIndex &index) -> void
{
  if (m_view->isPrefix(index)) {
    m_prefixArrayIndex = index.row();
    m_fileArrayIndex = -1;
  } else {
    m_fileArrayIndex = index.row();
    m_prefixArrayIndex = m_view->model()->parent(index).row();
  }
}

auto ModelIndexViewCommand::makeIndex() const -> QModelIndex
{
  const auto prefixModelIndex = m_view->model()->index(m_prefixArrayIndex, 0, QModelIndex());
  if (m_fileArrayIndex != -1) {
    // File node
    const auto fileModelIndex = m_view->model()->index(m_fileArrayIndex, 0, prefixModelIndex);
    return fileModelIndex;
  } else {
    // Prefix node
    return prefixModelIndex;
  }
}

ModifyPropertyCommand::ModifyPropertyCommand(ResourceView *view, const QModelIndex &nodeIndex, ResourceView::NodeProperty property, const int mergeId, const QString &before, const QString &after) : ModelIndexViewCommand(view), m_property(property), m_before(before), m_after(after), m_mergeId(mergeId)
{
  storeIndex(nodeIndex);
}

auto ModifyPropertyCommand::mergeWith(const QUndoCommand *command) -> bool
{
  if (command->id() != id() || m_property != static_cast<const ModifyPropertyCommand*>(command)->m_property)
    return false;
  // Choose older command (this) and forgot the other
  return true;
}

auto ModifyPropertyCommand::undo() -> void
{
  Q_ASSERT(m_view);

  // Save current text in m_after for redo()
  m_after = m_view->getCurrentValue(m_property);

  // Reset text to m_before
  m_view->changeValue(makeIndex(), m_property, m_before);
}

auto ModifyPropertyCommand::redo() -> void
{
  // Prevent execution from within QUndoStack::push
  if (m_after.isNull())
    return;

  // Bring back text before undo
  Q_ASSERT(m_view);
  m_view->changeValue(makeIndex(), m_property, m_after);
}

RemoveEntryCommand::RemoveEntryCommand(ResourceView *view, const QModelIndex &index) : ModelIndexViewCommand(view), m_entry(nullptr), m_isExpanded(true)
{
  storeIndex(index);
}

RemoveEntryCommand::~RemoveEntryCommand()
{
  freeEntry();
}

auto RemoveEntryCommand::redo() -> void
{
  freeEntry();
  const auto index = makeIndex();
  m_isExpanded = m_view->isExpanded(index);
  m_entry = m_view->removeEntry(index);
}

auto RemoveEntryCommand::undo() -> void
{
  if (m_entry != nullptr) {
    m_entry->restore();
    Q_ASSERT(m_view != nullptr);
    const auto index = makeIndex();
    m_view->setExpanded(index, m_isExpanded);
    m_view->setCurrentIndex(index);
    freeEntry();
  }
}

auto RemoveEntryCommand::freeEntry() -> void
{
  delete m_entry;
  m_entry = nullptr;
}

RemoveMultipleEntryCommand::RemoveMultipleEntryCommand(ResourceView *view, const QList<QModelIndex> &list)
{
  m_subCommands.reserve(list.size());
  for (const auto &index : list)
    m_subCommands.push_back(new RemoveEntryCommand(view, index));
}

RemoveMultipleEntryCommand::~RemoveMultipleEntryCommand()
{
  qDeleteAll(m_subCommands);
}

auto RemoveMultipleEntryCommand::redo() -> void
{
  auto it = m_subCommands.rbegin();
  const auto end = m_subCommands.rend();

  for (; it != end; ++it)
    (*it)->redo();
}

auto RemoveMultipleEntryCommand::undo() -> void
{
  auto it = m_subCommands.begin();
  const auto end = m_subCommands.end();

  for (; it != end; ++it)
    (*it)->undo();
}

AddFilesCommand::AddFilesCommand(ResourceView *view, int prefixIndex, int cursorFileIndex, const QStringList &fileNames) : ViewCommand(view), m_prefixIndex(prefixIndex), m_cursorFileIndex(cursorFileIndex), m_fileNames(fileNames) { }

auto AddFilesCommand::redo() -> void
{
  m_view->addFiles(m_prefixIndex, m_fileNames, m_cursorFileIndex, m_firstFile, m_lastFile);
}

auto AddFilesCommand::undo() -> void
{
  m_view->removeFiles(m_prefixIndex, m_firstFile, m_lastFile);
}

AddEmptyPrefixCommand::AddEmptyPrefixCommand(ResourceView *view) : ViewCommand(view) { }

auto AddEmptyPrefixCommand::redo() -> void
{
  m_prefixArrayIndex = m_view->addPrefix().row();
}

auto AddEmptyPrefixCommand::undo() -> void
{
  const auto prefixModelIndex = m_view->model()->index(m_prefixArrayIndex, 0, QModelIndex());
  delete m_view->removeEntry(prefixModelIndex);
}
