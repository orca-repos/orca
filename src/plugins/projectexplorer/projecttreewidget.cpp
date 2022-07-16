// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projecttreewidget.hpp"

#include "projectexplorer.hpp"
#include "projectnodes.hpp"
#include "project.hpp"
#include "session.hpp"
#include "projectmodels.hpp"
#include "projecttree.hpp"

#include <core/core-action-manager.hpp>
#include <core/core-command.hpp>
#include <core/core-document-manager.hpp>
#include <core/core-interface.hpp>
#include <core/core-document-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-item-view-find.hpp>

#include <utils/algorithm.hpp>
#include <utils/navigationtreeview.hpp>
#include <utils/progressindicator.hpp>
#include <utils/tooltip/tooltip.hpp>
#include <utils/utilsicons.hpp>

#include <QApplication>
#include <QSettings>

#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QToolButton>
#include <QPainter>
#include <QAction>
#include <QLineEdit>
#include <QMenu>

#include <memory>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace ProjectExplorer::Internal;
using namespace Utils;

QList<ProjectTreeWidget *> ProjectTreeWidget::m_projectTreeWidgets;

namespace {

class ProjectTreeItemDelegate : public QStyledItemDelegate {
public:
  ProjectTreeItemDelegate(QTreeView *view) : QStyledItemDelegate(view), m_view(view)
  {
    connect(m_view->model(), &QAbstractItemModel::modelReset, this, &ProjectTreeItemDelegate::deleteAllIndicators);

    // Actually this only needs to delete the indicators in the effected rows and *after* it,
    // but just be lazy and nuke all the indicators.
    connect(m_view->model(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &ProjectTreeItemDelegate::deleteAllIndicators);
    connect(m_view->model(), &QAbstractItemModel::rowsAboutToBeInserted, this, &ProjectTreeItemDelegate::deleteAllIndicators);
  }

  ~ProjectTreeItemDelegate() override
  {
    deleteAllIndicators();
  }

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override
  {
    QStyledItemDelegate::paint(painter, option, index);

    if (index.data(Project::isParsingRole).toBool()) {
      auto opt = option;
      initStyleOption(&opt, index);
      const auto indicator = findOrCreateIndicatorPainter(index);

      const auto style = option.widget ? option.widget->style() : QApplication::style();
      const auto rect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);

      indicator->paint(*painter, rect);
    } else {
      delete m_indicators.value(index);
      m_indicators.remove(index);
    }
  }

private:
  auto findOrCreateIndicatorPainter(const QModelIndex &index) const -> ProgressIndicatorPainter*
  {
    auto indicator = m_indicators.value(index);
    if (!indicator)
      indicator = createIndicatorPainter(index);
    return indicator;
  }

  auto createIndicatorPainter(const QModelIndex &index) const -> ProgressIndicatorPainter*
  {
    const auto indicator = new ProgressIndicatorPainter(ProgressIndicatorSize::Small);
    indicator->setUpdateCallback([index, this]() { m_view->update(index); });
    indicator->startAnimation();
    m_indicators.insert(index, indicator);
    return indicator;
  }

  auto deleteAllIndicators() -> void
  {
    qDeleteAll(m_indicators);
    m_indicators.clear();
  }

  mutable QHash<QModelIndex, ProgressIndicatorPainter*> m_indicators;
  QTreeView *m_view;
};

bool debug = false;
}

class ProjectTreeView : public NavigationTreeView {
public:
  ProjectTreeView()
  {
    setEditTriggers(EditKeyPressed);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragEnabled(true);
    setDragDropMode(DragDrop);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    const auto context = new IContext(this);
    context->setContext(Context(ProjectExplorer::Constants::C_PROJECT_TREE));
    context->setWidget(this);

    ICore::addContextObject(context);

    connect(this, &ProjectTreeView::expanded, this, &ProjectTreeView::invalidateSize);
    connect(this, &ProjectTreeView::collapsed, this, &ProjectTreeView::invalidateSize);
  }

  auto invalidateSize() -> void
  {
    m_cachedSize = -1;
  }

