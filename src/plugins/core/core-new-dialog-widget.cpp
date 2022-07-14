// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-new-dialog-widget.hpp"
#include "ui_core-new-dialog.h"

#include "core-context-interface.hpp"
#include "core-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QDebug>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItem>

Q_DECLARE_METATYPE(Orca::Plugin::Core::IWizardFactory*)

using namespace Utils;

namespace Orca::Plugin::Core {
namespace {

constexpr int  icon_size = 48;
constexpr char last_category_key[] = "Core/NewDialog/LastCategory";
constexpr char last_platform_key[] = "Core/NewDialog/LastPlatform";
constexpr char allow_all_templates[] = "Core/NewDialog/AllowAllTemplates";
constexpr char show_platoform_filter[] = "Core/NewDialog/ShowPlatformFilter";
constexpr char blacklisted_categories_key[] = "Core/NewDialog/BlacklistedCategories";
constexpr char alternative_wizard_style[] = "Core/NewDialog/AlternativeWizardStyle";

class WizardFactoryContainer {
public:
  WizardFactoryContainer() = default;
  WizardFactoryContainer(IWizardFactory *w, const int i): wizard(w), wizard_option(i) {}

  IWizardFactory *wizard = nullptr;
  int wizard_option = 0;
};

auto factoryOfItem(const QStandardItem *item = nullptr) -> IWizardFactory*
{
  if (!item)
    return nullptr;
  return item->data(Qt::UserRole).value<WizardFactoryContainer>().wizard;
}

class PlatformFilterProxyModel final : public QSortFilterProxyModel {
public:
  explicit PlatformFilterProxyModel(QObject *parent = nullptr): QSortFilterProxyModel(parent)
  {
    m_blacklisted_categories = Id::fromStringList(ICore::settings()->value(blacklisted_categories_key).toStringList());
  }

  auto setPlatform(const Id platform) -> void
  {
    m_platform = platform;
    invalidateFilter();
  }

  auto manualReset() -> void
  {
    beginResetModel();
    endResetModel();
  }

  auto filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const -> bool override
  {
    if (!source_parent.isValid())
      return true;

    if (!source_parent.parent().isValid()) {
      // category
      const auto source_category_index = sourceModel()->index(source_row, 0, source_parent);
      for (auto i = 0; i < sourceModel()->rowCount(source_category_index); ++i)
        if (filterAcceptsRow(i, source_category_index))
          return true;
      return false;
    }

    const auto source_index = sourceModel()->index(source_row, 0, source_parent);
    if (const auto wizard = factoryOfItem(qobject_cast<QStandardItemModel*>(sourceModel())->itemFromIndex(source_index))) {
      if (m_blacklisted_categories.contains(Id::fromString(wizard->category())))
        return false;
      return wizard->isAvailable(m_platform);
    }

    return true;
  }

private:
  Id m_platform;
  QSet<Id> m_blacklisted_categories;
};

constexpr int row_height = 24;

class FancyTopLevelDelegate final : public QItemDelegate {
public:
  explicit FancyTopLevelDelegate(QObject *parent = nullptr) : QItemDelegate(parent) {}

  auto drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QString &text) const -> void override
  {
    auto new_option = option;

    if (!(option.state & QStyle::State_Enabled)) {
      QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
      gradient.setColorAt(0, option.palette.window().color().lighter(106));
      gradient.setColorAt(1, option.palette.window().color().darker(106));
      painter->fillRect(rect, gradient);
      painter->setPen(option.palette.window().color().darker(130));
      if (rect.top())
        painter->drawLine(rect.topRight(), rect.topLeft());
      painter->drawLine(rect.bottomRight(), rect.bottomLeft());
      // Fake enabled state
      new_option.state |= new_option.state | QStyle::State_Enabled;
    }

    QItemDelegate::drawDisplay(painter, new_option, rect, text);
  }

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize override
  {
    auto size = QItemDelegate::sizeHint(option, index);
    size = size.expandedTo(QSize(0, row_height));
    return size;
  }
};

} // namespace

