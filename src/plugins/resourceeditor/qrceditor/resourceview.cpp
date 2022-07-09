// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "resourceview.hpp"

#include "undocommands_p.hpp"

#include <core/fileutils.hpp>
#include <core/icore.hpp>

#include <QDebug>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QUndoStack>

namespace ResourceEditor {
namespace Internal {

ResourceView::ResourceView(RelativeResourceModel *model, QUndoStack *history, QWidget *parent) : Utils::TreeView(parent), m_qrcModel(model), m_history(history), m_mergeId(-1)
{
  advanceMergeId();
  setModel(m_qrcModel);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setEditTriggers(EditKeyPressed);

  header()->hide();

  connect(this, &QWidget::customContextMenuRequested, this, &ResourceView::showContextMenu);
  connect(this, &QAbstractItemView::activated, this, &ResourceView::onItemActivated);
}

ResourceView::~ResourceView() = default;

auto ResourceView::findSamePlacePostDeletionModelIndex(int &row, QModelIndex &parent) const -> void
{
  // Concept:
  // - Make selection stay on same Y level
  // - Enable user to hit delete several times in row
  const auto hasLowerBrother = m_qrcModel->hasIndex(row + 1, 0, parent);
  if (hasLowerBrother) {
    // First or mid child -> lower brother
    //  o
    //  +--o
    //  +-[o]  <-- deleted
    //  +--o   <-- chosen
    //  o
    // --> return unmodified
  } else {
    if (parent == QModelIndex()) {
      // Last prefix node
      if (row == 0) {
        // Last and only prefix node
        // [o]  <-- deleted
        //  +--o
        //  +--o
        row = -1;
        parent = QModelIndex();
      } else {
        const auto upperBrother = m_qrcModel->index(row - 1, 0, parent);
        if (m_qrcModel->hasChildren(upperBrother)) {
          //  o
          //  +--o  <-- selected
          // [o]    <-- deleted
          row = m_qrcModel->rowCount(upperBrother) - 1;
          parent = upperBrother;
        } else {
          //  o
          //  o  <-- selected
          // [o] <-- deleted
          row--;
        }
      }
    } else {
      // Last file node
      const auto hasPrefixBelow = m_qrcModel->hasIndex(parent.row() + 1, parent.column(), QModelIndex());
      if (hasPrefixBelow) {
        // Last child or parent with lower brother -> lower brother of parent
        //  o
        //  +--o
        //  +-[o]  <-- deleted
        //  o      <-- chosen
        row = parent.row() + 1;
        parent = QModelIndex();
      } else {
        const auto onlyChild = row == 0;
        if (onlyChild) {
          // Last and only child of last parent -> parent
          //  o      <-- chosen
          //  +-[o]  <-- deleted
          row = parent.row();
          parent = m_qrcModel->parent(parent);
        } else {
          // Last child of last parent -> upper brother
          //  o
          //  +--o   <-- chosen
          //  +-[o]  <-- deleted
          row--;
        }
      }
    }
  }
}

auto ResourceView::removeEntry(const QModelIndex &index) -> EntryBackup*
{
  Q_ASSERT(m_qrcModel);
  return m_qrcModel->removeEntry(index);
}

auto ResourceView::existingFilesSubtracted(int prefixIndex, const QStringList &fileNames) const -> QStringList
{
  return m_qrcModel->existingFilesSubtracted(prefixIndex, fileNames);
}

auto ResourceView::addFiles(int prefixIndex, const QStringList &fileNames, int cursorFile, int &firstFile, int &lastFile) -> void
{
  Q_ASSERT(m_qrcModel);
  m_qrcModel->addFiles(prefixIndex, fileNames, cursorFile, firstFile, lastFile);

  // Expand prefix node
  const auto prefixModelIndex = m_qrcModel->index(prefixIndex, 0, QModelIndex());
  if (prefixModelIndex.isValid())
    this->setExpanded(prefixModelIndex, true);
}

auto ResourceView::removeFiles(int prefixIndex, int firstFileIndex, int lastFileIndex) -> void
{
  Q_ASSERT(prefixIndex >= 0 && prefixIndex < m_qrcModel->rowCount(QModelIndex()));
  const auto prefixModelIndex = m_qrcModel->index(prefixIndex, 0, QModelIndex());
  Q_ASSERT(prefixModelIndex != QModelIndex());
  Q_ASSERT(firstFileIndex >= 0 && firstFileIndex < m_qrcModel->rowCount(prefixModelIndex));
  Q_ASSERT(lastFileIndex >= 0 && lastFileIndex < m_qrcModel->rowCount(prefixModelIndex));

  for (auto i = lastFileIndex; i >= firstFileIndex; i--) {
    const auto index = m_qrcModel->index(i, 0, prefixModelIndex);
    delete removeEntry(index);
  }
}

auto ResourceView::keyPressEvent(QKeyEvent *e) -> void
{
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) emit removeItem();
  else
    Utils::TreeView::keyPressEvent(e);
}

auto ResourceView::addPrefix() -> QModelIndex
{
  const auto idx = m_qrcModel->addNewPrefix();
  selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
  return idx;
}

auto ResourceView::nonExistingFiles() -> QList<QModelIndex>
{
  return m_qrcModel->nonExistingFiles();
}

auto ResourceView::refresh() -> void
{
  m_qrcModel->refresh();
  const auto idx = currentIndex();
  setModel(nullptr);
  setModel(m_qrcModel);
  setCurrentIndex(idx);
  expandAll();
}

auto ResourceView::fileNamesToAdd() -> QStringList
{
  return QFileDialog::getOpenFileNames(this, tr("Open File"), m_qrcModel->absolutePath(QString()), tr("All files (*)"));
}

auto ResourceView::currentAlias() const -> QString
{
  const auto current = currentIndex();
  if (!current.isValid())
    return QString();
  return m_qrcModel->alias(current);
}

auto ResourceView::currentPrefix() const -> QString
{
  const auto current = currentIndex();
  if (!current.isValid())
    return QString();
  const auto preindex = m_qrcModel->prefixIndex(current);
  QString prefix, file;
  m_qrcModel->getItem(preindex, prefix, file);
  return prefix;
}

auto ResourceView::currentLanguage() const -> QString
{
  const auto current = currentIndex();
  if (!current.isValid())
    return QString();
  const auto preindex = m_qrcModel->prefixIndex(current);
  return m_qrcModel->lang(preindex);
}

auto ResourceView::currentResourcePath() const -> QString
{
  const auto current = currentIndex();
  if (!current.isValid())
    return QString();

  const auto alias = m_qrcModel->alias(current);
  if (!alias.isEmpty())
    return QLatin1Char(':') + currentPrefix() + QLatin1Char('/') + alias;

  return QLatin1Char(':') + currentPrefix() + QLatin1Char('/') + m_qrcModel->relativePath(m_qrcModel->file(current));
}

auto ResourceView::getCurrentValue(NodeProperty property) const -> QString
{
  switch (property) {
  case AliasProperty:
    return currentAlias();
  case PrefixProperty:
    return currentPrefix();
  case LanguageProperty:
    return currentLanguage();
  default: Q_ASSERT(false);
    return QString(); // Kill warning
  }
}

auto ResourceView::changeValue(const QModelIndex &nodeIndex, NodeProperty property, const QString &value) -> void
{
  switch (property) {
  case AliasProperty:
    m_qrcModel->changeAlias(nodeIndex, value);
    return;
  case PrefixProperty:
    m_qrcModel->changePrefix(nodeIndex, value);
    return;
  case LanguageProperty:
    m_qrcModel->changeLang(nodeIndex, value);
    return;
  default: Q_ASSERT(false);
  }
}

auto ResourceView::onItemActivated(const QModelIndex &index) -> void
{
  const auto fileName = m_qrcModel->file(index);
  if (fileName.isEmpty())
    return;
  emit itemActivated(fileName);
}

auto ResourceView::showContextMenu(const QPoint &pos) -> void
{
  const auto index = indexAt(pos);
  const auto fileName = m_qrcModel->file(index);
  if (fileName.isEmpty())
    return;
  emit contextMenuShown(mapToGlobal(pos), fileName);
}

auto ResourceView::advanceMergeId() -> void
{
  m_mergeId++;
  if (m_mergeId < 0)
    m_mergeId = 0;
}

auto ResourceView::addUndoCommand(const QModelIndex &nodeIndex, NodeProperty property, const QString &before, const QString &after) -> void
{
  QUndoCommand *const command = new ModifyPropertyCommand(this, nodeIndex, property, m_mergeId, before, after);
  m_history->push(command);
}

auto ResourceView::setCurrentAlias(const QString &before, const QString &after) -> void
{
  const auto current = currentIndex();
  if (!current.isValid())
    return;

  addUndoCommand(current, AliasProperty, before, after);
}

auto ResourceView::setCurrentPrefix(const QString &before, const QString &after) -> void
{
  const auto current = currentIndex();
  if (!current.isValid())
    return;
  const auto preindex = m_qrcModel->prefixIndex(current);

  addUndoCommand(preindex, PrefixProperty, before, after);
}

auto ResourceView::setCurrentLanguage(const QString &before, const QString &after) -> void
{
  const auto current = currentIndex();
  if (!current.isValid())
    return;
  const auto preindex = m_qrcModel->prefixIndex(current);

  addUndoCommand(preindex, LanguageProperty, before, after);
}

auto ResourceView::isPrefix(const QModelIndex &index) const -> bool
{
  if (!index.isValid())
    return false;
  const auto preindex = m_qrcModel->prefixIndex(index);
  if (preindex == index)
    return true;
  return false;
}

auto ResourceView::filePath() const -> Utils::FilePath
{
  return m_qrcModel->filePath();
}

auto ResourceView::setResourceDragEnabled(bool e) -> void
{
  setDragEnabled(e);
  m_qrcModel->setResourceDragEnabled(e);
}

auto ResourceView::resourceDragEnabled() const -> bool
{
  return m_qrcModel->resourceDragEnabled();
}

} // Internal
} // ResourceEditor