  auto setModel(QAbstractItemModel *newModel) -> void override
  {
    // Note: Don't connect to column signals, as we have only one column
    if (model()) {
      const auto m = model();
      disconnect(m, &QAbstractItemModel::dataChanged, this, &ProjectTreeView::invalidateSize);
      disconnect(m, &QAbstractItemModel::layoutChanged, this, &ProjectTreeView::invalidateSize);
      disconnect(m, &QAbstractItemModel::modelReset, this, &ProjectTreeView::invalidateSize);
      disconnect(m, &QAbstractItemModel::rowsInserted, this, &ProjectTreeView::invalidateSize);
      disconnect(m, &QAbstractItemModel::rowsMoved, this, &ProjectTreeView::invalidateSize);
      disconnect(m, &QAbstractItemModel::rowsRemoved, this, &ProjectTreeView::invalidateSize);
    }
    if (newModel) {
      connect(newModel, &QAbstractItemModel::dataChanged, this, &ProjectTreeView::invalidateSize);
      connect(newModel, &QAbstractItemModel::layoutChanged, this, &ProjectTreeView::invalidateSize);
      connect(newModel, &QAbstractItemModel::modelReset, this, &ProjectTreeView::invalidateSize);
      connect(newModel, &QAbstractItemModel::rowsInserted, this, &ProjectTreeView::invalidateSize);
      connect(newModel, &QAbstractItemModel::rowsMoved, this, &ProjectTreeView::invalidateSize);
      connect(newModel, &QAbstractItemModel::rowsRemoved, this, &ProjectTreeView::invalidateSize);
    }
    NavigationTreeView::setModel(newModel);
  }

  auto sizeHintForColumn(int column) const -> int override
  {
    if (m_cachedSize < 0)
      m_cachedSize = NavigationTreeView::sizeHintForColumn(column);

    return m_cachedSize;
  }

private:
  mutable int m_cachedSize = -1;
};

/*!
  /class ProjectTreeWidget

  Shows the projects in form of a tree.
  */
