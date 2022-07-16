// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpptypehierarchy.hpp"

#include "cppeditorconstants.hpp"
#include "cppeditorwidget.hpp"
#include "cppeditorplugin.hpp"
#include "cppelementevaluator.hpp"

#include <core/core-item-view-find.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-progress-manager.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/algorithm.hpp>
#include <utils/delegates.hpp>
#include <utils/dropsupport.hpp>
#include <utils/navigationtreeview.hpp>
#include <utils/progressindicator.hpp>

#include <QApplication>
#include <QLabel>
#include <QLatin1String>
#include <QMenu>
#include <QModelIndex>
#include <QStackedLayout>
#include <QVBoxLayout>

using namespace CppEditor;
using namespace CppEditor::Internal;
using namespace Utils;

namespace {

enum ItemRole {
  AnnotationRole = Qt::UserRole + 1,
  LinkRole
};

auto itemForClass(const CppClass &cppClass) -> QStandardItem*
{
  auto item = new QStandardItem;
  item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
  item->setData(cppClass.name, Qt::DisplayRole);
  if (cppClass.name != cppClass.qualifiedName)
    item->setData(cppClass.qualifiedName, AnnotationRole);
  item->setData(cppClass.icon, Qt::DecorationRole);
  QVariant link;
  link.setValue(Link(cppClass.link));
  item->setData(link, LinkRole);
  return item;
}

auto sortClasses(const QList<CppClass> &cppClasses) -> QList<CppClass>
{
  auto sorted = cppClasses;
  sort(sorted, [](const CppClass &c1, const CppClass &c2) -> bool {
    const QString key1 = c1.name + QLatin1String("::") + c1.qualifiedName;
    const QString key2 = c2.name + QLatin1String("::") + c2.qualifiedName;
    return key1 < key2;
  });
  return sorted;
}

} // Anonymous

class CppTypeHierarchyTreeView : public NavigationTreeView {
  Q_OBJECT public:
  CppTypeHierarchyTreeView(QWidget *parent);

  auto contextMenuEvent(QContextMenuEvent *event) -> void override;
};

CppTypeHierarchyTreeView::CppTypeHierarchyTreeView(QWidget *parent) : NavigationTreeView(parent) {}

auto CppTypeHierarchyTreeView::contextMenuEvent(QContextMenuEvent *event) -> void
{
  if (!event)
    return;

  QMenu contextMenu;

  auto action = contextMenu.addAction(tr("Open in Editor"));
  connect(action, &QAction::triggered, this, [this]() {
    emit activated(currentIndex());
  });
  action = contextMenu.addAction(tr("Open Type Hierarchy"));
  connect(action, &QAction::triggered, this, [this]() {
    emit doubleClicked(currentIndex());
  });

  contextMenu.addSeparator();

  action = contextMenu.addAction(tr("Expand All"));
  connect(action, &QAction::triggered, this, &QTreeView::expandAll);
  action = contextMenu.addAction(tr("Collapse All"));
  connect(action, &QAction::triggered, this, &QTreeView::collapseAll);

  contextMenu.exec(event->globalPos());

  event->accept();
}

namespace CppEditor {namespace Internal {

  // CppTypeHierarchyWidget
  CppTypeHierarchyWidget::CppTypeHierarchyWidget()
  {
    m_inspectedClass = new TextEditor::TextEditorLinkLabel(this);
    m_inspectedClass->setContentsMargins(5, 5, 5, 5);
    m_model = new CppTypeHierarchyModel(this);
    m_treeView = new CppTypeHierarchyTreeView(this);
    m_treeView->setActivationMode(SingleClickActivation);
    m_delegate = new AnnotatedItemDelegate(this);
    m_delegate->setDelimiter(QLatin1String(" "));
    m_delegate->setAnnotationRole(AnnotationRole);
    m_treeView->setModel(m_model);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setItemDelegate(m_delegate);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setDragEnabled(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);
    m_treeView->setDefaultDropAction(Qt::MoveAction);
    connect(m_treeView, &QTreeView::activated, this, &CppTypeHierarchyWidget::onItemActivated);
    connect(m_treeView, &QTreeView::doubleClicked, this, &CppTypeHierarchyWidget::onItemDoubleClicked);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setAutoFillBackground(true);
    m_infoLabel->setBackgroundRole(QPalette::Base);

    m_hierarchyWidget = new QWidget(this);
    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_inspectedClass);
    layout->addWidget(Orca::Plugin::Core::ItemViewFind::createSearchableWrapper(m_treeView));
    m_hierarchyWidget->setLayout(layout);

