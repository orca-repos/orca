// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "settingsdialog.h"

#include <core/icore.h>
#include <core/dialogs/ioptionspage.h>
#include <core/iwizardfactory.h>

#include <utils/algorithm.h>
#include <utils/hostosinfo.h>
#include <utils/fancylineedit.h>
#include <utils/qtcassert.h>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSpacerItem>
#include <QStackedLayout>
#include <QStyle>
#include <QStyledItemDelegate>

const int kInitialWidth = 750;
const int kInitialHeight = 450;
const int kMaxMinimumWidth = 250;
const int kMaxMinimumHeight = 250;

static const char pageKeyC[] = "General/LastPreferencePage";
const int categoryIconSize = 24;

using namespace Utils;

namespace Core {
namespace Internal {

auto optionsPageLessThan(const IOptionsPage *p1, const IOptionsPage *p2) -> bool
{
  if (p1->category() != p2->category())
    return p1->category().alphabeticallyBefore(p2->category());
  return p1->id().alphabeticallyBefore(p2->id());
}

static auto sortedOptionsPages() -> QList<IOptionsPage*>
{
  auto rc = IOptionsPage::allOptionsPages();
  std::ranges::stable_sort(rc, optionsPageLessThan);
  return rc;
}

class Category {
public:
  auto findPageById(const Id id, int *page_index) const -> bool
  {
    *page_index = indexOf(pages, equal(&IOptionsPage::id, id));
    return *page_index != -1;
  }

  Id id;
  int index = -1;
  QString display_name;
  QIcon icon;
  QList<IOptionsPage*> pages;
  QList<IOptionsPageProvider*> providers;
  bool provider_pages_created = false;
  QTabWidget *tab_widget = nullptr;
};

class CategoryModel final : public QAbstractListModel {
  Q_DISABLE_COPY_MOVE(CategoryModel)

public:
  CategoryModel();
  ~CategoryModel() override;

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setPages(const QList<IOptionsPage*> &pages, const QList<IOptionsPageProvider*> &providers) -> void;
  auto ensurePages(Category *category) const -> void;
  auto categories() const -> const QList<Category*>& { return m_categories; }

private:
  auto findCategoryById(Id id) -> Category*;

  QList<Category*> m_categories;
  QSet<Id> m_page_ids;
  QIcon m_empty_icon;
};

CategoryModel::CategoryModel()
{
  QPixmap empty(categoryIconSize, categoryIconSize);
  empty.fill(Qt::transparent);
  m_empty_icon = QIcon(empty);
}

CategoryModel::~CategoryModel()
{
  qDeleteAll(m_categories);
}

auto CategoryModel::rowCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : static_cast<int>(m_categories.size());
}

auto CategoryModel::data(const QModelIndex &index, const int role) const -> QVariant
{
  switch (role) {
  case Qt::DisplayRole:
    return m_categories.at(index.row())->display_name;
  case Qt::DecorationRole: {
    auto &icon = m_categories.at(index.row())->icon;
    if (icon.isNull())
      icon = m_empty_icon;
    return icon;
  }
  default:
    break;
  }

  return {};
}

auto CategoryModel::setPages(const QList<IOptionsPage*> &pages, const QList<IOptionsPageProvider*> &providers) -> void
{
  beginResetModel();

  // Clear any previous categories
  qDeleteAll(m_categories);
  m_categories.clear();
  m_page_ids.clear();

  // Put the pages in categories
  for (const auto page : pages) {
    QTC_ASSERT(!m_page_ids.contains(page->id()), qWarning("duplicate options page id '%s'", qPrintable(page->id().toString())));
    m_page_ids.insert(page->id());

    const auto category_id = page->category();
    auto category = findCategoryById(category_id);

    if (!category) {
      category = new Category;
      category->id = category_id;
      category->tab_widget = nullptr;
      category->index = -1;
      m_categories.append(category);
    }

    if (category->display_name.isEmpty())
      category->display_name = page->displayCategory();

    if (category->icon.isNull())
      category->icon = page->categoryIcon();

    category->pages.append(page);
  }

  for (const auto provider : providers) {
    const auto category_id = provider->category();
    auto category = findCategoryById(category_id);

    if (!category) {
      category = new Category;
      category->id = category_id;
      category->tab_widget = nullptr;
      category->index = -1;
      m_categories.append(category);
    }

    if (category->display_name.isEmpty())
      category->display_name = provider->displayCategory();

    if (category->icon.isNull())
      category->icon = provider->categoryIcon();

    category->providers.append(provider);
  }

  Utils::sort(m_categories, [](const Category *c1, const Category *c2) {
    return c1->id.alphabeticallyBefore(c2->id);
  });

  endResetModel();
}

auto CategoryModel::ensurePages(Category *category) const -> void
{
  if (!category->provider_pages_created) {
    QList<IOptionsPage*> created_pages;
    for (const auto provider : qAsConst(category->providers))
      created_pages += provider->pages();

    // check for duplicate ids
    for (const IOptionsPage *page : qAsConst(created_pages)) {
      QTC_ASSERT(!m_page_ids.contains(page->id()), qWarning("duplicate options page id '%s'", qPrintable(page->id().toString())));
    }

    category->pages += created_pages;
    category->provider_pages_created = true;
    std::ranges::stable_sort(category->pages, optionsPageLessThan);
  }
}

auto CategoryModel::findCategoryById(const Id id) -> Category*
{
  for (const auto category : m_categories) {
    if (category->id == id)
      return category;
  }

  return nullptr;
}

// ----------- Category filter model

/**
 * A filter model that returns true for each category node that has pages that
 * match the search string.
 */
class CategoryFilterModel final : public QSortFilterProxyModel {
public:
  CategoryFilterModel() = default;

protected:
  auto filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool override;
};

auto CategoryFilterModel::filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const -> bool
{
  // Regular contents check, then check page-filter.
  if (QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
    return true;

  const auto regex = filterRegularExpression();
  const CategoryModel *cm = dynamic_cast<CategoryModel*>(sourceModel());
  const Category *category = cm->categories().at(source_row);

  for (const IOptionsPage *page : category->pages) {
    if (page->displayCategory().contains(regex) || page->displayName().contains(regex) || page->matches(regex))
      return true;
  }

  if (!category->provider_pages_created) {
    for (const IOptionsPageProvider *provider : category->providers) {
      if (provider->matches(regex))
        return true;
    }
  }

  return false;
}

class CategoryListViewDelegate final : public QStyledItemDelegate {
public:
  explicit CategoryListViewDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override
  {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), 32));
    return size;
  }
};

