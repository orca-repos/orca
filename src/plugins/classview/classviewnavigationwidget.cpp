// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewnavigationwidget.hpp"
#include "classviewmanager.hpp"
#include "classviewsymbollocation.hpp"
#include "classviewsymbolinformation.hpp"
#include "classviewutils.hpp"
#include "classviewconstants.hpp"

#include <core/core-item-view-find.hpp>

#include <cplusplus/Icons.h>

#include <utils/navigationtreeview.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QVariant>
#include <QVBoxLayout>
#include <QElapsedTimer>

enum {
  debug = false
};

namespace ClassView {
namespace Internal {

///////////////////////////////// NavigationWidget //////////////////////////////////

/*!
    \class NavigationWidget

    The NavigationWidget class is a widget for the class view tree.
*/


/*!
    \fn void NavigationWidget::visibilityChanged(bool visibility)

    Emits a signal when the widget visibility is changed. \a visibility returns
    true if plugin becames visible, otherwise it returns false.
*/

/*!
    \fn void NavigationWidget::requestGotoLocations(const QList<QVariant> &locations)

    Emits a signal to request to go to any of the Symbol \a locations in the
    list.

   \sa Manager::gotoLocations
*/

NavigationWidget::NavigationWidget(QWidget *parent) : QWidget(parent)
{
  const auto verticalLayout = new QVBoxLayout(this);
  verticalLayout->setSpacing(0);
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  treeView = new ::Utils::NavigationTreeView(this);
  treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  treeView->setDragEnabled(true);
  treeView->setDragDropMode(QAbstractItemView::DragOnly);
  treeView->setDefaultDropAction(Qt::MoveAction);
  treeView->setExpandsOnDoubleClick(false);
  verticalLayout->addWidget(Orca::Plugin::Core::ItemViewFind::createSearchableWrapper(treeView, Orca::Plugin::Core::ItemViewFind::DarkColored, Orca::Plugin::Core::ItemViewFind::FetchMoreWhileSearching));
  setFocusProxy(treeView);
  // tree model
  treeModel = new TreeItemModel(this);
  treeView->setModel(treeModel);

  // connect signal/slots
  // selected item
  connect(treeView, &QAbstractItemView::activated, this, &NavigationWidget::onItemActivated);

  // double-clicked item
  connect(treeView, &QAbstractItemView::doubleClicked, this, &NavigationWidget::onItemDoubleClicked);

  // connections to the manager
  const auto manager = Manager::instance();

  connect(this, &NavigationWidget::visibilityChanged, manager, &Manager::onWidgetVisibilityIsChanged);

  connect(this, &NavigationWidget::requestGotoLocations, manager, &Manager::gotoLocations);

  connect(manager, &Manager::treeDataUpdate, this, &NavigationWidget::onDataUpdate);
}

NavigationWidget::~NavigationWidget() = default;

auto NavigationWidget::hideEvent(QHideEvent *event) -> void
{
  emit visibilityChanged(false);
  QWidget::hideEvent(event);
}

auto NavigationWidget::showEvent(QShowEvent *event) -> void
{
  emit visibilityChanged(true);

  QWidget::showEvent(event);
}

/*!
    Creates QToolbuttons for the Navigation Pane widget.

    Returns the list of created QToolButtons.

   \sa NavigationWidgetFactory::createWidget
*/

auto NavigationWidget::createToolButtons() -> QList<QToolButton*>
{
  QList<QToolButton*> list;

  // full projects mode
  if (!fullProjectsModeButton) {
    // create a button
    fullProjectsModeButton = new QToolButton(this);
    fullProjectsModeButton->setIcon(::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Class));
    fullProjectsModeButton->setCheckable(true);
    fullProjectsModeButton->setToolTip(tr("Show Subprojects"));

    // by default - not a flat mode
    setFlatMode(false);

    // connections
    connect(fullProjectsModeButton.data(), &QAbstractButton::toggled, this, &NavigationWidget::onFullProjectsModeToggled);
  }

  list << fullProjectsModeButton;

