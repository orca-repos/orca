// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewtreeitemmodel.hpp"
#include "classviewconstants.hpp"
#include "classviewmanager.hpp"
#include "classviewutils.hpp"

#include <cplusplus/Icons.h>
#include <utils/dropsupport.hpp>

namespace ClassView {
namespace Internal {

/*!
   Moves \a item to \a target (sorted).
*/

static auto moveItemToTarget(QStandardItem *item, const QStandardItem *target) -> void
{
  if (!item || !target)
    return;

  auto itemIndex = 0;
  auto targetIndex = 0;
  auto itemRows = item->rowCount();
  const auto targetRows = target->rowCount();

  while (itemIndex < itemRows && targetIndex < targetRows) {
    const auto itemChild = item->child(itemIndex);
    const QStandardItem *targetChild = target->child(targetIndex);

    const auto &itemInf = Internal::symbolInformationFromItem(itemChild);
    const auto &targetInf = Internal::symbolInformationFromItem(targetChild);

    if (itemInf < targetInf) {
      item->removeRow(itemIndex);
      --itemRows;
    } else if (itemInf == targetInf) {
      moveItemToTarget(itemChild, targetChild);
      ++itemIndex;
      ++targetIndex;
    } else {
      item->insertRow(itemIndex, targetChild->clone());
      moveItemToTarget(item->child(itemIndex), targetChild);
      ++itemIndex;
      ++itemRows;
      ++targetIndex;
    }
  }

  // append
  while (targetIndex < targetRows) {
    item->appendRow(target->child(targetIndex)->clone());
    moveItemToTarget(item->child(itemIndex), target->child(targetIndex));
    ++itemIndex;
    ++itemRows;
    ++targetIndex;
  }

  // remove end of item
  while (itemIndex < itemRows) {
    item->removeRow(itemIndex);
    --itemRows;
  }
}

///////////////////////////////// TreeItemModel //////////////////////////////////

/*!
   \class TreeItemModel
   \brief The TreeItemModel class provides a model for the Class View tree.
*/

TreeItemModel::TreeItemModel(QObject *parent) : QStandardItemModel(parent) {}

TreeItemModel::~TreeItemModel() = default;

auto TreeItemModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid())
    return QStandardItemModel::data(index, role);

  switch (role) {
  case Qt::DecorationRole: {
    const auto iconType = data(index, Constants::IconTypeRole);
    if (iconType.isValid()) {
      auto ok = false;
      auto type = iconType.toInt(&ok);
      if (ok && type >= 0)
        return ::Utils::CodeModelIcon::iconForType(static_cast<::Utils::CodeModelIcon::Type>(type));
    }
  }
  break;
  case Qt::ToolTipRole:
  case Qt::DisplayRole: {
    const auto &inf = Internal::symbolInformationFromItem(itemFromIndex(index));

    if (inf.name() == inf.type() || inf.iconType() < 0)
      return inf.name();

    auto name(inf.name());

    if (!inf.type().isEmpty())
      name += QLatin1Char(' ') + inf.type();

    return name;
  }
  break;
  default:
    break;
  }

  return QStandardItemModel::data(index, role);
}

auto TreeItemModel::canFetchMore(const QModelIndex &parent) const -> bool
{
  if (!parent.isValid())
    return false;

  return Manager::instance()->canFetchMore(itemFromIndex(parent));
}

auto TreeItemModel::fetchMore(const QModelIndex &parent) -> void
{
  if (!parent.isValid())
    return;

  return Manager::instance()->fetchMore(itemFromIndex(parent));
}

auto TreeItemModel::hasChildren(const QModelIndex &parent) const -> bool
{
  if (!parent.isValid())
    return true;

  return Manager::instance()->hasChildren(itemFromIndex(parent));
}

auto TreeItemModel::supportedDragActions() const -> Qt::DropActions
{
  return Qt::MoveAction | Qt::CopyAction;
}

auto TreeItemModel::mimeTypes() const -> QStringList
{
  return ::Utils::DropSupport::mimeTypesForFilePaths();
}

auto TreeItemModel::mimeData(const QModelIndexList &indexes) const -> QMimeData*
{
  const auto mimeData = new ::Utils::DropMimeData;
  mimeData->setOverrideFileDropAction(Qt::CopyAction);
  for (const auto &index : indexes) {
    const auto locations = Internal::roleToLocations(data(index, Constants::SymbolLocationsRole).toList());
    if (locations.isEmpty())
      continue;
    const auto loc = *locations.constBegin();
    mimeData->addFile(Utils::FilePath::fromString(loc.fileName()), loc.line(), loc.column());
  }
  if (mimeData->files().isEmpty()) {
    delete mimeData;
    return nullptr;
  }
  return mimeData;
}

/*!
   Moves the root item to the \a target item.
*/

auto TreeItemModel::moveRootToTarget(const QStandardItem *target) -> void
{
  emit layoutAboutToBeChanged();

  moveItemToTarget(invisibleRootItem(), target);

  emit layoutChanged();
}

} // namespace Internal
} // namespace ClassView