    m_stackLayout = new QStackedLayout;
    m_stackLayout->addWidget(m_hierarchyWidget);
    m_stackLayout->addWidget(m_infoLabel);
    showNoTypeHierarchyLabel();
    setLayout(m_stackLayout);

    connect(CppEditorPlugin::instance(), &CppEditorPlugin::typeHierarchyRequested, this, &CppTypeHierarchyWidget::perform);
    connect(&m_futureWatcher, &QFutureWatcher<void>::finished, this, &CppTypeHierarchyWidget::displayHierarchy);

    m_synchronizer.setCancelOnWait(true);
  }

  auto CppTypeHierarchyWidget::perform() -> void
  {
    if (m_future.isRunning())
      m_future.cancel();

    m_showOldClass = false;

    auto editor = qobject_cast<TextEditor::BaseTextEditor*>(Orca::Plugin::Core::EditorManager::currentEditor());
    if (!editor) {
      showNoTypeHierarchyLabel();
      return;
    }

    auto widget = qobject_cast<CppEditorWidget*>(editor->widget());
    if (!widget) {
      showNoTypeHierarchyLabel();
      return;
    }

    showProgress();

    m_future = CppElementEvaluator::asyncExecute(widget);
    m_futureWatcher.setFuture(QFuture<void>(m_future));
    m_synchronizer.addFuture(m_future);

    Orca::Plugin::Core::ProgressManager::addTask(m_future, tr("Evaluating Type Hierarchy"), "TypeHierarchy");
  }

  auto CppTypeHierarchyWidget::performFromExpression(const QString &expression, const QString &fileName) -> void
  {
    if (m_future.isRunning())
      m_future.cancel();

    m_showOldClass = true;

    showProgress();

    m_future = CppElementEvaluator::asyncExecute(expression, fileName);
    m_futureWatcher.setFuture(QFuture<void>(m_future));
    m_synchronizer.addFuture(m_future);

    Orca::Plugin::Core::ProgressManager::addTask(m_future, tr("Evaluating Type Hierarchy"), "TypeHierarchy");
  }

  auto CppTypeHierarchyWidget::displayHierarchy() -> void
  {
    m_synchronizer.flushFinishedFutures();
    hideProgress();
    clearTypeHierarchy();

    if (!m_future.resultCount() || m_future.isCanceled()) {
      showNoTypeHierarchyLabel();
      return;
    }
    const auto &cppElement = m_future.result();
    if (cppElement.isNull()) {
      showNoTypeHierarchyLabel();
      return;
    }
    auto cppClass = cppElement->toCppClass();
    if (!cppClass) {
      showNoTypeHierarchyLabel();
      return;
    }

    m_inspectedClass->setText(cppClass->name);
    m_inspectedClass->setLink(cppClass->link);
    auto bases = new QStandardItem(tr("Bases"));
    m_model->invisibleRootItem()->appendRow(bases);
    auto selectedItem1 = buildHierarchy(*cppClass, bases, true, &CppClass::bases);
    auto derived = new QStandardItem(tr("Derived"));
    m_model->invisibleRootItem()->appendRow(derived);
    auto selectedItem2 = buildHierarchy(*cppClass, derived, true, &CppClass::derived);
    m_treeView->expandAll();
    m_oldClass = cppClass->qualifiedName;

    auto selectedItem = selectedItem1 ? selectedItem1 : selectedItem2;
    if (selectedItem)
      m_treeView->setCurrentIndex(m_model->indexFromItem(selectedItem));

    showTypeHierarchy();
  }

