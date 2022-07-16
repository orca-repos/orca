// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filterkitaspectsdialog.hpp"

#include "kitmanager.hpp"

#include <utils/itemviews.hpp>
#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QString>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class FilterTreeItem : public TreeItem {
public:
  FilterTreeItem(const KitAspect *aspect, bool enabled) : m_aspect(aspect), m_enabled(enabled) { }

  auto displayName() const -> QString { return m_aspect->displayName(); }
  auto id() const -> Id { return m_aspect->id(); }
  auto enabled() const -> bool { return m_enabled; }

private:
  auto data(int column, int role) const -> QVariant override
  {
    QTC_ASSERT(column < 2, return QVariant());
    if (column == 0 && role == Qt::DisplayRole)
      return displayName();
    if (column == 1 && role == Qt::CheckStateRole)
      return m_enabled ? Qt::Checked : Qt::Unchecked;
    return QVariant();
  }

  auto setData(int column, const QVariant &data, int role) -> bool override
  {
    QTC_ASSERT(column == 1 && !m_aspect->isEssential(), return false);
    if (role == Qt::CheckStateRole) {
      m_enabled = data.toInt() == Qt::Checked;
      return true;
    }
    return false;
  }

  auto flags(int column) const -> Qt::ItemFlags override
  {
    QTC_ASSERT(column < 2, return Qt::ItemFlags());
    Qt::ItemFlags flags = Qt::ItemIsSelectable;
    if (column == 0 || !m_aspect->isEssential())
      flags |= Qt::ItemIsEnabled;
    if (column == 1 && !m_aspect->isEssential())
      flags |= Qt::ItemIsUserCheckable;
    return flags;
  }

  const KitAspect *const m_aspect;
  bool m_enabled;
};

class FilterKitAspectsModel : public TreeModel<TreeItem, FilterTreeItem> {
public:
  FilterKitAspectsModel(const Kit *kit, QObject *parent) : TreeModel(parent)
  {
    setHeader({FilterKitAspectsDialog::tr("Setting"), FilterKitAspectsDialog::tr("Visible")});
    for (const KitAspect *const aspect : KitManager::kitAspects()) {
      if (kit && !aspect->isApplicableToKit(kit))
        continue;
      const auto irrelevantAspects = kit ? kit->irrelevantAspects() : KitManager::irrelevantAspects();
      auto *const item = new FilterTreeItem(aspect, !irrelevantAspects.contains(aspect->id()));
      rootItem()->appendChild(item);
    }
    static const auto cmp = [](const TreeItem *item1, const TreeItem *item2) {
      return static_cast<const FilterTreeItem*>(item1)->displayName() < static_cast<const FilterTreeItem*>(item2)->displayName();
    };
    rootItem()->sortChildren(cmp);
  }

  auto disabledItems() const -> QSet<Id>
  {
    QSet<Id> ids;
    for (auto i = 0; i < rootItem()->childCount(); ++i) {
      const FilterTreeItem *const item = static_cast<FilterTreeItem*>(rootItem()->childAt(i));
      if (!item->enabled())
        ids << item->id();
    }
    return ids;
  }
};

class FilterTreeView : public TreeView {
public:
  FilterTreeView(QWidget *parent) : TreeView(parent)
  {
    setUniformRowHeights(true);
  }

private:
  auto sizeHint() const -> QSize override
  {
    const auto width = columnWidth(0) + columnWidth(1);
    const auto height = model()->rowCount() * rowHeight(model()->index(0, 0)) + header()->sizeHint().height();
    return {width, height};
  }
};

FilterKitAspectsDialog::FilterKitAspectsDialog(const Kit *kit, QWidget *parent) : QDialog(parent), m_model(new FilterKitAspectsModel(kit, this))
{
  auto *const layout = new QVBoxLayout(this);
  auto *const view = new FilterTreeView(this);
  view->setModel(m_model);
  view->resizeColumnToContents(0);
  layout->addWidget(view);
  auto *const buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  layout->addWidget(buttonBox);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

auto FilterKitAspectsDialog::irrelevantAspects() const -> QSet<Id>
{
  return static_cast<FilterKitAspectsModel*>(m_model)->disabledItems();
}

} // namespace Internal
} // namespace ProjectExplorer