Q_DECLARE_METATYPE(WizardFactoryContainer)

NewDialogWidget::NewDialogWidget(QWidget *parent) : QDialog(parent), m_ui(new Ui::NewDialog)
{
  setAttribute(Qt::WA_DeleteOnClose);
  ICore::registerWindow(this, Context("Core.NewDialog"));

  m_ui->setupUi(this);

  auto p = m_ui->frame->palette();
  p.setColor(QPalette::Window, p.color(QPalette::Base));

  m_ui->frame->setPalette(p);

  m_ok_button = m_ui->buttonBox->button(QDialogButtonBox::Ok);
  m_ok_button->setDefault(true);
  m_ok_button->setText(tr("Choose..."));

  m_model = new QStandardItemModel(this);

  m_filter_proxy_model = new PlatformFilterProxyModel(this);
  m_filter_proxy_model->setSourceModel(m_model);

  m_ui->templateCategoryView->setModel(m_filter_proxy_model);
  m_ui->templateCategoryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_ui->templateCategoryView->setItemDelegate(new FancyTopLevelDelegate(this));
  m_ui->templatesView->setModel(m_filter_proxy_model);
  m_ui->templatesView->setIconSize(QSize(icon_size, icon_size));

  if (ICore::settings()->value(alternative_wizard_style, false).toBool()) {
    m_ui->templatesView->setGridSize(QSize(256, 128));
    m_ui->templatesView->setIconSize(QSize(96, 96));
    m_ui->templatesView->setSpacing(4);
    m_ui->templatesView->setViewMode(QListView::IconMode);
    m_ui->templatesView->setMovement(QListView::Static);
    m_ui->templatesView->setResizeMode(QListView::Adjust);
    m_ui->templatesView->setSelectionRectVisible(false);
    m_ui->templatesView->setWrapping(true);
    m_ui->templatesView->setWordWrap(true);
  }

  connect(m_ui->templateCategoryView->selectionModel(), &QItemSelectionModel::currentChanged, this, &NewDialogWidget::currentCategoryChanged);
  connect(m_ui->templatesView->selectionModel(), &QItemSelectionModel::currentChanged, this, &NewDialogWidget::currentItemChanged);
  connect(m_ui->templatesView, &QListView::doubleClicked, this, &NewDialogWidget::accept);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &NewDialogWidget::accept);
  connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &NewDialogWidget::reject);
  connect(m_ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewDialogWidget::setSelectedPlatform);
}

// Sort by category. id
static auto wizardFactoryLessThan(const IWizardFactory *f1, const IWizardFactory *f2) -> bool
{
  if (const auto cc = f1->category().compare(f2->category()))
    return cc < 0;
  return f1->id().toString().compare(f2->id().toString()) < 0;
}

auto NewDialogWidget::setWizardFactories(QList<IWizardFactory*> factories, const FilePath &default_location, const QVariantMap &extra_variables) -> void
{
  m_default_location = default_location;
  m_extra_variables = extra_variables;
  std::ranges::stable_sort(factories, wizardFactoryLessThan);
  m_model->clear();


  const auto project_kind_item = new QStandardItem(tr("Projects"));
  project_kind_item->setData(IWizardFactory::ProjectWizard, Qt::UserRole);
  project_kind_item->setFlags({}); // disable item to prevent focus

  const auto files_kind_item = new QStandardItem(tr("Files and Classes"));
  files_kind_item->setData(IWizardFactory::FileWizard, Qt::UserRole);
  files_kind_item->setFlags({}); // disable item to prevent focus

  const auto parent_item = m_model->invisibleRootItem();
  parent_item->appendRow(project_kind_item);
  parent_item->appendRow(files_kind_item);

  const auto available_platforms = IWizardFactory::allAvailablePlatforms();

  if (ICore::settings()->value(allow_all_templates, true).toBool())
    m_ui->comboBox->addItem(tr("All Templates"), Id().toSetting());

  for (auto &platform : available_platforms) {
    const auto display_name_for_platform = IWizardFactory::displayNameForPlatform(platform);
    m_ui->comboBox->addItem(tr("%1 Templates").arg(display_name_for_platform), platform.toSetting());
  }

  m_ui->comboBox->setCurrentIndex(0); // "All templates"
  m_ui->comboBox->setEnabled(!available_platforms.isEmpty());

  if (const auto show_platform_filter = ICore::settings()->value(show_platoform_filter, true).toBool(); !show_platform_filter)
    m_ui->comboBox->hide();

  for (const auto factory : qAsConst(factories)) {
    QStandardItem *kind_item;
    switch (factory->kind()) {
    case IWizardFactory::ProjectWizard:
      kind_item = project_kind_item;
      break;
    default:
      kind_item = files_kind_item;
      break;
    }
    addItem(kind_item, factory);
  }

  if (files_kind_item->columnCount() == 0)
    parent_item->removeRow(1);

  if (project_kind_item->columnCount() == 0)
    parent_item->removeRow(0);
}

