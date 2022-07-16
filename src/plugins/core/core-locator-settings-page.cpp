// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-locator-settings-page.hpp"
#include "ui_core-locator-settings-page.h"

#include "core-constants.hpp"
#include "core-directory-filter.hpp"
#include "core-locator-constants.hpp"
#include "core-locator-filter-interface.hpp"
#include "core-locator.hpp"
#include "core-url-locator-filter.hpp"

#include <utils/algorithm.hpp>
#include <utils/categorysortfiltermodel.hpp>
#include <utils/headerviewstretcher.hpp>
#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>

#include <QCoreApplication>
#include <QHash>
#include <QMenu>

using namespace Utils;

static constexpr int sort_role = Qt::UserRole + 1;

namespace Orca::Plugin::Core {

enum filter_item_column {
  FilterName = 0,
  FilterPrefix,
  FilterIncludedByDefault
};

class FilterItem final : public TreeItem {
public:
  explicit FilterItem(ILocatorFilter *filter);

  auto data(int column, int role) const -> QVariant override;
  auto flags(int column) const -> Qt::ItemFlags override;
  auto setData(int column, const QVariant &data, int role) -> bool override;
  auto filter() const -> ILocatorFilter*;

private:
  ILocatorFilter *m_filter = nullptr;
};

class CategoryItem final : public TreeItem {
public:
  CategoryItem(QString name, int order);

  auto data(int column, int role) const -> QVariant override;

  auto flags(const int column) const -> Qt::ItemFlags override
  {
    Q_UNUSED(column)
    return Qt::ItemIsEnabled;
  }

private:
  QString m_name;
  int m_order = 0;
};

FilterItem::FilterItem(ILocatorFilter *filter) : m_filter(filter) {}

auto FilterItem::data(const int column, const int role) const -> QVariant
{
  switch (column) {
  case FilterName:
    if (role == Qt::DisplayRole || role == sort_role)
      return m_filter->displayName();
    break;
  case FilterPrefix:
    if (role == Qt::DisplayRole || role == sort_role || role == Qt::EditRole)
      return m_filter->shortcutString();
    break;
  case FilterIncludedByDefault:
    if (role == Qt::CheckStateRole || role == sort_role || role == Qt::EditRole)
      return m_filter->isIncludedByDefault() ? Qt::Checked : Qt::Unchecked;
    break;
  default:
    break;
  }

  if (role == Qt::ToolTipRole)
    return m_filter->description();

  return {};
}

auto FilterItem::flags(const int column) const -> Qt::ItemFlags
{
  if (column == FilterPrefix)
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;

  if (column == FilterIncludedByDefault)
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsUserCheckable;

  return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

auto FilterItem::setData(const int column, const QVariant &data, const int role) -> bool
{
  switch (column) {
  case FilterName:
    break;
  case FilterPrefix:
    if (role == Qt::EditRole && data.canConvert<QString>()) {
      m_filter->setShortcutString(data.toString());
      return true;
    }
    break;
  case FilterIncludedByDefault:
    if (role == Qt::CheckStateRole && data.canConvert<bool>()) {
      m_filter->setIncludedByDefault(data.toBool());
      return true;
    }
  default:
    break;
  }
  return false;
}

auto FilterItem::filter() const -> ILocatorFilter*
{
  return m_filter;
}

CategoryItem::CategoryItem(QString name, const int order) : m_name(std::move(name)), m_order(order) {}

auto CategoryItem::data(const int column, const int role) const -> QVariant
{
  Q_UNUSED(column)

  if (role == sort_role)
    return m_order;

  if (role == Qt::DisplayRole)
    return m_name;

  return {};
}

class LocatorSettingsWidget final : public IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::LocatorSettingsWidget)

public:
  LocatorSettingsWidget()
  {
    m_plugin = Locator::instance();
    m_filters = Locator::filters();
    m_custom_filters = m_plugin->customFilters();

    m_ui.setupUi(this);
    m_ui.refreshInterval->setToolTip(m_ui.refreshIntervalLabel->toolTip());
    m_ui.filterEdit->setFiltering(true);
    m_ui.filterList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui.filterList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui.filterList->setSortingEnabled(true);
    m_ui.filterList->setUniformRowHeights(true);
    m_ui.filterList->setActivationMode(DoubleClickActivation);

    m_model = new TreeModel<>(m_ui.filterList);
    initializeModel();

    m_proxy_model = new CategorySortFilterModel(m_ui.filterList);
    m_proxy_model->setSourceModel(m_model);
    m_proxy_model->setSortRole(sort_role);
    m_proxy_model->setFilterKeyColumn(-1/*all*/);

    m_ui.filterList->setModel(m_proxy_model);
    m_ui.filterList->expandAll();

    new HeaderViewStretcher(m_ui.filterList->header(), FilterName);
    m_ui.filterList->header()->setSortIndicator(FilterName, Qt::AscendingOrder);

    connect(m_ui.filterEdit, &FancyLineEdit::filterChanged, this, &LocatorSettingsWidget::setFilter);
    connect(m_ui.filterList->selectionModel(), &QItemSelectionModel::currentChanged, this, &LocatorSettingsWidget::updateButtonStates);
    connect(m_ui.filterList, &TreeView::activated, this, &LocatorSettingsWidget::configureFilter);
    connect(m_ui.editButton, &QPushButton::clicked, this, [this]() { configureFilter(m_ui.filterList->currentIndex()); });
    connect(m_ui.removeButton, &QPushButton::clicked, this, &LocatorSettingsWidget::removeCustomFilter);

    const auto add_menu = new QMenu(m_ui.addButton);
    add_menu->addAction(tr("Files in Directories"), this, [this] {
      addCustomFilter(new DirectoryFilter(Id(custom_directory_filter_baseid).withSuffix(m_custom_filters.size() + 1)));
    });

    add_menu->addAction(tr("URL Template"), this, [this] {
      const auto filter = new UrlLocatorFilter(Id(custom_url_filter_baseid).withSuffix(m_custom_filters.size() + 1));
      filter->setIsCustomFilter(true);
      addCustomFilter(filter);
    });

    m_ui.addButton->setMenu(add_menu);
    m_ui.refreshInterval->setValue(m_plugin->refreshInterval());

    saveFilterStates();
  }