ProjectTreeWidget::ProjectTreeWidget(QWidget *parent) : QWidget(parent)
{
  // We keep one instance per tree as this also manages the
  // simple/non-simple etc state which is per tree.
  m_model = new FlatModel(this);
  m_view = new ProjectTreeView;
  m_view->setModel(m_model);
  m_view->setItemDelegate(new ProjectTreeItemDelegate(m_view));
  setFocusProxy(m_view);
  m_view->installEventFilter(this);

  const auto layout = new QVBoxLayout();
  layout->addWidget(ItemViewFind::createSearchableWrapper(m_view, ItemViewFind::DarkColored, ItemViewFind::FetchMoreWhileSearching));
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  m_filterProjectsAction = new QAction(tr("Simplify Tree"), this);
  m_filterProjectsAction->setCheckable(true);
  m_filterProjectsAction->setChecked(false); // default is the traditional complex tree
  connect(m_filterProjectsAction, &QAction::toggled, this, &ProjectTreeWidget::setProjectFilter);

  m_filterGeneratedFilesAction = new QAction(tr("Hide Generated Files"), this);
  m_filterGeneratedFilesAction->setCheckable(true);
  m_filterGeneratedFilesAction->setChecked(true);
  connect(m_filterGeneratedFilesAction, &QAction::toggled, this, &ProjectTreeWidget::setGeneratedFilesFilter);

  m_filterDisabledFilesAction = new QAction(tr("Hide Disabled Files"), this);
  m_filterDisabledFilesAction->setCheckable(true);
  m_filterDisabledFilesAction->setChecked(false);
  connect(m_filterDisabledFilesAction, &QAction::toggled, this, &ProjectTreeWidget::setDisabledFilesFilter);

  const char focusActionId[] = "ProjectExplorer.FocusDocumentInProjectTree";
  if (!ActionManager::command(focusActionId)) {
    const auto focusDocumentInProjectTree = new QAction(tr("Focus Document in Project Tree"), this);
    const auto cmd = ActionManager::registerAction(focusDocumentInProjectTree, focusActionId);
    cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? tr("Meta+Shift+L") : tr("Alt+Shift+L")));
    connect(focusDocumentInProjectTree, &QAction::triggered, this, [this]() {
      syncFromDocumentManager();
    });
  }

  m_trimEmptyDirectoriesAction = new QAction(tr("Hide Empty Directories"), this);
  m_trimEmptyDirectoriesAction->setCheckable(true);
  m_trimEmptyDirectoriesAction->setChecked(true);
  connect(m_trimEmptyDirectoriesAction, &QAction::toggled, this, &ProjectTreeWidget::setTrimEmptyDirectories);

  m_hideSourceGroupsAction = new QAction(tr("Hide Source and Header Groups"), this);
  m_hideSourceGroupsAction->setCheckable(true);
  m_hideSourceGroupsAction->setChecked(false);
  connect(m_hideSourceGroupsAction, &QAction::toggled, this, &ProjectTreeWidget::setHideSourceGroups);

  // connections
  connect(m_model, &FlatModel::renamed, this, &ProjectTreeWidget::renamed);
  connect(m_model, &FlatModel::requestExpansion, m_view, &QTreeView::expand);
  connect(m_view, &QAbstractItemView::activated, this, &ProjectTreeWidget::openItem);
  connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &ProjectTreeWidget::handleCurrentItemChange);
  connect(m_view, &QWidget::customContextMenuRequested, this, &ProjectTreeWidget::showContextMenu);
  connect(m_view, &QTreeView::expanded, m_model, &FlatModel::onExpanded);
  connect(m_view, &QTreeView::collapsed, m_model, &FlatModel::onCollapsed);

  m_toggleSync = new QAction(this);
  m_toggleSync->setIcon(Icons::LINK_TOOLBAR.icon());
  m_toggleSync->setCheckable(true);
  m_toggleSync->setChecked(autoSynchronization());
  m_toggleSync->setToolTip(tr("Synchronize with Editor"));
  connect(m_toggleSync, &QAction::triggered, this, &ProjectTreeWidget::toggleAutoSynchronization);

  setCurrentItem(ProjectTree::currentNode());
  setAutoSynchronization(true);

  m_projectTreeWidgets << this;
  ProjectTree::registerWidget(this);
}

ProjectTreeWidget::~ProjectTreeWidget()
{
  m_projectTreeWidgets.removeOne(this);
  ProjectTree::unregisterWidget(this);
}

// returns how many nodes need to be expanded to make node visible
auto ProjectTreeWidget::expandedCount(Node *node) -> int
{
  if (m_projectTreeWidgets.isEmpty())
    return 0;
  const auto model = m_projectTreeWidgets.first()->m_model;
  const auto index = model->indexForNode(node);
  if (!index.isValid())
    return 0;

  auto count = 0;
  foreach(ProjectTreeWidget *tree, m_projectTreeWidgets) {
    auto idx = index;
    while (idx.isValid() && idx != tree->m_view->rootIndex()) {
      if (!tree->m_view->isExpanded(idx))
        ++count;
      idx = model->parent(idx);
    }
  }
  return count;
}

auto ProjectTreeWidget::rowsInserted(const QModelIndex &parent, int start, int end) -> void
{
  if (m_delayedRename.isEmpty())
    return;
  const auto node = m_model->nodeForIndex(parent);
  QTC_ASSERT(node, return);
  for (auto i = start; i <= end && !m_delayedRename.isEmpty(); ++i) {
    auto idx = m_model->index(i, 0, parent);
    const auto n = m_model->nodeForIndex(idx);
    if (!n)
      continue;
    const int renameIdx = m_delayedRename.indexOf(n->filePath());
    if (renameIdx != -1) {
      m_view->setCurrentIndex(idx);
      m_delayedRename.removeAt(renameIdx);
    }
  }
}