/**
 * Special version of a QListView that has the width of the first column as
 * minimum size.
 */
class CategoryListView final : public QListView {
public:
  CategoryListView()
  {
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setItemDelegate(new CategoryListViewDelegate(this));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  auto sizeHint() const -> QSize override
  {
    auto width = sizeHintForColumn(0) + frameWidth() * 2 + 5;
    width += verticalScrollBar()->sizeHint().width();
    return {width, 100};
  }

  // QListView installs a event filter on its scrollbars
  auto eventFilter(QObject *obj, QEvent *event) -> bool final
  {
    if (obj == verticalScrollBar() && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
      updateGeometry();

    return QListView::eventFilter(obj, event);
  }
};

class SmartScrollArea final : public QScrollArea {
public:
  explicit SmartScrollArea(QWidget *parent) : QScrollArea(parent)
  {
    setFrameStyle(NoFrame | Plain);
    viewport()->setAutoFillBackground(false);
    setWidgetResizable(true);
  }

private:
  auto resizeEvent(QResizeEvent *event) -> void final
  {
    if (const auto inner = widget()) {
      const auto fw = frameWidth() * 2;
      auto inner_size = event->size() - QSize(fw, fw);

      if (const auto inner_size_hint = inner->minimumSizeHint(); inner_size_hint.height() > inner_size.height()) {
        // Widget wants to be bigger than available space
        inner_size.setWidth(inner_size.width() - scrollBarWidth());
        inner_size.setHeight(inner_size_hint.height());
      }
      inner->resize(inner_size);
    }
    QScrollArea::resizeEvent(event);
  }

  auto minimumSizeHint() const -> QSize final
  {
    if (const auto inner = widget()) {
      const auto fw = frameWidth() * 2;
      auto min_size = inner->minimumSizeHint();
      min_size += QSize(fw, fw);
      min_size += QSize(scrollBarWidth(), 0);
      min_size.setWidth(qMin(min_size.width(), kMaxMinimumWidth));
      min_size.setHeight(qMin(min_size.height(), kMaxMinimumHeight));
      return min_size;
    }
    return {0, 0};
  }

  auto event(QEvent *event) -> bool final
  {
    if (event->type() == QEvent::LayoutRequest)
      updateGeometry();
    return QScrollArea::event(event);
  }

  auto scrollBarWidth() const -> int
  {
    const auto that = const_cast<SmartScrollArea*>(this);
    auto list = that->scrollBarWidgets(Qt::AlignRight);

    if (list.isEmpty())
      return 0;

    return list.first()->sizeHint().width();
  }
};

class SettingsDialog final : public QDialog {
public:
  explicit SettingsDialog(QWidget *parent);

  auto showPage(Id page_id) -> void;
  auto execDialog() -> bool;

private:
  // Make sure the settings dialog starts up as small as possible.
  auto sizeHint() const -> QSize override { return minimumSize(); }
  auto done(int) -> void override;
  auto accept() -> void override;
  auto reject() -> void override;
  auto apply() -> void;
  auto currentChanged(const QModelIndex &current) -> void;
  auto currentTabChanged(int) -> void;
  auto filter(const QString &text) const -> void;
  auto createGui() -> void;
  auto showCategory(int index) -> void;
  static auto updateEnabledTabs(const Category *category, const QString &search_text) -> void;
  auto ensureCategoryWidget(Category *category) -> void;
  auto disconnectTabWidgets() -> void;

  const QList<IOptionsPage*> m_pages;
  QSet<IOptionsPage*> m_visited_pages;
  CategoryFilterModel m_proxy_model;
  CategoryModel m_model;
  Id m_current_category;
  Id m_current_page;
  QStackedLayout *m_stacked_layout;
  FancyLineEdit *m_filter_line_edit;
  QListView *m_category_list;
  QLabel *m_header_label;
  std::vector<QEventLoop*> m_event_loops;
  bool m_running = false;
  bool m_applied = false;
  bool m_finished = false;
};

static QPointer<SettingsDialog> m_instance = nullptr;

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), m_pages(sortedOptionsPages()), m_stacked_layout(new QStackedLayout), m_filter_line_edit(new FancyLineEdit), m_category_list(new CategoryListView), m_header_label(new QLabel)
{
  m_filter_line_edit->setFiltering(true);

  createGui();

  setWindowTitle(QCoreApplication::translate("Core::Internal::SettingsDialog", "Preferences"));

  m_model.setPages(m_pages, IOptionsPageProvider::allOptionsPagesProviders());
  m_proxy_model.setSourceModel(&m_model);
  m_proxy_model.setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_category_list->setIconSize(QSize(categoryIconSize, categoryIconSize));
  m_category_list->setModel(&m_proxy_model);
  m_category_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_category_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  connect(m_category_list->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SettingsDialog::currentChanged);

  // The order of the slot connection matters here, the filter slot
  // opens the matching page after the model has filtered.
  connect(m_filter_line_edit, &FancyLineEdit::filterChanged, &m_proxy_model, [this](const QString &filter) {
    m_proxy_model.setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(filter), QRegularExpression::CaseInsensitiveOption));
  });

  connect(m_filter_line_edit, &FancyLineEdit::filterChanged, this, &SettingsDialog::filter);
  m_category_list->setFocus();
}

