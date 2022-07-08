// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/treemodel.hpp>

QT_BEGIN_NAMESPACE
class QBoxLayout;
QT_END_NAMESPACE

namespace ProjectExplorer {

class Kit;
class KitFactory;
class KitManager;

namespace Internal {

class KitManagerConfigWidget;
class KitNode;

// --------------------------------------------------------------------------
// KitModel:
// --------------------------------------------------------------------------

class KitModel : public Utils::TreeModel<Utils::TreeItem, Utils::TreeItem, KitNode> {
  Q_OBJECT

public:
  explicit KitModel(QBoxLayout *parentLayout, QObject *parent = nullptr);

  auto kit(const QModelIndex &) -> Kit*;
  auto kitNode(const QModelIndex &) -> KitNode*;
  auto indexOf(Kit *k) const -> QModelIndex;
  auto setDefaultKit(const QModelIndex &index) -> void;
  auto isDefaultKit(Kit *k) const -> bool;
  auto widget(const QModelIndex &) -> KitManagerConfigWidget*;
  auto apply() -> void;
  auto markForRemoval(Kit *k) -> void;
  auto markForAddition(Kit *baseKit) -> Kit*;
  auto updateVisibility() -> void;
  auto newKitName(const QString &sourceName) const -> QString;

signals:
  auto kitStateChanged() -> void;

private:
  auto addKit(Kit *k) -> void;
  auto updateKit(Kit *k) -> void;
  auto removeKit(Kit *k) -> void;
  auto changeDefaultKit() -> void;
  auto validateKitNames() -> void;
  auto findWorkingCopy(Kit *k) const -> KitNode*;
  auto createNode(Kit *k) -> KitNode*;
  auto setDefaultNode(KitNode *node) -> void;

  Utils::TreeItem *m_autoRoot;
  Utils::TreeItem *m_manualRoot;
  QList<KitNode*> m_toRemoveList;
  QBoxLayout *m_parentLayout;
  KitNode *m_defaultNode = nullptr;
  bool m_keepUnique = true;
};

} // namespace Internal
} // namespace ProjectExplorer
