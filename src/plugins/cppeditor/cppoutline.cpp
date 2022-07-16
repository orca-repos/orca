// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppoutline.hpp"

#include "cppeditoroutline.hpp"
#include "cppmodelmanager.hpp"
#include "cppoverviewmodel.hpp"

#include <core/core-item-view-find.hpp>
#include <core/core-editor-manager.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/linecolumn.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QVBoxLayout>
#include <QMenu>

namespace CppEditor {
namespace Internal {

CppOutlineTreeView::CppOutlineTreeView(QWidget *parent) : Utils::NavigationTreeView(parent)
{
  setExpandsOnDoubleClick(false);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
}

auto CppOutlineTreeView::contextMenuEvent(QContextMenuEvent *event) -> void
{
  if (!event)
    return;

  QMenu contextMenu;

  auto action = contextMenu.addAction(tr("Expand All"));
  connect(action, &QAction::triggered, this, &QTreeView::expandAll);
  action = contextMenu.addAction(tr("Collapse All"));
  connect(action, &QAction::triggered, this, &QTreeView::collapseAll);

  contextMenu.exec(event->globalPos());

  event->accept();
}

CppOutlineFilterModel::CppOutlineFilterModel(AbstractOverviewModel &sourceModel, QObject *parent) : QSortFilterProxyModel(parent), m_sourceModel(sourceModel) {}

auto CppOutlineFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const -> bool
{
  // ignore artifical "<Select Symbol>" entry
  if (!sourceParent.isValid() && sourceRow == 0)
    return false;
  // ignore generated symbols, e.g. by macro expansion (Q_OBJECT)
  const auto sourceIndex = m_sourceModel.index(sourceRow, 0, sourceParent);
  if (m_sourceModel.isGenerated(sourceIndex))
    return false;

  return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

auto CppOutlineFilterModel::supportedDragActions() const -> Qt::DropActions
{
  return sourceModel()->supportedDragActions();
}

CppOutlineWidget::CppOutlineWidget(CppEditorWidget *editor) : m_editor(editor), m_treeView(new CppOutlineTreeView(this)), m_enableCursorSync(true), m_blockCursorSync(false), m_sorted(false)
{
  auto model = m_editor->outline()->model();
  m_proxyModel = new CppOutlineFilterModel(*model, this);
  m_proxyModel->setSourceModel(model);

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(Orca::Plugin::Core::ItemViewFind::createSearchableWrapper(m_treeView));
  setLayout(layout);

  m_treeView->setModel(m_proxyModel);
  m_treeView->setSortingEnabled(true);
  setFocusProxy(m_treeView);

  connect(model, &QAbstractItemModel::modelReset, this, &CppOutlineWidget::modelUpdated);
  modelUpdated();

  connect(m_editor->outline(), &CppEditorOutline::modelIndexChanged, this, &CppOutlineWidget::updateSelectionInTree);
  connect(m_treeView, &QAbstractItemView::activated, this, &CppOutlineWidget::onItemActivated);
}

auto CppOutlineWidget::filterMenuActions() const -> QList<QAction*>
{
  return QList<QAction*>();
}

auto CppOutlineWidget::setCursorSynchronization(bool syncWithCursor) -> void
{
  m_enableCursorSync = syncWithCursor;
  if (m_enableCursorSync)
    updateSelectionInTree(m_editor->outline()->modelIndex());
}

auto CppOutlineWidget::isSorted() const -> bool
{
  return m_sorted;
}

auto CppOutlineWidget::setSorted(bool sorted) -> void
{
  m_sorted = sorted;
  m_proxyModel->sort(m_sorted ? 0 : -1);
}

auto CppOutlineWidget::restoreSettings(const QVariantMap &map) -> void
{
  setSorted(map.value(QString("CppOutline.Sort"), false).toBool());
}

auto CppOutlineWidget::settings() const -> QVariantMap
{
  return {{QString("CppOutline.Sort"), m_sorted}};
}

auto CppOutlineWidget::modelUpdated() -> void
{
  m_treeView->expandAll();
}

auto CppOutlineWidget::updateSelectionInTree(const QModelIndex &index) -> void
{
  if (!syncCursor())
    return;

  auto proxyIndex = m_proxyModel->mapFromSource(index);

  m_blockCursorSync = true;
  m_treeView->setCurrentIndex(proxyIndex);
  m_treeView->scrollTo(proxyIndex);
  m_blockCursorSync = false;
}

auto CppOutlineWidget::updateTextCursor(const QModelIndex &proxyIndex) -> void
{
  auto index = m_proxyModel->mapToSource(proxyIndex);
  auto model = m_editor->outline()->model();
  auto lineColumn = model->lineColumnFromIndex(index);
  if (!lineColumn.isValid())
    return;

  m_blockCursorSync = true;

  Orca::Plugin::Core::EditorManager::cutForwardNavigationHistory();
  Orca::Plugin::Core::EditorManager::addCurrentPositionToNavigationHistory();

  // line has to be 1 based, column 0 based!
  m_editor->gotoLine(lineColumn.line, lineColumn.column - 1, true, true);
  m_blockCursorSync = false;
}

auto CppOutlineWidget::onItemActivated(const QModelIndex &index) -> void
{
  if (!index.isValid())
    return;

  updateTextCursor(index);
  m_editor->setFocus();
}

auto CppOutlineWidget::syncCursor() -> bool
{
  return m_enableCursorSync && !m_blockCursorSync;
}

auto CppOutlineWidgetFactory::supportsEditor(Orca::Plugin::Core::IEditor *editor) const -> bool
{
  const auto cppEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor);
  if (!cppEditor || !CppModelManager::isCppEditor(cppEditor))
    return false;
  return CppModelManager::supportsOutline(cppEditor->textDocument());
}

auto CppOutlineWidgetFactory::createWidget(Orca::Plugin::Core::IEditor *editor) -> TextEditor::IOutlineWidget*
{
  const auto cppEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor);
  QTC_ASSERT(cppEditor, return nullptr);
  const auto cppEditorWidget = qobject_cast<CppEditorWidget*>(cppEditor->widget());
  QTC_ASSERT(cppEditorWidget, return nullptr);

  return new CppOutlineWidget(cppEditorWidget);
}

} // namespace Internal
} // namespace CppEditor