auto ProjectTreeWidget::nodeForFile(const FilePath &fileName) -> Node*
{
  if (fileName.isEmpty())
    return nullptr;
  Node *bestNode = nullptr;
  auto bestNodeExpandCount = INT_MAX;

  // FIXME: Looks like this could be done with less cycles.
  for (const auto project : SessionManager::projects()) {
    if (const auto projectNode = project->rootProjectNode()) {
      projectNode->forEachGenericNode([&](Node *node) {
        if (node->filePath() == fileName) {
          if (!bestNode || node->priority() < bestNode->priority()) {
            bestNode = node;
            bestNodeExpandCount = expandedCount(node);
          } else if (node->priority() == bestNode->priority()) {
            const auto nodeExpandCount = expandedCount(node);
            if (nodeExpandCount < bestNodeExpandCount) {
              bestNode = node;
              bestNodeExpandCount = expandedCount(node);
            }
          }
        }
      });
    }
  }

  return bestNode;
}

auto ProjectTreeWidget::toggleAutoSynchronization() -> void
{
  setAutoSynchronization(!m_autoSync);
}

auto ProjectTreeWidget::autoSynchronization() const -> bool
{
  return m_autoSync;
}

auto ProjectTreeWidget::setAutoSynchronization(bool sync) -> void
{
  m_toggleSync->setChecked(sync);
  if (sync == m_autoSync)
    return;

  m_autoSync = sync;

  if (debug)
    qDebug() << (m_autoSync ? "Enabling auto synchronization" : "Disabling auto synchronization");

  if (m_autoSync)
    syncFromDocumentManager();
}

auto ProjectTreeWidget::expandNodeRecursively(const QModelIndex &index) -> void
{
  const auto rc = index.model()->rowCount(index);
  for (auto i = 0; i < rc; ++i)
    expandNodeRecursively(index.model()->index(i, index.column(), index));
  if (rc > 0)
    m_view->expand(index);
}

auto ProjectTreeWidget::expandCurrentNodeRecursively() -> void
{
  expandNodeRecursively(m_view->currentIndex());
}

auto ProjectTreeWidget::collapseAll() -> void
{
  m_view->collapseAll();
}

auto ProjectTreeWidget::expandAll() -> void
{
  m_view->expandAll();
}

auto ProjectTreeWidget::createToolButtons() -> QList<QToolButton*>
{
  auto filter = new QToolButton(this);
  filter->setIcon(Icons::FILTER.icon());
  filter->setToolTip(tr("Filter Tree"));
  filter->setPopupMode(QToolButton::InstantPopup);
  filter->setProperty("noArrow", true);

  const auto filterMenu = new QMenu(filter);
  filterMenu->addAction(m_filterProjectsAction);
  filterMenu->addAction(m_filterGeneratedFilesAction);
  filterMenu->addAction(m_filterDisabledFilesAction);
  filterMenu->addAction(m_trimEmptyDirectoriesAction);
  filterMenu->addAction(m_hideSourceGroupsAction);
  filter->setMenu(filterMenu);

  auto toggleSync = new QToolButton;
  toggleSync->setDefaultAction(m_toggleSync);

  return {filter, toggleSync};
}

auto ProjectTreeWidget::editCurrentItem() -> void
{
  m_delayedRename.clear();
  const auto currentIndex = m_view->selectionModel()->currentIndex();
  if (!currentIndex.isValid())
    return;

  m_view->edit(currentIndex);
  // Select complete file basename for renaming
  const Node *node = m_model->nodeForIndex(currentIndex);
  if (!node)
    return;
  auto *editor = qobject_cast<QLineEdit*>(m_view->indexWidget(currentIndex));
  if (!editor)
    return;

  const int dotIndex = FilePath::fromString(editor->text()).completeBaseName().length();
  if (dotIndex > 0)
    editor->setSelection(0, dotIndex);
}

auto ProjectTreeWidget::renamed(const FilePath &oldPath, const FilePath &newPath) -> void
{
  update();
  Q_UNUSED(oldPath)
  if (!currentNode() || currentNode()->filePath() != newPath) {
    // try to find the node
    const auto node = nodeForFile(newPath);
    if (node)
      m_view->setCurrentIndex(m_model->indexForNode(node));
    else
      m_delayedRename << newPath;
  }
}