  auto apply() -> void override;
  auto finish() -> void override;

private:
  auto updateButtonStates() const -> void;
  auto configureFilter(const QModelIndex &proxy_index) -> void;
  auto addCustomFilter(ILocatorFilter *filter) -> void;
  auto removeCustomFilter() -> void;
  auto initializeModel() -> void;
  auto saveFilterStates() -> void;
  auto restoreFilterStates() const -> void;
  auto requestRefresh() const -> void;
  auto setFilter(const QString &text) const -> void;

  Ui::LocatorSettingsWidget m_ui{};
  Locator *m_plugin = nullptr;
  TreeModel<> *m_model = nullptr;
  QSortFilterProxyModel *m_proxy_model = nullptr;
  TreeItem *m_custom_filter_root = nullptr;
  QList<ILocatorFilter*> m_filters;
  QList<ILocatorFilter*> m_added_filters;
  QList<ILocatorFilter*> m_removed_filters;
  QList<ILocatorFilter*> m_custom_filters;
  QList<ILocatorFilter*> m_refresh_filters;
  QHash<ILocatorFilter*, QByteArray> m_filter_states;
};

auto LocatorSettingsWidget::apply() -> void
{
  // Delete removed filters and clear added filters
  qDeleteAll(m_removed_filters);
  m_removed_filters.clear();
  m_added_filters.clear();

  // Pass the new configuration on to the plugin
  m_plugin->setFilters(m_filters);
  m_plugin->setCustomFilters(m_custom_filters);
  m_plugin->setRefreshInterval(m_ui.refreshInterval->value());
  requestRefresh();
  m_plugin->saveSettings();
  saveFilterStates();
}

auto LocatorSettingsWidget::finish() -> void
{
  // If settings were applied, this shouldn't change anything. Otherwise it
  // makes sure the filter states aren't changed permanently.
  restoreFilterStates();

  // Delete added filters and clear removed filters
  qDeleteAll(m_added_filters);
  m_added_filters.clear();
  m_removed_filters.clear();

  // Further cleanup
  m_filters.clear();
  m_custom_filters.clear();
  m_refresh_filters.clear();
}

auto LocatorSettingsWidget::requestRefresh() const -> void
{
  if (!m_refresh_filters.isEmpty())
    m_plugin->refresh(m_refresh_filters);
}

auto LocatorSettingsWidget::setFilter(const QString &text) const -> void
{
  m_proxy_model->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption));
  m_ui.filterList->expandAll();
}

auto LocatorSettingsWidget::saveFilterStates() -> void
{
  m_filter_states.clear();
  for (auto filter : qAsConst(m_filters))
    m_filter_states.insert(filter, filter->saveState());
}

