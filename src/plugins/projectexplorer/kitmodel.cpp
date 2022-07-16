// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitmodel.hpp"

#include "kit.hpp"
#include "kitmanagerconfigwidget.hpp"
#include "kitmanager.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>
#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QApplication>
#include <QLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class KitNode : public TreeItem {
public:
  KitNode(Kit *k, KitModel *m)
  {
    widget = new KitManagerConfigWidget(k);

    QObject::connect(widget, &KitManagerConfigWidget::dirty, m, [this] { update(); });

    QObject::connect(widget, &KitManagerConfigWidget::isAutoDetectedChanged, m, [this, m] {
      const auto oldParent = parent();
      const auto newParent = m->rootItem()->childAt(widget->workingCopy()->isAutoDetected() ? 0 : 1);
      if (oldParent && oldParent != newParent) {
        m->takeItem(this);
        newParent->appendChild(this);
      }
    });
  }

  ~KitNode() override
  {
    delete widget;
  }

  auto data(int, int role) const -> QVariant override
  {
    if (widget) {
      if (role == Qt::FontRole) {
        auto f = QApplication::font();
        if (widget->isDirty())
          f.setBold(!f.bold());
        if (widget->isDefaultKit())
          f.setItalic(f.style() != QFont::StyleItalic);
        return f;
      }
      if (role == Qt::DisplayRole) {
        auto baseName = widget->displayName();
        if (widget->isDefaultKit())
          //: Mark up a kit as the default one.
          baseName = KitModel::tr("%1 (default)").arg(baseName);
        return baseName;
      }
      if (role == Qt::DecorationRole) {
        return widget->displayIcon();
      }
      if (role == Qt::ToolTipRole) {
        return widget->validityMessage();
      }
    }
    return QVariant();
  }

  KitManagerConfigWidget *widget;
};

// --------------------------------------------------------------------------
// KitModel
// --------------------------------------------------------------------------

KitModel::KitModel(QBoxLayout *parentLayout, QObject *parent) : TreeModel<TreeItem, TreeItem, KitNode>(parent), m_parentLayout(parentLayout)
{
  setHeader(QStringList(tr("Name")));
  m_autoRoot = new StaticTreeItem({Constants::msgAutoDetected()}, {Constants::msgAutoDetectedToolTip()});
  m_manualRoot = new StaticTreeItem(Constants::msgManual());
  rootItem()->appendChild(m_autoRoot);
  rootItem()->appendChild(m_manualRoot);

  foreach(Kit *k, KitManager::sortKits(KitManager::kits()))
    addKit(k);

  changeDefaultKit();

  connect(KitManager::instance(), &KitManager::kitAdded, this, &KitModel::addKit);
  connect(KitManager::instance(), &KitManager::kitUpdated, this, &KitModel::updateKit);
  connect(KitManager::instance(), &KitManager::unmanagedKitUpdated, this, &KitModel::updateKit);
  connect(KitManager::instance(), &KitManager::kitRemoved, this, &KitModel::removeKit);
  connect(KitManager::instance(), &KitManager::defaultkitChanged, this, &KitModel::changeDefaultKit);
}

auto KitModel::kit(const QModelIndex &index) -> Kit*
{
  const auto n = kitNode(index);
  return n ? n->widget->workingCopy() : nullptr;
}

auto KitModel::kitNode(const QModelIndex &index) -> KitNode*
{
  const auto n = itemForIndex(index);
  return (n && n->level() == 2) ? static_cast<KitNode*>(n) : nullptr;
}

auto KitModel::indexOf(Kit *k) const -> QModelIndex
{
  const auto n = findWorkingCopy(k);
  return n ? indexForItem(n) : QModelIndex();
}

auto KitModel::setDefaultKit(const QModelIndex &index) -> void
{
  if (const auto n = kitNode(index))
    setDefaultNode(n);
}

auto KitModel::isDefaultKit(Kit *k) const -> bool
{
  return m_defaultNode && m_defaultNode->widget->workingCopy() == k;
}

auto KitModel::widget(const QModelIndex &index) -> KitManagerConfigWidget*
{
  const auto n = kitNode(index);
  return n ? n->widget : nullptr;
}

auto KitModel::validateKitNames() -> void
{
  QHash<QString, int> nameHash;
  forItemsAtLevel<2>([&nameHash](KitNode *n) {
    const auto displayName = n->widget->displayName();
    if (nameHash.contains(displayName))
      ++nameHash[displayName];
    else
      nameHash.insert(displayName, 1);
  });

  forItemsAtLevel<2>([&nameHash](KitNode *n) {
    const auto displayName = n->widget->displayName();
    n->widget->setHasUniqueName(nameHash.value(displayName) == 1);
  });
}