  auto CppTypeHierarchyWidget::buildHierarchy(const CppClass &cppClass, QStandardItem *parent, bool isRoot, const HierarchyMember member) -> QStandardItem*
  {
    QStandardItem *selectedItem = nullptr;
    if (!isRoot) {
      auto item = itemForClass(cppClass);
      parent->appendRow(item);
      parent = item;
      if (m_showOldClass && cppClass.qualifiedName == m_oldClass)
        selectedItem = item;
    }
    foreach(const CppClass &klass, sortClasses(cppClass.*member)) {
      auto item = buildHierarchy(klass, parent, false, member);
      if (!selectedItem)
        selectedItem = item;
    }
    return selectedItem;
  }

  auto CppTypeHierarchyWidget::showNoTypeHierarchyLabel() -> void
  {
    m_infoLabel->setText(tr("No type hierarchy available"));
    m_stackLayout->setCurrentWidget(m_infoLabel);
  }

  auto CppTypeHierarchyWidget::showTypeHierarchy() -> void
  {
    m_stackLayout->setCurrentWidget(m_hierarchyWidget);
  }

  auto CppTypeHierarchyWidget::showProgress() -> void
  {
    m_infoLabel->setText(tr("Evaluating type hierarchy..."));
    if (!m_progressIndicator) {
      m_progressIndicator = new ProgressIndicator(ProgressIndicatorSize::Large);
      m_progressIndicator->attachToWidget(this);
    }
    m_progressIndicator->show();
    m_progressIndicator->raise();
  }

  auto CppTypeHierarchyWidget::hideProgress() -> void
  {
    if (m_progressIndicator)
      m_progressIndicator->hide();
  }

  auto CppTypeHierarchyWidget::clearTypeHierarchy() -> void
  {
    m_inspectedClass->clear();
    m_model->clear();
  }

  static auto getExpression(const QModelIndex &index) -> QString
  {
    const auto annotation = index.data(AnnotationRole).toString();
    if (!annotation.isEmpty())
      return annotation;
    return index.data(Qt::DisplayRole).toString();
  }

  auto CppTypeHierarchyWidget::onItemActivated(const QModelIndex &index) -> void
  {
    auto link = index.data(LinkRole).value<Link>();
    if (!link.hasValidTarget())
      return;

    const auto updatedLink = CppElementEvaluator::linkFromExpression(getExpression(index), link.targetFilePath.toString());
    if (updatedLink.hasValidTarget())
      link = updatedLink;

    Orca::Plugin::Core::EditorManager::openEditorAt(link, Constants::CPPEDITOR_ID);
  }

  auto CppTypeHierarchyWidget::onItemDoubleClicked(const QModelIndex &index) -> void
  {
    const auto link = index.data(LinkRole).value<Link>();
    if (link.hasValidTarget())
      performFromExpression(getExpression(index), link.targetFilePath.toString());
  }

  // CppTypeHierarchyFactory
  CppTypeHierarchyFactory::CppTypeHierarchyFactory()
  {
    setDisplayName(tr("Type Hierarchy"));
    setPriority(700);
    setId(Constants::TYPE_HIERARCHY_ID);
  }

  auto CppTypeHierarchyFactory::createWidget() -> Orca::Plugin::Core::NavigationView
  {
    auto w = new CppTypeHierarchyWidget;
    w->perform();
    return {w, {}};
  }

  CppTypeHierarchyModel::CppTypeHierarchyModel(QObject *parent) : QStandardItemModel(parent) {}

  auto CppTypeHierarchyModel::supportedDragActions() const -> Qt::DropActions
  {
    // copy & move actions to avoid idiotic behavior of drag and drop:
    // standard item model removes nodes automatically that are
    // dropped anywhere with move action, but we do not want the '+' sign in the
    // drag handle that would appear when only allowing copy action
    return Qt::CopyAction | Qt::MoveAction;
  }

  auto CppTypeHierarchyModel::mimeTypes() const -> QStringList
  {
    return DropSupport::mimeTypesForFilePaths();
  }

  auto CppTypeHierarchyModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
  {
    auto data = new DropMimeData;
    data->setOverrideFileDropAction(Qt::CopyAction); // do not remove the item from the model
    foreach(const QModelIndex &index, indexes) {
      auto link = index.data(LinkRole).value<Link>();
      if (link.hasValidTarget())
        data->addFile(link.targetFilePath, link.targetLine, link.targetColumn);
    }
    return data;
  }

} // namespace Internal
} // namespace CppEditor

#include "cpptypehierarchy.moc"
