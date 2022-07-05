// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include "indexedcontainerproxyconstiterator.h"

#include <QAbstractItemModel>

#include <functional>

namespace Utils {

class BaseTreeModel;

class ORCA_UTILS_EXPORT TreeItem {
public:
  TreeItem();
  virtual ~TreeItem();
  
  using const_iterator = QVector<TreeItem*>::const_iterator;
  using value_type = TreeItem*;

  virtual auto data(int column, int role) const -> QVariant;
  virtual auto setData(int column, const QVariant &data, int role) -> bool;
  virtual auto flags(int column) const -> Qt::ItemFlags;
  virtual auto hasChildren() const -> bool;
  virtual auto canFetchMore() const -> bool;
  virtual auto fetchMore() -> void {}

  auto parent() const -> TreeItem* { return m_parent; }
  auto prependChild(TreeItem *item) -> void;
  auto appendChild(TreeItem *item) -> void;
  auto insertChild(int pos, TreeItem *item) -> void;
  auto insertOrderedChild(TreeItem *item, const std::function<bool(const TreeItem *, const TreeItem *)> &cmp) -> void;
  auto removeChildAt(int pos) -> void;
  auto removeChildren() -> void;
  auto sortChildren(const std::function<bool(const TreeItem *, const TreeItem *)> &cmp) -> void;
  auto update() -> void;
  auto updateAll() -> void;
  auto updateColumn(int column) -> void;
  auto expand() -> void;
  auto collapse() -> void;
  auto firstChild() const -> TreeItem*;
  auto lastChild() const -> TreeItem*;
  auto level() const -> int;
  auto childCount() const -> int { return m_children.size(); }
  auto indexInParent() const -> int;
  auto childAt(int index) const -> TreeItem*;
  auto indexOf(const TreeItem *item) const -> int;
  auto begin() const -> const_iterator { return m_children.begin(); }
  auto end() const -> const_iterator { return m_children.end(); }
  auto index() const -> QModelIndex;
  auto model() const -> QAbstractItemModel*;
  auto forSelectedChildren(const std::function<bool(TreeItem *)> &pred) const -> void;
  auto forAllChildren(const std::function<void(TreeItem *)> &pred) const -> void;
  auto findAnyChild(const std::function<bool(TreeItem *)> &pred) const -> TreeItem*;
  // like findAnyChild() but processes children in exact reverse order
  // (bottom to top, most inner children first)
  auto reverseFindAnyChild(const std::function<bool(TreeItem *)> &pred) const -> TreeItem*;
  // Levels are 1-based: Child at Level 1 is an immediate child.
  auto forChildrenAtLevel(int level, const std::function<void(TreeItem *)> &pred) const -> void;
  auto findChildAtLevel(int level, const std::function<bool(TreeItem *)> &pred) const -> TreeItem*;

private:
  TreeItem(const TreeItem &) = delete;
  auto operator=(const TreeItem &) -> void = delete;

  auto clear() -> void;
  auto removeItemAt(int pos) -> void;
  auto propagateModel(BaseTreeModel *m) -> void;

  TreeItem *m_parent = nullptr;     // Not owned.
  BaseTreeModel *m_model = nullptr; // Not owned.
  QVector<TreeItem*> m_children;    // Owned.
  friend class BaseTreeModel;
};

// A TreeItem with children all of the same type.
template <class ChildType, class ParentType = TreeItem>
class TypedTreeItem : public TreeItem {
public:
  auto childAt(int index) const -> ChildType* { return static_cast<ChildType*>(TreeItem::childAt(index)); }

  auto sortChildren(const std::function<bool(const ChildType *, const ChildType *)> &lessThan) -> void
  {
    return TreeItem::sortChildren([lessThan](const TreeItem *a, const TreeItem *b) {
      return lessThan(static_cast<const ChildType*>(a), static_cast<const ChildType*>(b));
    });
  }

  using value_type = ChildType*;
  using const_iterator = IndexedContainerProxyConstIterator<TypedTreeItem>;
  using size_type = int;

  auto operator[](int index) const -> ChildType* { return childAt(index); }
  auto size() const -> int { return childCount(); }
  auto begin() const -> const_iterator { return const_iterator(*this, 0); }
  auto end() const -> const_iterator { return const_iterator(*this, size()); }

  template <typename Predicate>
  auto forAllChildren(const Predicate &pred) const -> void
  {
    const auto pred0 = [pred](TreeItem *treeItem) { pred(static_cast<ChildType*>(treeItem)); };
    TreeItem::forAllChildren(pred0);
  }

