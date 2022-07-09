// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "resourcefile_p.hpp"

#include <utils/itemviews.hpp>

#include <QPoint>

QT_BEGIN_NAMESPACE
class QUndoStack;
QT_END_NAMESPACE

namespace ResourceEditor {
namespace Internal {

class RelativeResourceModel;

class ResourceView : public Utils::TreeView {
  Q_OBJECT

public:
  enum NodeProperty {
    AliasProperty,
    PrefixProperty,
    LanguageProperty
  };

  explicit ResourceView(RelativeResourceModel *model, QUndoStack *history, QWidget *parent = nullptr);
  ~ResourceView() override;

  auto filePath() const -> Utils::FilePath;
  auto isPrefix(const QModelIndex &index) const -> bool;
  auto currentAlias() const -> QString;
  auto currentPrefix() const -> QString;
  auto currentLanguage() const -> QString;
  auto currentResourcePath() const -> QString;
  auto setResourceDragEnabled(bool e) -> void;
  auto resourceDragEnabled() const -> bool;
  auto findSamePlacePostDeletionModelIndex(int &row, QModelIndex &parent) const -> void;
  auto removeEntry(const QModelIndex &index) -> EntryBackup*;
  auto existingFilesSubtracted(int prefixIndex, const QStringList &fileNames) const -> QStringList;
  auto addFiles(int prefixIndex, const QStringList &fileNames, int cursorFile, int &firstFile, int &lastFile) -> void;
  auto removeFiles(int prefixIndex, int firstFileIndex, int lastFileIndex) -> void;
  auto fileNamesToAdd() -> QStringList;
  auto addPrefix() -> QModelIndex;
  auto nonExistingFiles() -> QList<QModelIndex>;
  auto refresh() -> void;
  auto setCurrentAlias(const QString &before, const QString &after) -> void;
  auto setCurrentPrefix(const QString &before, const QString &after) -> void;
  auto setCurrentLanguage(const QString &before, const QString &after) -> void;
  auto advanceMergeId() -> void;
  auto getCurrentValue(NodeProperty property) const -> QString;
  auto changeValue(const QModelIndex &nodeIndex, NodeProperty property, const QString &value) -> void;

signals:
  auto removeItem() -> void;
  auto itemActivated(const QString &fileName) -> void;
  auto contextMenuShown(const QPoint &globalPos, const QString &fileName) -> void;

protected:
  auto keyPressEvent(QKeyEvent *e) -> void override;

private:
  auto onItemActivated(const QModelIndex &index) -> void;
  auto showContextMenu(const QPoint &pos) -> void;
  auto addUndoCommand(const QModelIndex &nodeIndex, NodeProperty property, const QString &before, const QString &after) -> void;

  RelativeResourceModel *m_qrcModel;
  QUndoStack *m_history;
  int m_mergeId;
};

} // namespace Internal
} // namespace ResourceEditor