auto ProjectTreeWidget::syncFromDocumentManager() -> void
{
  // sync from document manager
  FilePath fileName;
  if (const auto doc = EditorManager::currentDocument())
    fileName = doc->filePath();
  if (!currentNode() || currentNode()->filePath() != fileName)
    setCurrentItem(nodeForFile(fileName));
}

auto ProjectTreeWidget::setCurrentItem(Node *node) -> void
{
  const auto mainIndex = m_model->indexForNode(node);

  if (mainIndex.isValid()) {
    if (mainIndex != m_view->selectionModel()->currentIndex()) {
      // Expand everything between the index and the root index!
      auto parent = mainIndex.parent();
      while (parent.isValid()) {
        m_view->setExpanded(parent, true);
        parent = parent.parent();
      }
      m_view->setCurrentIndex(mainIndex);
      m_view->scrollTo(mainIndex);
    }
  } else {
    m_view->clearSelection();
    m_view->setCurrentIndex({});
  }
}

auto ProjectTreeWidget::handleCurrentItemChange(const QModelIndex &current) -> void
{
  Q_UNUSED(current)
  ProjectTree::nodeChanged(this);
}

auto ProjectTreeWidget::currentNode() -> Node*
{
  return m_model->nodeForIndex(m_view->currentIndex());
}

auto ProjectTreeWidget::sync(Node *node) -> void
{
  if (m_autoSync)
    setCurrentItem(node);
}

auto ProjectTreeWidget::showMessage(Node *node, const QString &message) -> void
{
  const auto idx = m_model->indexForNode(node);
  m_view->setCurrentIndex(idx);
  m_view->scrollTo(idx);

  auto pos = m_view->mapToGlobal(m_view->visualRect(idx).bottomLeft());
  pos -= ToolTip::offsetFromPosition();
  ToolTip::show(pos, message);
}

auto ProjectTreeWidget::showContextMenu(const QPoint &pos) -> void
{
  const auto index = m_view->indexAt(pos);
  const auto node = m_model->nodeForIndex(index);
  ProjectTree::showContextMenu(this, m_view->mapToGlobal(pos), node);
}

auto ProjectTreeWidget::openItem(const QModelIndex &mainIndex) -> void
{
  const auto node = m_model->nodeForIndex(mainIndex);
  if (!node || !node->asFileNode())
    return;
  const auto editor = EditorManager::openEditor(node->filePath(), {}, EditorManager::AllowExternalEditor);
  if (editor && node->line() >= 0)
    editor->gotoLine(node->line());
}

auto ProjectTreeWidget::setProjectFilter(bool filter) -> void
{
  m_model->setProjectFilterEnabled(filter);
  m_filterProjectsAction->setChecked(filter);
}

auto ProjectTreeWidget::setGeneratedFilesFilter(bool filter) -> void
{
  m_model->setGeneratedFilesFilterEnabled(filter);
  m_filterGeneratedFilesAction->setChecked(filter);
}

auto ProjectTreeWidget::setDisabledFilesFilter(bool filter) -> void
{
  m_model->setDisabledFilesFilterEnabled(filter);
  m_filterDisabledFilesAction->setChecked(filter);
}

auto ProjectTreeWidget::setTrimEmptyDirectories(bool filter) -> void
{
  m_model->setTrimEmptyDirectories(filter);
  m_trimEmptyDirectoriesAction->setChecked(filter);
}

auto ProjectTreeWidget::setHideSourceGroups(bool filter) -> void
{
  m_model->setHideSourceGroups(filter);
  m_hideSourceGroupsAction->setChecked(filter);
}

auto ProjectTreeWidget::generatedFilesFilter() -> bool
{
  return m_model->generatedFilesFilterEnabled();
}

auto ProjectTreeWidget::disabledFilesFilter() -> bool
{
  return m_model->disabledFilesFilterEnabled();
}

auto ProjectTreeWidget::trimEmptyDirectoriesFilter() -> bool
{
  return m_model->trimEmptyDirectoriesEnabled();
}

