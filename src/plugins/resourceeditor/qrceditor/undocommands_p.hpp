// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "resourceview.hpp"

#include <QString>
#include <QUndoCommand>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace ResourceEditor {
namespace Internal {

/*!
    \class ViewCommand

    Provides a base for \l ResourceView-related commands.
*/
class ViewCommand : public QUndoCommand {
protected:
  ResourceView *m_view;

  ViewCommand(ResourceView *view);
  ~ViewCommand() override;
};

/*!
    \class ModelIndexViewCommand

    Provides a mean to store/restore a \l QModelIndex as it cannot
    be stored safely in most cases. This is an abstract class.
*/
class ModelIndexViewCommand : public ViewCommand {
  int m_prefixArrayIndex;
  int m_fileArrayIndex;

protected:
  ModelIndexViewCommand(ResourceView *view);
  ~ModelIndexViewCommand() override;

  auto storeIndex(const QModelIndex &index) -> void;
  auto makeIndex() const -> QModelIndex;
};

/*!
    \class ModifyPropertyCommand

    Modifies the name/prefix/language property of a prefix/file node.
*/
class ModifyPropertyCommand : public ModelIndexViewCommand {
  ResourceView::NodeProperty m_property;
  QString m_before;
  QString m_after;
  int m_mergeId;

public:
  ModifyPropertyCommand(ResourceView *view, const QModelIndex &nodeIndex, ResourceView::NodeProperty property, const int mergeId, const QString &before, const QString &after = QString());

private:
  auto id() const -> int override { return m_mergeId; }
  auto mergeWith(const QUndoCommand *command) -> bool override;
  auto undo() -> void override;
  auto redo() -> void override;
};

/*!
    \class RemoveEntryCommand

    Removes a \l QModelIndex including all children from a \l ResourceView.
*/
class RemoveEntryCommand : public ModelIndexViewCommand {
  EntryBackup *m_entry;
  bool m_isExpanded;

public:
  RemoveEntryCommand(ResourceView *view, const QModelIndex &index);
  ~RemoveEntryCommand() override;

private:
  auto redo() -> void override;
  auto undo() -> void override;
  auto freeEntry() -> void;
};

/*!
    \class RemoveMultipleEntryCommand

    Removes multiple \l QModelIndex including all children from a \l ResourceView.
*/
class RemoveMultipleEntryCommand : public QUndoCommand {
  std::vector<QUndoCommand*> m_subCommands;
public:
  // list must be in view order
  RemoveMultipleEntryCommand(ResourceView *view, const QList<QModelIndex> &list);
  ~RemoveMultipleEntryCommand() override;
private:
  auto redo() -> void override;
  auto undo() -> void override;
};

/*!
    \class AddFilesCommand

    Adds a list of files to a given prefix node.
*/
class AddFilesCommand : public ViewCommand {
  int m_prefixIndex;
  int m_cursorFileIndex;
  int m_firstFile;
  int m_lastFile;
  const QStringList m_fileNames;

public:
  AddFilesCommand(ResourceView *view, int prefixIndex, int cursorFileIndex, const QStringList &fileNames);

private:
  auto redo() -> void override;
  auto undo() -> void override;
};

/*!
    \class AddEmptyPrefixCommand

    Adds a new, empty prefix node.
*/
class AddEmptyPrefixCommand : public ViewCommand {
  int m_prefixArrayIndex;

public:
  AddEmptyPrefixCommand(ResourceView *view);

private:
  auto redo() -> void override;
  auto undo() -> void override;
};

} // namespace Internal
} // namespace ResourceEditor