auto NewDialogWidget::showDialog() -> void
{
  QModelIndex idx;

  const auto last_platform = ICore::settings()->value(QLatin1String(last_platform_key)).toString();
  const auto last_category = ICore::settings()->value(QLatin1String(last_category_key)).toString();

  if (!last_platform.isEmpty()) {
    if (const auto index = m_ui->comboBox->findData(last_platform); index != -1)
      m_ui->comboBox->setCurrentIndex(index);
  }

  dynamic_cast<PlatformFilterProxyModel*>(m_filter_proxy_model)->manualReset();

  if (!last_category.isEmpty()) {
    for (const auto item : qAsConst(m_category_items)) {
      if (item->data(Qt::UserRole) == last_category)
        idx = m_filter_proxy_model->mapFromSource(m_model->indexFromItem(item));
    }
  }

  if (!idx.isValid())
    idx = m_filter_proxy_model->index(0, 0, m_filter_proxy_model->index(0, 0));

  m_ui->templateCategoryView->setCurrentIndex(idx);
  m_ui->templateCategoryView->setFocus(Qt::NoFocusReason);  // We need to ensure that the category has default focus

  for (auto row = 0; row < m_filter_proxy_model->rowCount(); ++row)
    m_ui->templateCategoryView->setExpanded(m_filter_proxy_model->index(row, 0), true);

  // Ensure that item description is visible on first show
  currentItemChanged(m_filter_proxy_model->index(0, 0, m_ui->templatesView->rootIndex()));
  updateOkButton();
  show();
}

auto NewDialogWidget::selectedPlatform() const -> Id
{
  const auto index = m_ui->comboBox->currentIndex();
  return Id::fromSetting(m_ui->comboBox->itemData(index));
}

auto NewDialogWidget::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ShortcutOverride) {
    if (const auto ke = dynamic_cast<QKeyEvent*>(event); ke->key() == Qt::Key_Escape && !ke->modifiers()) {
      ke->accept();
      return true;
    }
  }
  return QDialog::event(event);
}

NewDialogWidget::~NewDialogWidget()
{
  delete m_ui;
}

auto NewDialogWidget::currentWizardFactory() const -> IWizardFactory*
{
  const auto index = m_filter_proxy_model->mapToSource(m_ui->templatesView->currentIndex());
  return factoryOfItem(m_model->itemFromIndex(index));
}

auto NewDialogWidget::addItem(QStandardItem *top_level_category_item, IWizardFactory *factory) -> void
{
  const auto category_name = factory->category();
  QStandardItem *category_item = nullptr;

  for (auto i = 0; i < top_level_category_item->rowCount(); i++) {
    if (top_level_category_item->child(i, 0)->data(Qt::UserRole) == category_name)
      category_item = top_level_category_item->child(i, 0);
  }

  if (!category_item) {
    category_item = new QStandardItem();
    top_level_category_item->appendRow(category_item);
    m_category_items.append(category_item);
    category_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    category_item->setText(QLatin1String("  ") + factory->displayCategory());
    category_item->setData(factory->category(), Qt::UserRole);
  }

  const auto wizard_item = new QStandardItem(factory->icon(), factory->displayName());
  wizard_item->setData(QVariant::fromValue(WizardFactoryContainer(factory, 0)), Qt::UserRole);
  wizard_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

  category_item->appendRow(wizard_item);
}

