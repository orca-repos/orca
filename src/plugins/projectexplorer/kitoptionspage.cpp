// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitoptionspage.hpp"

#include "filterkitaspectsdialog.hpp"
#include "kitmodel.hpp"
#include "kit.hpp"
#include "projectexplorerconstants.hpp"
#include "projectexplorericons.hpp"
#include "kitmanagerconfigwidget.hpp"
#include "kitmanager.hpp"

#include <utils/qtcassert.hpp>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------------
// KitOptionsPageWidget:
// --------------------------------------------------------------------------

class KitOptionsPageWidget : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjextExplorer::Internal::KitOptionsPageWidget)

public:
  KitOptionsPageWidget();

  auto currentIndex() const -> QModelIndex;
  auto currentKit() const -> Kit*;
  auto kitSelectionChanged() -> void;
  auto addNewKit() -> void;
  auto cloneKit() -> void;
  auto removeKit() -> void;
  auto makeDefaultKit() -> void;
  auto updateState() -> void;
  
  QTreeView *m_kitsView = nullptr;
  QPushButton *m_addButton = nullptr;
  QPushButton *m_cloneButton = nullptr;
  QPushButton *m_delButton = nullptr;
  QPushButton *m_makeDefaultButton = nullptr;
  QPushButton *m_filterButton = nullptr;
  QPushButton *m_defaultFilterButton = nullptr;
  KitModel *m_model = nullptr;
  QItemSelectionModel *m_selectionModel = nullptr;
  KitManagerConfigWidget *m_currentWidget = nullptr;
};

KitOptionsPageWidget::KitOptionsPageWidget()
{
  m_kitsView = new QTreeView(this);
  m_kitsView->setUniformRowHeights(true);
  m_kitsView->header()->setStretchLastSection(true);
  m_kitsView->setSizePolicy(m_kitsView->sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);

  m_addButton = new QPushButton(tr("Add"), this);
  m_cloneButton = new QPushButton(tr("Clone"), this);
  m_delButton = new QPushButton(tr("Remove"), this);
  m_makeDefaultButton = new QPushButton(tr("Make Default"), this);
  m_filterButton = new QPushButton(tr("Settings Filter..."), this);
  m_filterButton->setToolTip(tr("Choose which settings to display for this kit."));
  m_defaultFilterButton = new QPushButton(tr("Default Settings Filter..."), this);
  m_defaultFilterButton->setToolTip(tr("Choose which kit settings to display by default."));

  const auto buttonLayout = new QVBoxLayout;
  buttonLayout->setSpacing(6);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addWidget(m_addButton);
  buttonLayout->addWidget(m_cloneButton);
  buttonLayout->addWidget(m_delButton);
  buttonLayout->addWidget(m_makeDefaultButton);
  buttonLayout->addWidget(m_filterButton);
  buttonLayout->addWidget(m_defaultFilterButton);
  buttonLayout->addStretch();

  const auto horizontalLayout = new QHBoxLayout;
  horizontalLayout->addWidget(m_kitsView);
  horizontalLayout->addLayout(buttonLayout);

  const auto verticalLayout = new QVBoxLayout(this);
  verticalLayout->addLayout(horizontalLayout);

  m_model = new KitModel(verticalLayout, this);
  connect(m_model, &KitModel::kitStateChanged, this, &KitOptionsPageWidget::updateState);
  verticalLayout->setStretch(0, 1);
  verticalLayout->setStretch(1, 0);

  m_kitsView->setModel(m_model);
  m_kitsView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_kitsView->expandAll();

  m_selectionModel = m_kitsView->selectionModel();
  connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, &KitOptionsPageWidget::kitSelectionChanged);
  connect(KitManager::instance(), &KitManager::kitAdded, this, &KitOptionsPageWidget::kitSelectionChanged);
  connect(KitManager::instance(), &KitManager::kitRemoved, this, &KitOptionsPageWidget::kitSelectionChanged);
  connect(KitManager::instance(), &KitManager::kitUpdated, this, &KitOptionsPageWidget::kitSelectionChanged);

  // Set up add menu:
  connect(m_addButton, &QAbstractButton::clicked, this, &KitOptionsPageWidget::addNewKit);
  connect(m_cloneButton, &QAbstractButton::clicked, this, &KitOptionsPageWidget::cloneKit);
  connect(m_delButton, &QAbstractButton::clicked, this, &KitOptionsPageWidget::removeKit);
  connect(m_makeDefaultButton, &QAbstractButton::clicked, this, &KitOptionsPageWidget::makeDefaultKit);
  connect(m_filterButton, &QAbstractButton::clicked, this, [this] {
    QTC_ASSERT(m_currentWidget, return);
    FilterKitAspectsDialog dlg(m_currentWidget->workingCopy(), this);
    if (dlg.exec() == QDialog::Accepted) {
      m_currentWidget->workingCopy()->setIrrelevantAspects(dlg.irrelevantAspects());
      m_currentWidget->updateVisibility();
    }
  });
  connect(m_defaultFilterButton, &QAbstractButton::clicked, this, [this] {
    FilterKitAspectsDialog dlg(nullptr, this);
    if (dlg.exec() == QDialog::Accepted) {
      KitManager::setIrrelevantAspects(dlg.irrelevantAspects());
      m_model->updateVisibility();
    }
  });
  updateState();
}

