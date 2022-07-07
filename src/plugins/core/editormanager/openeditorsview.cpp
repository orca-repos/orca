// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "openeditorsview.hpp"
#include "editormanager.hpp"
#include "ieditor.hpp"
#include "documentmodel.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>

#include <utils/qtcassert.hpp>

#include <QApplication>
#include <QMenu>

using namespace Core;
using namespace Internal;

////
// OpenEditorsWidget
////

OpenEditorsWidget::OpenEditorsWidget()
{
  setWindowTitle(tr("Open Documents"));
  setDragEnabled(true);
  setDragDropMode(DragOnly);

  m_model = new ProxyModel(this);
  m_model->setSourceModel(DocumentModel::model());
  OpenDocumentsTreeView::setModel(m_model);

  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(EditorManager::instance(), &EditorManager::currentEditorChanged, this, &OpenEditorsWidget::updateCurrentItem);
  connect(this, &OpenDocumentsTreeView::activated, this, &OpenEditorsWidget::handleActivated);
  connect(this, &OpenDocumentsTreeView::closeActivated, this, &OpenEditorsWidget::closeDocument);
  connect(this, &OpenDocumentsTreeView::customContextMenuRequested, this, &OpenEditorsWidget::contextMenuRequested);
  updateCurrentItem(EditorManager::currentEditor());
}

OpenEditorsWidget::~OpenEditorsWidget() = default;