auto ProjectTreeWidget::hideSourceGroups() -> bool
{
  return m_model->hideSourceGroups();
}

auto ProjectTreeWidget::projectFilter() -> bool
{
  return m_model->projectFilterEnabled();
}

ProjectTreeWidgetFactory::ProjectTreeWidgetFactory()
{
  setDisplayName(tr("Projects"));
  setPriority(100);
  setId(Constants::PROJECTTREE_ID);
  setActivationSequence(QKeySequence(use_mac_shortcuts ? tr("Meta+X") : tr("Alt+X")));
}

auto ProjectTreeWidgetFactory::createWidget() -> NavigationView
{
  const auto ptw = new ProjectTreeWidget;
  return {ptw, ptw->createToolButtons()};
}

constexpr bool kProjectFilterDefault = false;
constexpr bool kHideGeneratedFilesDefault = true;
constexpr bool kHideDisabledFilesDefault = false;
constexpr bool kTrimEmptyDirsDefault = true;
constexpr bool kHideSourceGroupsDefault = false;
constexpr bool kSyncDefault = true;
constexpr char kBaseKey[] = "ProjectTreeWidget.";
constexpr char kProjectFilterKey[] = ".ProjectFilter";
constexpr char kHideGeneratedFilesKey[] = ".GeneratedFilter";
constexpr char kHideDisabledFilesKey[] = ".DisabledFilesFilter";
constexpr char kTrimEmptyDirsKey[] = ".TrimEmptyDirsFilter";
constexpr char kSyncKey[] = ".SyncWithEditor";
constexpr char kHideSourceGroupsKey[] = ".HideSourceGroups";

auto ProjectTreeWidgetFactory::saveSettings(QtcSettings *settings, int position, QWidget *widget) -> void
{
  const auto ptw = qobject_cast<ProjectTreeWidget*>(widget);
  Q_ASSERT(ptw);
  const QString baseKey = kBaseKey + QString::number(position);
  settings->setValueWithDefault(baseKey + kProjectFilterKey, ptw->projectFilter(), kProjectFilterDefault);
  settings->setValueWithDefault(baseKey + kHideGeneratedFilesKey, ptw->generatedFilesFilter(), kHideGeneratedFilesDefault);
  settings->setValueWithDefault(baseKey + kHideDisabledFilesKey, ptw->disabledFilesFilter(), kHideDisabledFilesDefault);
  settings->setValueWithDefault(baseKey + kTrimEmptyDirsKey, ptw->trimEmptyDirectoriesFilter(), kTrimEmptyDirsDefault);
  settings->setValueWithDefault(baseKey + kHideSourceGroupsKey, ptw->hideSourceGroups(), kHideSourceGroupsDefault);
  settings->setValueWithDefault(baseKey + kSyncKey, ptw->autoSynchronization(), kSyncDefault);
}

auto ProjectTreeWidgetFactory::restoreSettings(QSettings *settings, int position, QWidget *widget) -> void
{
  const auto ptw = qobject_cast<ProjectTreeWidget*>(widget);
  Q_ASSERT(ptw);
  const QString baseKey = kBaseKey + QString::number(position);
  ptw->setProjectFilter(settings->value(baseKey + kProjectFilterKey, kProjectFilterDefault).toBool());
  ptw->setGeneratedFilesFilter(settings->value(baseKey + kHideGeneratedFilesKey, kHideGeneratedFilesDefault).toBool());
  ptw->setDisabledFilesFilter(settings->value(baseKey + kHideDisabledFilesKey, kHideDisabledFilesDefault).toBool());
  ptw->setTrimEmptyDirectories(settings->value(baseKey + kTrimEmptyDirsKey, kTrimEmptyDirsDefault).toBool());
  ptw->setHideSourceGroups(settings->value(baseKey + kHideSourceGroupsKey, kHideSourceGroupsDefault).toBool());
  ptw->setAutoSynchronization(settings->value(baseKey + kSyncKey, kSyncDefault).toBool());
}