auto NewDialogWidget::currentCategoryChanged(const QModelIndex &index) const -> void
{
  if (index.parent() != m_model->invisibleRootItem()->index()) {
    auto source_index = m_filter_proxy_model->mapToSource(index);
    source_index = m_filter_proxy_model->mapFromSource(source_index);
    m_ui->templatesView->setRootIndex(source_index);
    // Focus the first item by default
    m_ui->templatesView->setCurrentIndex(m_filter_proxy_model->index(0, 0, m_ui->templatesView->rootIndex()));
  }
}

auto NewDialogWidget::currentItemChanged(const QModelIndex &index) -> void
{
  const auto source_index = m_filter_proxy_model->mapToSource(index);
  const auto cat = m_model->itemFromIndex(source_index);

  if (const IWizardFactory *wizard = factoryOfItem(cat)) {
    auto desciption = wizard->description();
    QStringList display_names_for_supported_platforms;

    for (const auto platforms = wizard->supportedPlatforms(); const auto platform : platforms)
      display_names_for_supported_platforms << IWizardFactory::displayNameForPlatform(platform);

    sort(display_names_for_supported_platforms);

    if (!Qt::mightBeRichText(desciption))
      desciption.replace(QLatin1Char('\n'), QLatin1String("<br>"));

    desciption += QLatin1String("<br><br><b>");

    if (wizard->flags().testFlag(IWizardFactory::PlatformIndependent))
      desciption += tr("Platform independent") + QLatin1String("</b>");
    else
      desciption += tr("Supported Platforms") + QLatin1String("</b>: <ul>") + "<li>" + display_names_for_supported_platforms.join("</li><li>") + "</li>" + QLatin1String("</ul>");

    m_ui->templateDescription->setHtml(desciption);

    if (!wizard->descriptionImage().isEmpty()) {
      m_ui->imageLabel->setVisible(true);
      m_ui->imageLabel->setPixmap(wizard->descriptionImage());
    } else {
      m_ui->imageLabel->setVisible(false);
    }
  } else {
    m_ui->templateDescription->clear();
  }

  updateOkButton();
}

auto NewDialogWidget::saveState() const -> void
{
  const auto filter_idx = m_ui->templateCategoryView->currentIndex();
  const auto idx = m_filter_proxy_model->mapToSource(filter_idx);
  const auto current_item = m_model->itemFromIndex(idx);

  if (current_item)
    ICore::settings()->setValue(last_category_key, current_item->data(Qt::UserRole));

  ICore::settings()->setValueWithDefault(last_platform_key, m_ui->comboBox->currentData().toString());
}

static auto runWizard(IWizardFactory *wizard, const FilePath &default_location, const Id platform, const QVariantMap &variables) -> void
{
  const auto path = wizard->runPath(default_location);
  wizard->runWizard(path, ICore::dialogParent(), platform, variables);
}

auto NewDialogWidget::accept() -> void
{
  saveState();
  if (m_ui->templatesView->currentIndex().isValid()) {
    if (auto wizard = currentWizardFactory(); QTC_GUARD(wizard)) {
      QMetaObject::invokeMethod(wizard, [wizard, this, capture0 = selectedPlatform()] { return runWizard(wizard, m_default_location, capture0, m_extra_variables); }, Qt::QueuedConnection);
    }
  }
  QDialog::accept();
}

auto NewDialogWidget::reject() -> void
{
  saveState();
  QDialog::reject();
}

auto NewDialogWidget::updateOkButton() const -> void
{
  m_ok_button->setEnabled(currentWizardFactory() != nullptr);
}

auto NewDialogWidget::setSelectedPlatform(int /*platform*/) const -> void
{
  //The static cast allows us to keep PlatformFilterProxyModel anonymous
  dynamic_cast<PlatformFilterProxyModel*>(m_filter_proxy_model)->setPlatform(selectedPlatform());
}

} // namespace Orca::Plugin::Core