  template <typename Predicate>
  auto forFirstLevelChildren(Predicate pred) const -> void
  {
    const auto pred0 = [pred](TreeItem *treeItem) { pred(static_cast<ChildType*>(treeItem)); };
    TreeItem::forChildrenAtLevel(1, pred0);
  }

  template <typename Predicate>
  auto findFirstLevelChild(Predicate pred) const -> ChildType*
  {
    const auto pred0 = [pred](TreeItem *treeItem) { return pred(static_cast<ChildType*>(treeItem)); };
    return static_cast<ChildType*>(TreeItem::findChildAtLevel(1, pred0));
  }

  auto parent() const -> ParentType*
  {
    return static_cast<ParentType*>(TreeItem::parent());
  }

  auto insertOrderedChild(ChildType *item, const std::function<bool(const ChildType *, const ChildType *)> &cmp) -> void
  {
    const auto cmp0 = [cmp](const TreeItem *lhs, const TreeItem *rhs) {
      return cmp(static_cast<const ChildType*>(lhs), static_cast<const ChildType*>(rhs));
    };
    TreeItem::insertOrderedChild(item, cmp0);
  }

  auto findAnyChild(const std::function<bool(TreeItem *)> &pred) const -> ChildType*
  {
    return static_cast<ChildType*>(TreeItem::findAnyChild(pred));
  }

  auto reverseFindAnyChild(const std::function<bool(TreeItem *)> &pred) const -> ChildType*
  {
    return static_cast<ChildType*>(TreeItem::reverseFindAnyChild(pred));
  }
};

class ORCA_UTILS_EXPORT StaticTreeItem : public TreeItem {
public:
  StaticTreeItem(const QStringList &displays);
  StaticTreeItem(const QString &display);
  StaticTreeItem(const QStringList &displays, const QStringList &toolTips);

  auto data(int column, int role) const -> QVariant override;
  auto flags(int column) const -> Qt::ItemFlags override;

private:
  QStringList m_displays;
  QStringList m_toolTips;
};

// A general purpose multi-level model where each item can have its
// own (TreeItem-derived) type.
class ORCA_UTILS_EXPORT BaseTreeModel : public QAbstractItemModel {
  Q_OBJECT

protected:
  explicit BaseTreeModel(QObject *parent = nullptr);
  explicit BaseTreeModel(TreeItem *root, QObject *parent = nullptr);
  ~BaseTreeModel() override;

  auto setHeader(const QStringList &displays) -> void;
  auto setHeaderToolTip(const QStringList &tips) -> void;
  auto clear() -> void;
  auto rootItem() const -> TreeItem*;
  auto setRootItem(TreeItem *item) -> void;
  auto itemForIndex(const QModelIndex &) const -> TreeItem*;
  auto indexForItem(const TreeItem *needle) const -> QModelIndex;
  auto rowCount(const QModelIndex &idx = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &idx) const -> int override;
  auto setData(const QModelIndex &idx, const QVariant &data, int role) -> bool override;
  auto data(const QModelIndex &idx, int role) const -> QVariant override;
  auto index(int, int, const QModelIndex &idx = QModelIndex()) const -> QModelIndex override;
  auto parent(const QModelIndex &idx) const -> QModelIndex override;
  auto sibling(int row, int column, const QModelIndex &idx) const -> QModelIndex override;
  auto flags(const QModelIndex &idx) const -> Qt::ItemFlags override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;
  auto hasChildren(const QModelIndex &idx) const -> bool override;
  auto canFetchMore(const QModelIndex &idx) const -> bool override;
  auto fetchMore(const QModelIndex &idx) -> void override;
  auto takeItem(TreeItem *item) -> TreeItem*; // item is not destroyed.
  auto destroyItem(TreeItem *item) -> void;   // item is destroyed.

signals:
  auto requestExpansion(const QModelIndex &) -> void;
  auto requestCollapse(const QModelIndex &) -> void;

protected:
  friend class TreeItem;