auto SettingsDialog::showPage(const Id page_id) -> void
{
  // handle the case of "show last page"
  auto initial_page_id = page_id;
  if (!initial_page_id.isValid()) {
    const QSettings *settings = ICore::settings();
    initial_page_id = Id::fromSetting(settings->value(QLatin1String(pageKeyC)));
  }

  auto initial_category_index = -1;
  auto initial_page_index = -1;

  const auto &categories = m_model.categories();
  if (initial_page_id.isValid()) {
    // First try categories without lazy items.
    for (auto i = 0; i < categories.size(); ++i) {
      if (const auto category = categories.at(i); category->providers.isEmpty()) {
        // no providers
        if (category->findPageById(initial_page_id, &initial_page_index)) {
          initial_category_index = i;
          break;
        }
      }
    }

    if (initial_page_index == -1) {
      // On failure, expand the remaining items.
      for (auto i = 0; i < categories.size(); ++i) {
        if (const auto category = categories.at(i); !category->providers.isEmpty()) {
          // has providers
          ensureCategoryWidget(category);
          if (category->findPageById(initial_page_id, &initial_page_index)) {
            initial_category_index = i;
            break;
          }
        }
      }
    }
  }

  if (initial_page_id.isValid() && initial_page_index == -1)
    return; // Unknown settings page, probably due to missing plugin.

  if (initial_category_index != -1) {
    auto model_index = m_proxy_model.mapFromSource(m_model.index(initial_category_index));
    if (!model_index.isValid()) {
      // filtered out, so clear filter first
      m_filter_line_edit->setText(QString());
      model_index = m_proxy_model.mapFromSource(m_model.index(initial_category_index));
    }
    m_category_list->setCurrentIndex(model_index);
    if (initial_page_index != -1) {
      if (QTC_GUARD(categories.at(initial_category_index)->tab_widget))
        categories.at(initial_category_index)->tab_widget->setCurrentIndex(initial_page_index);
    }
  }
}

