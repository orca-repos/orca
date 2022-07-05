// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "treemodel.h"

namespace Utils {

template <class ChildType>
class BaseListModel : public TreeModel<TypedTreeItem<ChildType>, ChildType> {
public:
  using BaseModel = TreeModel<TypedTreeItem<ChildType>, ChildType>;
  using BaseModel::rootItem;

  explicit BaseListModel(QObject *parent = nullptr) : BaseModel(parent) {}

  auto itemCount() const -> int { return rootItem()->childCount(); }
  auto itemAt(int row) const -> ChildType* { return rootItem()->childAt(row); }
  auto appendItem(ChildType *item) -> void { rootItem()->appendChild(item); }

  template <class Predicate>
  auto forItems(const Predicate &pred) const -> void
  {
    rootItem()->forFirstLevelChildren(pred);
  }

  template <class Predicate>
  auto findItem(const Predicate &pred) const -> ChildType*
  {
    return rootItem()->findFirstLevelChild(pred);
  }

  auto sortItems(const std::function<bool(const ChildType *, const ChildType *)> &lessThan) -> void
  {
    return rootItem()->sortChildren([lessThan](const TreeItem *a, const TreeItem *b) {
      return lessThan(static_cast<const ChildType*>(a), static_cast<const ChildType*>(b));
    });
  }

  auto indexOf(const ChildType *item) const -> int { return rootItem()->indexOf(item); }
  auto clear() -> void { rootItem()->removeChildren(); }
  auto begin() const { return rootItem()->begin(); }
  auto end() const { return rootItem()->end(); }
};

template <class ItemData>
class ListItem : public TreeItem
{
public:
    ItemData itemData;
};

template <class ItemData>
class ListModel : public BaseListModel<ListItem<ItemData>> {
public:
  using ChildType = ListItem<ItemData>;
  using BaseModel = BaseListModel<ChildType>;

  explicit ListModel(QObject *parent = nullptr) : BaseModel(parent) {}

  auto dataAt(int row) const -> const ItemData&
  {
    static const ItemData dummyData = {};
    auto item = BaseModel::itemAt(row);
    return item ? item->itemData : dummyData;
  }

  auto findItemByData(const std::function<bool(const ItemData &)> &pred) const -> ChildType*
  {
    return BaseModel::rootItem()->findFirstLevelChild([pred](ChildType *child) {
      return pred(child->itemData);
    });
  }

  auto destroyItems(const std::function<bool(const ItemData &)> &pred) -> void
  {
    QList<ChildType*> toDestroy;
    BaseModel::rootItem()->forFirstLevelChildren([pred, &toDestroy](ChildType *item) {
      if (pred(item->itemData))
        toDestroy.append(item);
    });
    for (ChildType *item : toDestroy)
      this->destroyItem(item);
  }

  auto findData(const std::function<bool(const ItemData &)> &pred) const -> ItemData*
  {
    ChildType *item = findItemByData(pred);
    return item ? &item->itemData : nullptr;
  }

  auto findIndex(const std::function<bool(const ItemData &)> &pred) const -> QModelIndex
  {
    ChildType *item = findItemByData(pred);
    return item ? BaseTreeModel::indexForItem(item) : QModelIndex();
  }

  auto allData() const -> QList<ItemData>
  {
    QList<ItemData> res;
    BaseModel::rootItem()->forFirstLevelChildren([&res](ChildType *child) {
      res.append(child->itemData);
    });
    return res;
  }

  auto setAllData(const QList<ItemData> &items) -> void
  {
    BaseModel::rootItem()->removeChildren();
    for (const ItemData &data : items)
      appendItem(data);
  }

  auto forAllData(const std::function<void(ItemData &)> &func) const -> void
  {
    BaseModel::rootItem()->forFirstLevelChildren([func](ChildType *child) {
      func(child->itemData);
    });
  }

  auto appendItem(const ItemData &data) -> ChildType*
  {
    auto item = new ChildType;
    item->itemData = data;
    BaseModel::rootItem()->appendChild(item);
    return item;
  }

  auto data(const QModelIndex &idx, int role) const -> QVariant override
  {
    TreeItem *item = BaseModel::itemForIndex(idx);
    if (item && item->parent() == BaseModel::rootItem())
      return itemData(static_cast<ChildType*>(item)->itemData, idx.column(), role);
    return {};
  }

  auto flags(const QModelIndex &idx) const -> Qt::ItemFlags override
  {
    TreeItem *item = BaseModel::itemForIndex(idx);
    if (item && item->parent() == BaseModel::rootItem())
      return itemFlags(static_cast<ChildType*>(item)->itemData, idx.column());
    return {};
  }

  using QAbstractItemModel::itemData;

  virtual auto itemData(const ItemData &idata, int column, int role) const -> QVariant
  {
    if (m_dataAccessor)
      return m_dataAccessor(idata, column, role);
    return {};
  }

  virtual auto itemFlags(const ItemData &idata, int column) const -> Qt::ItemFlags
  {
    if (m_flagsAccessor)
      return m_flagsAccessor(idata, column);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

  auto setDataAccessor(const std::function<QVariant(const ItemData &, int, int)> &accessor) -> void
  {
    m_dataAccessor = accessor;
  }

  auto setFlagsAccessor(const std::function<Qt::ItemFlags(const ItemData &, int)> &accessor) -> void
  {
    m_flagsAccessor = accessor;
  }

private:
  std::function<QVariant(const ItemData &, int, int)> m_dataAccessor;
  std::function<Qt::ItemFlags(const ItemData &, int)> m_flagsAccessor;
};

} // namespace Utils