  TreeItem *m_root; // Owned.
  QStringList m_header;
  QStringList m_headerToolTip;
  int m_columnCount;
};

namespace Internal {

// SelectType<N, T0, T1, T2, ...> selects the Nth type from the list
// If there are not enough types in the list, 'TreeItem' is used.
template <int N, typename ...All>
struct SelectType;

template <int N, typename First, typename ...Rest>
struct SelectType<N, First, Rest...> {
  using Type = typename SelectType<N - 1, Rest...>::Type;
};

template <typename First, typename ...Rest>
struct SelectType<0, First, Rest...> {
  using Type = First;
};

template <int N>
struct SelectType<N> {
  using Type = TreeItem;
};

// BestItem<T0, T1, T2, ... > selects T0 if all types are equal and 'TreeItem' otherwise
template <typename ...All>
struct BestItemType;

template <typename First, typename Second, typename ...Rest>
struct BestItemType<First, Second, Rest...> {
  using Type = TreeItem;
};

template <typename First, typename ...Rest>
struct BestItemType<First, First, Rest...> {
  using Type = typename BestItemType<First, Rest...>::Type;
};

template <typename First>
struct BestItemType<First> {
  using Type = First;
};

template <>
struct BestItemType<> {
  using Type = TreeItem;

};

} // namespace Internal

// A multi-level model with possibly uniform types per level.
template <typename ...LevelItemTypes>
class TreeModel : public BaseTreeModel {
public:
  using RootItem = typename Internal::SelectType<0, LevelItemTypes...>::Type;
  using BestItem = typename Internal::BestItemType<LevelItemTypes...>::Type;

  explicit TreeModel(QObject *parent = nullptr) : BaseTreeModel(new RootItem, parent) {}
  explicit TreeModel(RootItem *root, QObject *parent = nullptr) : BaseTreeModel(root, parent) {}

  using BaseTreeModel::canFetchMore;
  using BaseTreeModel::clear;
  using BaseTreeModel::columnCount;
  using BaseTreeModel::data;
  using BaseTreeModel::destroyItem;
  using BaseTreeModel::fetchMore;
  using BaseTreeModel::hasChildren;
  using BaseTreeModel::index;
  using BaseTreeModel::indexForItem;
  using BaseTreeModel::parent;
  using BaseTreeModel::rowCount;
  using BaseTreeModel::setData;
  using BaseTreeModel::setHeader;
  using BaseTreeModel::setHeaderToolTip;
  using BaseTreeModel::takeItem;

  template <int Level, class Predicate>
  auto forItemsAtLevel(const Predicate &pred) const -> void
  {
    using ItemType = typename Internal::SelectType<Level, LevelItemTypes...>::Type;
    const auto pred0 = [pred](TreeItem *treeItem) { pred(static_cast<ItemType*>(treeItem)); };
    m_root->forChildrenAtLevel(Level, pred0);
  }

  template <int Level, class Predicate>
  auto findItemAtLevel(const Predicate &pred) const -> typename Internal::SelectType<Level, LevelItemTypes...>::Type*
  {
    using ItemType = typename Internal::SelectType<Level, LevelItemTypes...>::Type;
    const auto pred0 = [pred](TreeItem *treeItem) { return pred(static_cast<ItemType*>(treeItem)); };
    return static_cast<ItemType*>(m_root->findChildAtLevel(Level, pred0));
  }

  auto rootItem() const -> RootItem*
  {
    return static_cast<RootItem*>(BaseTreeModel::rootItem());
  }

  template <int Level>
  auto itemForIndexAtLevel(const QModelIndex &idx) const -> typename Internal::SelectType<Level, LevelItemTypes...>::Type*
  {
    TreeItem *item = BaseTreeModel::itemForIndex(idx);
    return item && item->level() == Level ? static_cast<typename Internal::SelectType<Level, LevelItemTypes...>::Type*>(item) : nullptr;
  }

  auto nonRootItemForIndex(const QModelIndex &idx) const -> BestItem*
  {
    TreeItem *item = BaseTreeModel::itemForIndex(idx);
    return item && item->parent() ? static_cast<BestItem*>(item) : nullptr;
  }

  template <class Predicate>
  auto findNonRootItem(const Predicate &pred) const -> BestItem*
  {
    const auto pred0 = [pred](TreeItem *treeItem) -> bool { return pred(static_cast<BestItem*>(treeItem)); };
    return static_cast<BestItem*>(m_root->findAnyChild(pred0));
  }

  template <class Predicate>
  auto forSelectedItems(const Predicate &pred) const -> void
  {
    const auto pred0 = [pred](TreeItem *treeItem) -> bool { return pred(static_cast<BestItem*>(treeItem)); };
    m_root->forSelectedChildren(pred0);
  }

  template <class Predicate>
  auto forAllItems(const Predicate &pred) const -> void
  {
    const auto pred0 = [pred](TreeItem *treeItem) -> void { pred(static_cast<BestItem*>(treeItem)); };
    m_root->forAllChildren(pred0);
  }

  auto itemForIndex(const QModelIndex &idx) const -> BestItem*
  {
    return static_cast<BestItem*>(BaseTreeModel::itemForIndex(idx));
  }
};

} // namespace Utils

Q_DECLARE_METATYPE(Utils::TreeItem *)