  return list;
}

/*!
    Returns flat mode state.
*/

auto NavigationWidget::flatMode() const -> bool
{
  QTC_ASSERT(fullProjectsModeButton, return false);

  // button is 'full projects mode' - so it has to be inverted
  return !fullProjectsModeButton->isChecked();
}

/*!
   Sets the flat mode state to \a flatMode.
*/

auto NavigationWidget::setFlatMode(bool flatMode) -> void
{
  QTC_ASSERT(fullProjectsModeButton, return);

  // button is 'full projects mode' - so it has to be inverted
  fullProjectsModeButton->setChecked(!flatMode);
}

/*!
    Full projects mode button has been toggled. \a state holds the full
    projects mode.
*/

auto NavigationWidget::onFullProjectsModeToggled(bool state) -> void
{
  // button is 'full projects mode' - so it has to be inverted
  Manager::instance()->setFlatMode(!state);
}

/*!
    Activates the item with the \a index in the tree view.
*/

auto NavigationWidget::onItemActivated(const QModelIndex &index) -> void
{
  if (!index.isValid())
    return;

  const auto list = treeModel->data(index, Constants::SymbolLocationsRole).toList();

  emit requestGotoLocations(list);
}

/*!
    Expands/collapses the item given by \a index if it
    refers to a project file (.pro/.pri)
*/

auto NavigationWidget::onItemDoubleClicked(const QModelIndex &index) -> void
{
  if (!index.isValid())
    return;

  const auto iconType = treeModel->data(index, Constants::IconTypeRole);
  if (!iconType.isValid())
    return;

  auto ok = false;
  const auto type = iconType.toInt(&ok);
  if (ok && type == INT_MIN)
    treeView->setExpanded(index, !treeView->isExpanded(index));
}

/*!
    Receives new data for the tree. \a result is a pointer to the Class View
    model root item. The function does nothing if null is passed.
*/

auto NavigationWidget::onDataUpdate(QSharedPointer<QStandardItem> result) -> void
{
  if (result.isNull())
    return;

  QElapsedTimer timer;
  if (debug)
    timer.start();
  // update is received. root item must be updated - and received information
  // might be just a root - if a lazy data population is enabled.
  // so expanded items must be parsed and 'fetched'

  fetchExpandedItems(result.data(), treeModel->invisibleRootItem());

  treeModel->moveRootToTarget(result.data());

  // expand top level projects
  const QModelIndex sessionIndex;
  const auto toplevelCount = treeModel->rowCount(sessionIndex);
  for (auto i = 0; i < toplevelCount; ++i)
    treeView->expand(treeModel->index(i, 0, sessionIndex));

  if (!treeView->currentIndex().isValid() && toplevelCount > 0)
    treeView->setCurrentIndex(treeModel->index(0, 0, sessionIndex));
  if (debug)
    qDebug() << "Class View:" << QDateTime::currentDateTime().toString() << "TreeView is updated in" << timer.elapsed() << "msecs";
}

/*!
    Fetches data for expanded items to make sure that the content will exist.
    \a item and \a target do nothing if null is passed.
*/

auto NavigationWidget::fetchExpandedItems(QStandardItem *item, const QStandardItem *target) const -> void
{
  if (!item || !target)
    return;

  const auto &parent = treeModel->indexFromItem(target);
  if (treeView->isExpanded(parent) && Manager::instance()->canFetchMore(item, true))
    Manager::instance()->fetchMore(item, true);

  auto itemIndex = 0;
  auto targetIndex = 0;
  const auto itemRows = item->rowCount();
  const auto targetRows = target->rowCount();

  while (itemIndex < itemRows && targetIndex < targetRows) {
    const auto itemChild = item->child(itemIndex);
    const QStandardItem *targetChild = target->child(targetIndex);

    const auto &itemInf = Internal::symbolInformationFromItem(itemChild);
    const auto &targetInf = Internal::symbolInformationFromItem(targetChild);

    if (itemInf < targetInf) {
      ++itemIndex;
    } else if (itemInf == targetInf) {
      fetchExpandedItems(itemChild, targetChild);
      ++itemIndex;
      ++targetIndex;
    } else {
      ++targetIndex;
    }
  }
}

} // namespace Internal
} // namespace ClassView