auto LocatorSettingsWidget::restoreFilterStates() const -> void
{
  for (const auto filter_states_keys = m_filter_states.keys(); auto filter : filter_states_keys)
    filter->restoreState(m_filter_states.value(filter));
}

auto LocatorSettingsWidget::initializeModel() -> void
{
  m_model->setHeader({tr("Name"), tr("Prefix"), tr("Default")});
  m_model->setHeaderToolTip({QString(), ILocatorFilter::msgPrefixToolTip(), ILocatorFilter::msgIncludeByDefaultToolTip()});
  m_model->clear();

  const auto custom_filter_set = toSet(m_custom_filters);
  const auto built_in = new CategoryItem(tr("Built-in"), 0/*order*/);

  for (auto filter : qAsConst(m_filters))
    if (!filter->isHidden() && !custom_filter_set.contains(filter))
      built_in->appendChild(new FilterItem(filter));

  m_custom_filter_root = new CategoryItem(tr("Custom"), 1/*order*/);

  for (const auto custom_filter : qAsConst(m_custom_filters))
    m_custom_filter_root->appendChild(new FilterItem(custom_filter));

  m_model->rootItem()->appendChild(built_in);
  m_model->rootItem()->appendChild(m_custom_filter_root);
}

auto LocatorSettingsWidget::updateButtonStates() const -> void
{
  const auto current_index = m_proxy_model->mapToSource(m_ui.filterList->currentIndex());
  const auto selected = current_index.isValid();

  ILocatorFilter *filter = nullptr;

  if (selected) {
    if (const auto item = dynamic_cast<FilterItem*>(m_model->itemForIndex(current_index)))
      filter = item->filter();
  }

  m_ui.editButton->setEnabled(filter && filter->isConfigurable());
  m_ui.removeButton->setEnabled(filter && m_custom_filters.contains(filter));
}

auto LocatorSettingsWidget::configureFilter(const QModelIndex &proxy_index) -> void
{
  const auto index = m_proxy_model->mapToSource(proxy_index);
  QTC_ASSERT(index.isValid(), return);
  const auto item = dynamic_cast<FilterItem*>(m_model->itemForIndex(index));
  QTC_ASSERT(item, return);
  const auto filter = item->filter();
  QTC_ASSERT(filter->isConfigurable(), return);
  const auto included_by_default = filter->isIncludedByDefault();
  const auto shortcut_string = filter->shortcutString();
  auto needs_refresh = false;

  filter->openConfigDialog(this, needs_refresh);

  if (needs_refresh && !m_refresh_filters.contains(filter))
    m_refresh_filters.append(filter);

  if (filter->isIncludedByDefault() != included_by_default)
    item->updateColumn(FilterIncludedByDefault);

  if (filter->shortcutString() != shortcut_string)
    item->updateColumn(FilterPrefix);
}

auto LocatorSettingsWidget::addCustomFilter(ILocatorFilter *filter) -> void
{
  if (auto needs_refresh = false; filter->openConfigDialog(this, needs_refresh)) {
    m_filters.append(filter);
    m_added_filters.append(filter);
    m_custom_filters.append(filter);
    m_refresh_filters.append(filter);
    m_custom_filter_root->appendChild(new FilterItem(filter));
  }
}

auto LocatorSettingsWidget::removeCustomFilter() -> void
{
  const auto current_index = m_proxy_model->mapToSource(m_ui.filterList->currentIndex());
  QTC_ASSERT(current_index.isValid(), return);
  const auto item = dynamic_cast<FilterItem*>(m_model->itemForIndex(current_index));
  QTC_ASSERT(item, return);
  const auto filter = item->filter();
  QTC_ASSERT(m_custom_filters.contains(filter), return);

  m_model->destroyItem(item);
  m_filters.removeAll(filter);
  m_custom_filters.removeAll(filter);
  m_refresh_filters.removeAll(filter);

  if (m_added_filters.contains(filter)) {
    m_added_filters.removeAll(filter);
    delete filter;
  } else {
    m_removed_filters.append(filter);
  }
}

// LocatorSettingsPage

LocatorSettingsPage::LocatorSettingsPage()
{
  setId(filter_options_page);
  setDisplayName(QCoreApplication::translate("Locator", filter_options_page));
  setCategory(SETTINGS_CATEGORY_CORE);
  setWidgetCreator([] { return new LocatorSettingsWidget; });
}

} // namespace Orca::Plugin::Core