auto KitOptionsPageWidget::kitSelectionChanged() -> void
{
  const auto current = currentIndex();
  const auto newWidget = m_model->widget(current);
  if (newWidget == m_currentWidget)
    return;

  if (m_currentWidget)
    m_currentWidget->setVisible(false);

  m_currentWidget = newWidget;

  if (m_currentWidget) {
    m_currentWidget->setVisible(true);
    m_kitsView->scrollTo(current);
  }

  updateState();
}

auto KitOptionsPageWidget::addNewKit() -> void
{
  const auto k = m_model->markForAddition(nullptr);

  const auto newIdx = m_model->indexOf(k);
  m_selectionModel->select(newIdx, QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

auto KitOptionsPageWidget::currentKit() const -> Kit*
{
  return m_model->kit(currentIndex());
}

auto KitOptionsPageWidget::cloneKit() -> void
{
  const auto current = currentKit();
  if (!current)
    return;

  const auto k = m_model->markForAddition(current);
  const auto newIdx = m_model->indexOf(k);
  m_kitsView->scrollTo(newIdx);
  m_selectionModel->select(newIdx, QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

auto KitOptionsPageWidget::removeKit() -> void
{
  if (const auto k = currentKit())
    m_model->markForRemoval(k);
}

auto KitOptionsPageWidget::makeDefaultKit() -> void
{
  m_model->setDefaultKit(currentIndex());
  updateState();
}

auto KitOptionsPageWidget::updateState() -> void
{
  if (!m_kitsView)
    return;

  auto canCopy = false;
  auto canDelete = false;
  auto canMakeDefault = false;

  if (const auto k = currentKit()) {
    canCopy = true;
    canDelete = !k->isAutoDetected();
    canMakeDefault = !m_model->isDefaultKit(k);
  }

  m_cloneButton->setEnabled(canCopy);
  m_delButton->setEnabled(canDelete);
  m_makeDefaultButton->setEnabled(canMakeDefault);
  m_filterButton->setEnabled(canCopy);
}

auto KitOptionsPageWidget::currentIndex() const -> QModelIndex
{
  if (!m_selectionModel)
    return QModelIndex();

  const auto idxs = m_selectionModel->selectedRows();
  if (idxs.count() != 1)
    return QModelIndex();
  return idxs.at(0);
}

} // namespace Internal

// --------------------------------------------------------------------------
// KitOptionsPage:
// --------------------------------------------------------------------------

static KitOptionsPage *theKitOptionsPage = nullptr;

KitOptionsPage::KitOptionsPage()
{
  theKitOptionsPage = this;
  setId(Constants::KITS_SETTINGS_PAGE_ID);
  setDisplayName(Internal::KitOptionsPageWidget::tr("Kits"));
  setCategory(Constants::KITS_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", "Kits"));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_kits.png");
}

auto KitOptionsPage::widget() -> QWidget*
{
  if (!m_widget)
    m_widget = new Internal::KitOptionsPageWidget;

  return m_widget;
}

auto KitOptionsPage::apply() -> void
{
  if (m_widget)
    m_widget->m_model->apply();
}

auto KitOptionsPage::finish() -> void
{
  if (m_widget) {
    delete m_widget;
    m_widget = nullptr;
  }
}

auto KitOptionsPage::showKit(Kit *k) -> void
{
  if (!k)
    return;

  (void)widget();
  const auto index = m_widget->m_model->indexOf(k);
  m_widget->m_selectionModel->select(index, QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
  m_widget->m_kitsView->scrollTo(index);
}

auto KitOptionsPage::instance() -> KitOptionsPage*
{
  return theKitOptionsPage;
}

} // namespace ProjectExplorer