auto OpenEditorsWidget::updateCurrentItem(const IEditor *editor) -> void
{
  if (!editor) {
    clearSelection();
    return;
  }

  if (const auto index = DocumentModel::indexOfDocument(editor->document()); QTC_GUARD(index))
    setCurrentIndex(m_model->index(index.value(), 0));

  selectionModel()->select(currentIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  scrollTo(currentIndex());
}

auto OpenEditorsWidget::handleActivated(const QModelIndex &index) -> void
{
  if (index.column() == 0) {
    activateEditor(index);
  } else if (index.column() == 1) {
    // the funky close button
    closeDocument(index);

    // work around a bug in itemviews where the delegate wouldn't get the QStyle::State_MouseOver
    const auto cursor_pos = QCursor::pos();
    const auto vp = viewport();

    QMouseEvent e(QEvent::MouseMove, vp->mapFromGlobal(cursor_pos), cursor_pos, Qt::NoButton, {}, {});
    QCoreApplication::sendEvent(vp, &e);
  }
}

auto OpenEditorsWidget::activateEditor(const QModelIndex &index) const -> void
{
  selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  EditorManager::activateEditorForEntry(DocumentModel::entryAtRow(m_model->mapToSource(index).row()));
}

auto OpenEditorsWidget::closeDocument(const QModelIndex &index) -> void
{
  EditorManager::closeDocuments({DocumentModel::entryAtRow(m_model->mapToSource(index).row())});
  // work around selection changes
  updateCurrentItem(EditorManager::currentEditor());
}

auto OpenEditorsWidget::contextMenuRequested(const QPoint pos) const -> void
{
  QMenu context_menu;
  const auto editor_index = indexAt(pos);
  const auto row = m_model->mapToSource(editor_index).row();
  const auto entry = DocumentModel::entryAtRow(row);
  EditorManager::addSaveAndCloseEditorActions(&context_menu, entry);
  context_menu.addSeparator();
  EditorManager::addPinEditorActions(&context_menu, entry);
  context_menu.addSeparator();
  EditorManager::addNativeDirAndOpenWithActions(&context_menu, entry);
  context_menu.exec(mapToGlobal(pos));
}

///
// OpenEditorsViewFactory
///

OpenEditorsViewFactory::OpenEditorsViewFactory()
{
  setId("Open Documents");
  setDisplayName(OpenEditorsWidget::tr("Open Documents"));

  if constexpr (use_mac_shortcuts)
    setActivationSequence(QKeySequence(OpenEditorsWidget::tr("Meta+O")));
  else
    setActivationSequence(QKeySequence(OpenEditorsWidget::tr("Alt+O")));

  setPriority(200);
}

auto OpenEditorsViewFactory::createWidget() -> NavigationView
{
  return {new OpenEditorsWidget, {}};
}

ProxyModel::ProxyModel(QObject *parent) : QAbstractProxyModel(parent) {}

auto ProxyModel::mapFromSource(const QModelIndex &source_index) const -> QModelIndex
{
  // root
  if (!source_index.isValid())
    return {};

  // hide the <no document>
  const auto row = source_index.row() - 1;

  if (row < 0)
    return {};

  return createIndex(row, source_index.column());
}

auto ProxyModel::mapToSource(const QModelIndex &proxy_index) const -> QModelIndex
{
  if (!proxy_index.isValid())
    return {};

  // handle missing <no document>
  return sourceModel()->index(proxy_index.row() + 1, proxy_index.column());
}

auto ProxyModel::index(const int row, const int column, const QModelIndex &parent) const -> QModelIndex
{
  if (parent.isValid() || row < 0 || row >= sourceModel()->rowCount(mapToSource(parent)) - 1 || column < 0 || column > 1)
    return {};

  return createIndex(row, column);
}

auto ProxyModel::parent(const QModelIndex &child) const -> QModelIndex
{
  Q_UNUSED(child)
  return {};
}

auto ProxyModel::rowCount(const QModelIndex &parent) const -> int
{
  if (!parent.isValid())
    return sourceModel()->rowCount(mapToSource(parent)) - 1;

  return 0;
}

auto ProxyModel::columnCount(const QModelIndex &parent) const -> int
{
  return sourceModel()->columnCount(mapToSource(parent));
}

auto ProxyModel::setSourceModel(QAbstractItemModel *source_model) -> void
{
  if (const auto previous_model = this->sourceModel()) {
    disconnect(previous_model, &QAbstractItemModel::dataChanged, this, &ProxyModel::sourceDataChanged);
    disconnect(previous_model, &QAbstractItemModel::rowsInserted, this, &ProxyModel::sourceRowsInserted);
    disconnect(previous_model, &QAbstractItemModel::rowsRemoved, this, &ProxyModel::sourceRowsRemoved);
    disconnect(previous_model, &QAbstractItemModel::rowsAboutToBeInserted, this, &ProxyModel::sourceRowsAboutToBeInserted);
    disconnect(previous_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ProxyModel::sourceRowsAboutToBeRemoved);
  }

  QAbstractProxyModel::setSourceModel(source_model);

  if (source_model) {
    connect(source_model, &QAbstractItemModel::dataChanged, this, &ProxyModel::sourceDataChanged);
    connect(source_model, &QAbstractItemModel::rowsInserted, this, &ProxyModel::sourceRowsInserted);
    connect(source_model, &QAbstractItemModel::rowsRemoved, this, &ProxyModel::sourceRowsRemoved);
    connect(source_model, &QAbstractItemModel::rowsAboutToBeInserted, this, &ProxyModel::sourceRowsAboutToBeInserted);
    connect(source_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ProxyModel::sourceRowsAboutToBeRemoved);
  }
}

auto ProxyModel::sibling(const int row, const int column, const QModelIndex &idx) const -> QModelIndex
{
  return QAbstractProxyModel::sibling(row, column, idx);
}

auto ProxyModel::supportedDragActions() const -> Qt::DropActions
{
  return sourceModel()->supportedDragActions();
}

auto ProxyModel::sourceDataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right) -> void
{
  auto top_left_index = mapFromSource(top_left);

  if (!top_left_index.isValid())
    top_left_index = index(0, top_left.column());

  auto bottom_right_index = mapFromSource(bottom_right);

  if (!bottom_right_index.isValid())
    bottom_right_index = index(0, bottom_right.column());

  emit dataChanged(top_left_index, bottom_right_index);
}

auto ProxyModel::sourceRowsRemoved(const QModelIndex &parent, const int start, const int end) -> void
{
  Q_UNUSED(parent)
  Q_UNUSED(start)
  Q_UNUSED(end)
  endRemoveRows();
}

auto ProxyModel::sourceRowsInserted(const QModelIndex &parent, const int start, const int end) -> void
{
  Q_UNUSED(parent)
  Q_UNUSED(start)
  Q_UNUSED(end)
  endInsertRows();
}

auto ProxyModel::sourceRowsAboutToBeRemoved(const QModelIndex &parent, const int start, const int end) -> void
{
  const auto real_start = parent.isValid() || start == 0 ? start : start - 1;
  const auto real_end = parent.isValid() || end == 0 ? end : end - 1;

  beginRemoveRows(parent, real_start, real_end);
}

auto ProxyModel::sourceRowsAboutToBeInserted(const QModelIndex &parent, const int start, const int end) -> void
{
  const auto real_start = parent.isValid() || start == 0 ? start : start - 1;
  const auto real_end = parent.isValid() || end == 0 ? end : end - 1;

  beginInsertRows(parent, real_start, real_end);
}