auto KitModel::apply() -> void
{
  // Add/update dirty nodes before removing kits. This ensures the right kit ends up as default.
  forItemsAtLevel<2>([](KitNode *n) {
    if (n->widget->isDirty()) {
      n->widget->apply();
      n->update();
    }
  });

  // Remove unused kits:
  foreach(KitNode *n, m_toRemoveList)
    n->widget->removeKit();

  emit layoutChanged(); // Force update.
}

auto KitModel::markForRemoval(Kit *k) -> void
{
  auto node = findWorkingCopy(k);
  if (!node)
    return;

  if (node == m_defaultNode) {
    auto newDefault = m_autoRoot->firstChild();
    if (!newDefault)
      newDefault = m_manualRoot->firstChild();
    setDefaultNode(static_cast<KitNode*>(newDefault));
  }

  if (node == m_defaultNode)
    setDefaultNode(findItemAtLevel<2>([node](KitNode *kn) { return kn != node; }));

  takeItem(node);
  if (node->widget->configures(nullptr))
    delete node;
  else
    m_toRemoveList.append(node);
  validateKitNames();
}

auto KitModel::markForAddition(Kit *baseKit) -> Kit*
{
  const auto newName = newKitName(baseKit ? baseKit->unexpandedDisplayName() : QString());
  const auto node = createNode(nullptr);
  m_manualRoot->appendChild(node);
  const auto k = node->widget->workingCopy();
  KitGuard g(k);
  if (baseKit) {
    k->copyFrom(baseKit);
    k->setAutoDetected(false); // Make sure we have a manual kit!
    k->setSdkProvided(false);
  } else {
    k->setup();
  }
  k->setUnexpandedDisplayName(newName);

  if (!m_defaultNode)
    setDefaultNode(node);

  return k;
}

auto KitModel::updateVisibility() -> void
{
  forItemsAtLevel<2>([](const TreeItem *ti) {
    static_cast<const KitNode*>(ti)->widget->updateVisibility();
  });
}

auto KitModel::newKitName(const QString &sourceName) const -> QString
{
  QList<Kit*> allKits;
  forItemsAtLevel<2>([&allKits](const TreeItem *ti) {
    allKits << static_cast<const KitNode*>(ti)->widget->workingCopy();
  });
  return Kit::newKitName(sourceName, allKits);
}

auto KitModel::findWorkingCopy(Kit *k) const -> KitNode*
{
  return findItemAtLevel<2>([k](KitNode *n) { return n->widget->workingCopy() == k; });
}

auto KitModel::createNode(Kit *k) -> KitNode*
{
  const auto node = new KitNode(k, this);
  m_parentLayout->addWidget(node->widget);
  return node;
}

auto KitModel::setDefaultNode(KitNode *node) -> void
{
  if (m_defaultNode) {
    m_defaultNode->widget->setIsDefaultKit(false);
    m_defaultNode->update();
  }
  m_defaultNode = node;
  if (m_defaultNode) {
    m_defaultNode->widget->setIsDefaultKit(true);
    m_defaultNode->update();
  }
}

auto KitModel::addKit(Kit *k) -> void
{
  for (const auto n : *m_manualRoot) {
    // Was added by us
    if (static_cast<KitNode*>(n)->widget->isRegistering())
      return;
  }

  const auto parent = k->isAutoDetected() ? m_autoRoot : m_manualRoot;
  parent->appendChild(createNode(k));

  validateKitNames();
  emit kitStateChanged();
}

auto KitModel::updateKit(Kit *) -> void
{
  validateKitNames();
  emit kitStateChanged();
}

auto KitModel::removeKit(Kit *k) -> void
{
  auto nodes = m_toRemoveList;
  foreach(KitNode *n, nodes) {
    if (n->widget->configures(k)) {
      m_toRemoveList.removeOne(n);
      if (m_defaultNode == n)
        m_defaultNode = nullptr;
      delete n;
      validateKitNames();
      return;
    }
  }

  auto node = findItemAtLevel<2>([k](KitNode *n) {
    return n->widget->configures(k);
  });

  if (node == m_defaultNode)
    setDefaultNode(findItemAtLevel<2>([node](KitNode *kn) { return kn != node; }));

  destroyItem(node);

  validateKitNames();
  emit kitStateChanged();
}

auto KitModel::changeDefaultKit() -> void
{
  auto defaultKit = KitManager::defaultKit();
  const auto node = findItemAtLevel<2>([defaultKit](KitNode *n) {
    return n->widget->configures(defaultKit);
  });
  setDefaultNode(node);
}

} // namespace Internal
} // namespace ProjectExplorer