auto SettingsDialog::createGui() -> void
{
  // Header label with large font and a bit of spacing (align with group boxes)
  auto header_label_font = m_header_label->font();
  header_label_font.setBold(true);

  // Paranoia: Should a font be set in pixels...
  if (const auto point_size = header_label_font.pointSize(); point_size > 0)
    header_label_font.setPointSize(point_size + 2);

  m_header_label->setFont(header_label_font);

  const auto header_h_layout = new QHBoxLayout;
  const auto left_margin = QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
  header_h_layout->addSpacerItem(new QSpacerItem(left_margin, 0, QSizePolicy::Fixed, QSizePolicy::Ignored));
  header_h_layout->addWidget(m_header_label);

  m_stacked_layout->setContentsMargins(0, 0, 0, 0);
  const auto empty_widget = new QWidget(this);
  m_stacked_layout->addWidget(empty_widget); // no category selected, for example when filtering

  const auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
  connect(button_box->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &SettingsDialog::apply);
  connect(button_box, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

  const auto main_grid_layout = new QGridLayout;
  main_grid_layout->addWidget(m_filter_line_edit, 0, 0, 1, 1);
  main_grid_layout->addLayout(header_h_layout, 0, 1, 1, 1);
  main_grid_layout->addWidget(m_category_list, 1, 0, 1, 1);
  main_grid_layout->addLayout(m_stacked_layout, 1, 1, 1, 1);
  main_grid_layout->addWidget(button_box, 2, 0, 1, 2);
  main_grid_layout->setColumnStretch(1, 4);
  setLayout(main_grid_layout);

  button_box->button(QDialogButtonBox::Ok)->setDefault(true);
  main_grid_layout->setSizeConstraint(QLayout::SetMinimumSize);
}

auto SettingsDialog::showCategory(const int index) -> void
{
  const auto category = m_model.categories().at(index);
  ensureCategoryWidget(category);

  // Update current category and page
  m_current_category = category->id;

  if (const auto current_tab_index = category->tab_widget->currentIndex(); current_tab_index != -1) {
    const auto page = category->pages.at(current_tab_index);
    m_current_page = page->id();
    m_visited_pages.insert(page);
  }

  m_stacked_layout->setCurrentIndex(category->index);
  m_header_label->setText(category->display_name);
  updateEnabledTabs(category, m_filter_line_edit->text());
}

auto SettingsDialog::ensureCategoryWidget(Category *category) -> void
{
  if (category->tab_widget)
    return;

  m_model.ensurePages(category);
  const auto tab_widget = new QTabWidget;
  tab_widget->tabBar()->setObjectName("qc_settings_main_tabbar"); // easier lookup in Squish

  for (const auto page : qAsConst(category->pages)) {
    const auto widget = page->widget();
    ICore::setupScreenShooter(page->displayName(), widget);
    const auto ssa = new SmartScrollArea(this);
    ssa->setWidget(widget);
    widget->setAutoFillBackground(false);
    tab_widget->addTab(ssa, page->displayName());
  }

  connect(tab_widget, &QTabWidget::currentChanged, this, &SettingsDialog::currentTabChanged);

  category->tab_widget = tab_widget;
  category->index = m_stacked_layout->addWidget(tab_widget);
}

auto SettingsDialog::disconnectTabWidgets() -> void
{
  for (const auto category : m_model.categories()) {
    if (category->tab_widget)
      disconnect(category->tab_widget, &QTabWidget::currentChanged, this, &SettingsDialog::currentTabChanged);
  }
}

auto SettingsDialog::updateEnabledTabs(const Category *category, const QString &search_text) -> void
{
  auto first_enabled_tab = -1;
  const QRegularExpression regex(QRegularExpression::escape(search_text), QRegularExpression::CaseInsensitiveOption);

  for (auto i = 0; i < category->pages.size(); ++i) {
    const IOptionsPage *page = category->pages.at(i);
    const auto enabled = search_text.isEmpty() || page->category().toString().contains(regex) || page->displayName().contains(regex) || page->matches(regex);
    category->tab_widget->setTabEnabled(i, enabled);
    if (enabled && first_enabled_tab < 0)
      first_enabled_tab = i;
  }

  if (!category->tab_widget->isTabEnabled(category->tab_widget->currentIndex()) && first_enabled_tab != -1) {
    // QTabWidget is dumb, so this can happen
    category->tab_widget->setCurrentIndex(first_enabled_tab);
  }
}

auto SettingsDialog::currentChanged(const QModelIndex &current) -> void
{
  if (current.isValid()) {
    showCategory(m_proxy_model.mapToSource(current).row());
  } else {
    m_stacked_layout->setCurrentIndex(0);
    m_header_label->clear();
  }
}

auto SettingsDialog::currentTabChanged(const int index) -> void
{
  if (index == -1)
    return;

  const auto model_index = m_proxy_model.mapToSource(m_category_list->currentIndex());
  if (!model_index.isValid())
    return;

  // Remember the current tab and mark it as visited
  const Category *category = m_model.categories().at(model_index.row());
  const auto page = category->pages.at(index);
  m_current_page = page->id();
  m_visited_pages.insert(page);
}

auto SettingsDialog::filter(const QString &text) const -> void
{
  // When there is no current index, select the first one when possible
  if (!m_category_list->currentIndex().isValid() && m_model.rowCount() > 0)
    m_category_list->setCurrentIndex(m_proxy_model.index(0, 0));

  const auto current_index = m_proxy_model.mapToSource(m_category_list->currentIndex());
  if (!current_index.isValid())
    return;

  const auto category = m_model.categories().at(current_index.row());
  updateEnabledTabs(category, text);
}

auto SettingsDialog::accept() -> void
{
  if (m_finished)
    return;
  m_finished = true;
  disconnectTabWidgets();
  m_applied = true;

  for (const auto page : qAsConst(m_visited_pages))
    page->apply();

  for (const auto page : qAsConst(m_pages))
    page->finish();

  done(Accepted);
}

auto SettingsDialog::reject() -> void
{
  if (m_finished)
    return;

  m_finished = true;

  disconnectTabWidgets();

  for (const auto page : qAsConst(m_pages))
    page->finish();

  done(Rejected);
}

auto SettingsDialog::apply() -> void
{
  for (const auto page : qAsConst(m_visited_pages))
    page->apply();

  m_applied = true;
}

auto SettingsDialog::done(const int val) -> void
{
  QSettings *settings = ICore::settings();
  settings->setValue(QLatin1String(pageKeyC), m_current_page.toSetting());
  ICore::saveSettings(ICore::SettingsDialogDone); // save all settings

  // exit event loops in reverse order of addition
  for (const auto event_loop : m_event_loops)
    event_loop->exit();

  m_event_loops.clear();
  QDialog::done(val);
}

auto SettingsDialog::execDialog() -> bool
{
  if (!m_running) {
    m_running = true;
    m_finished = false;
    static const QLatin1String k_preference_dialog_size("Core/PreferenceDialogSize");
    if (ICore::settings()->contains(k_preference_dialog_size))
      resize(ICore::settings()->value(k_preference_dialog_size).toSize());
    else
      resize(kInitialWidth, kInitialHeight);
    exec();
    m_running = false;
    m_instance = nullptr;
    ICore::settings()->setValueWithDefault(k_preference_dialog_size, size(), QSize(kInitialWidth, kInitialHeight));
    // make sure that the current "single" instance is deleted
    // we can't delete right away, since we still access the m_applied member
    deleteLater();
  } else {
    // exec dialog is called while the instance is already running
    // this can happen when a event triggers a code path that wants to
    // show the settings dialog again
    // e.g. when starting the debugger (with non-built debugging helpers),
    // and manually opening the settings dialog, after the debugger hit
    // a break point it will complain about missing helper, and offer the
    // option to open the settings dialog.
    // Keep the UI running by creating another event loop.
    QEventLoop event_loop;
    m_event_loops.emplace(m_event_loops.begin(), &event_loop);
    event_loop.exec();
    QTC_ASSERT(m_event_loops.empty(), return m_applied;);
  }
  return m_applied;
}

auto executeSettingsDialog(QWidget *parent, const Id initial_page) -> bool
{
  // Make sure all wizards are there when the user might access the keyboard shortcuts:
  (void)IWizardFactory::allWizardFactories();

  if (!m_instance)
    m_instance = new SettingsDialog(parent);

  m_instance->showPage(initial_page);
  return m_instance->execDialog();
}

} // namespace Internal
} // namespace Core
