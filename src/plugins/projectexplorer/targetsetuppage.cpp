// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "targetsetuppage.hpp"
#include "buildconfiguration.hpp"
#include "buildinfo.hpp"
#include "importwidget.hpp"
#include "kit.hpp"
#include "kitmanager.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "session.hpp"
#include "target.hpp"
#include "targetsetupwidget.hpp"
#include "task.hpp"

#include <core/core-interface.hpp>

#include <projectexplorer/ipotentialkit.hpp>

#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/wizard.hpp>
#include <utils/algorithm.hpp>
#include <utils/fancylineedit.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {

static QList<IPotentialKit *> g_potentialKits;

IPotentialKit::IPotentialKit()
{
  g_potentialKits.append(this);
}

IPotentialKit::~IPotentialKit()
{
  g_potentialKits.removeOne(this);
}

namespace Internal {

static auto importDirectory(const FilePath &projectPath) -> FilePath
{
  // Setup import widget:
  auto path = projectPath;
  path = path.parentDir(); // base dir
  path = path.parentDir(); // parent dir

  return path;
}

class TargetSetupPageUi {
public:
  QWidget *centralWidget;
  QWidget *scrollAreaWidget;
  QScrollArea *scrollArea;
  QLabel *headerLabel;
  QLabel *noValidKitLabel;
  QCheckBox *allKitsCheckBox;
  FancyLineEdit *kitFilterLineEdit;

  auto setupUi(TargetSetupPage *q) -> void
  {
    const auto setupTargetPage = new QWidget(q);

    headerLabel = new QLabel(setupTargetPage);
    headerLabel->setWordWrap(true);
    headerLabel->setVisible(false);

    noValidKitLabel = new QLabel(setupTargetPage);
    noValidKitLabel->setWordWrap(true);
    noValidKitLabel->setText("<span style=\" font-weight:600;\">" + TargetSetupPage::tr("No suitable kits found.") + "</span><br/>" + TargetSetupPage::tr("Add a kit in the <a href=\"buildandrun\">" "options</a> or via the maintenance tool of" " the SDK."));
    noValidKitLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    noValidKitLabel->setVisible(false);

    allKitsCheckBox = new QCheckBox(setupTargetPage);
    allKitsCheckBox->setTristate(true);
    allKitsCheckBox->setText(TargetSetupPage::tr("Select all kits"));

    kitFilterLineEdit = new FancyLineEdit(setupTargetPage);
    kitFilterLineEdit->setFiltering(true);
    kitFilterLineEdit->setPlaceholderText(TargetSetupPage::tr("Type to filter kits by name..."));

    centralWidget = new QWidget(setupTargetPage);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    policy.setHorizontalStretch(0);
    policy.setVerticalStretch(0);
    policy.setHeightForWidth(centralWidget->sizePolicy().hasHeightForWidth());
    centralWidget->setSizePolicy(policy);

    scrollAreaWidget = new QWidget(setupTargetPage);
    scrollArea = new QScrollArea(scrollAreaWidget);
    scrollArea->setWidgetResizable(true);

    const auto scrollAreaWidgetContents = new QWidget();
    scrollAreaWidgetContents->setGeometry(QRect(0, 0, 230, 81));
    scrollArea->setWidget(scrollAreaWidgetContents);

    const auto verticalLayout = new QVBoxLayout(scrollAreaWidget);
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->addWidget(scrollArea);

    const auto verticalLayout_2 = new QVBoxLayout(setupTargetPage);
    verticalLayout_2->addWidget(headerLabel);
    verticalLayout_2->addWidget(kitFilterLineEdit);
    verticalLayout_2->addWidget(noValidKitLabel);
    verticalLayout_2->addWidget(allKitsCheckBox);
    verticalLayout_2->addWidget(centralWidget);
    verticalLayout_2->addWidget(scrollAreaWidget);

    const auto verticalLayout_3 = new QVBoxLayout(q);
    verticalLayout_3->setContentsMargins(0, 0, 0, -1);
    verticalLayout_3->addWidget(setupTargetPage);

    QObject::connect(noValidKitLabel, &QLabel::linkActivated, q, &TargetSetupPage::openOptions);

    QObject::connect(allKitsCheckBox, &QAbstractButton::clicked, q, &TargetSetupPage::changeAllKitsSelections);

    QObject::connect(kitFilterLineEdit, &FancyLineEdit::filterChanged, q, &TargetSetupPage::kitFilterChanged);
  }
};

} // namespace Internal

static auto defaultTasksGenerator(const TasksGenerator &childGenerator) -> TasksGenerator
{
  return [childGenerator](const Kit *k) -> Tasks {
    if (!k->isValid())
      return {CompileTask(Task::Error, QCoreApplication::translate("ProjectExplorer", "Kit is not valid."))};
    if (childGenerator)
      return childGenerator(k);
    return {};
  };
}

using namespace Internal;

TargetSetupPage::TargetSetupPage(QWidget *parent) : WizardPage(parent), m_tasksGenerator(defaultTasksGenerator({})), m_ui(new TargetSetupPageUi), m_importWidget(new ImportWidget(this)), m_spacer(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding))
{
  m_importWidget->setVisible(false);

  setObjectName(QLatin1String("TargetSetupPage"));
  setWindowTitle(tr("Select Kits for Your Project"));
  m_ui->setupUi(this);

  QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  policy.setHorizontalStretch(0);
  policy.setVerticalStretch(0);
  policy.setHeightForWidth(sizePolicy().hasHeightForWidth());
  setSizePolicy(policy);

  const auto centralWidget = new QWidget(this);
  m_ui->scrollArea->setWidget(centralWidget);
  centralWidget->setLayout(new QVBoxLayout);
  m_ui->centralWidget->setLayout(new QVBoxLayout);
  m_ui->centralWidget->layout()->setContentsMargins(0, 0, 0, 0);

  setTitle(tr("Kit Selection"));

  for (const auto pk : qAsConst(g_potentialKits))
    if (pk->isEnabled())
      m_potentialWidgets.append(pk->createWidget(this));

  setUseScrollArea(true);

  const auto km = KitManager::instance();
  // do note that those slots are triggered once *per* targetsetuppage
  // thus the same slot can be triggered multiple times on different instances!
  connect(km, &KitManager::kitAdded, this, &TargetSetupPage::handleKitAddition);
  connect(km, &KitManager::kitRemoved, this, &TargetSetupPage::handleKitRemoval);
  connect(km, &KitManager::kitUpdated, this, &TargetSetupPage::handleKitUpdate);
  connect(m_importWidget, &ImportWidget::importFrom, this, [this](const FilePath &dir) { import(dir); });
  connect(KitManager::instance(), &KitManager::kitsChanged, this, &TargetSetupPage::updateVisibility);

  setProperty(SHORT_TITLE_PROPERTY, tr("Kits"));
}

auto TargetSetupPage::initializePage() -> void
{
  if (KitManager::isLoaded()) {
    doInitializePage();
  } else {
    connect(KitManager::instance(), &KitManager::kitsLoaded, this, &TargetSetupPage::doInitializePage);
  }
}

auto TargetSetupPage::setTasksGenerator(const TasksGenerator &tasksGenerator) -> void
{
  m_tasksGenerator = defaultTasksGenerator(tasksGenerator);
}

auto TargetSetupPage::selectedKits() const -> QList<Id>
{
  QList<Id> result;
  for (const auto w : m_widgets) {
    if (w->isKitSelected())
      result.append(w->kit()->id());
  }
  return result;
}

TargetSetupPage::~TargetSetupPage()
{
  disconnect();
  reset();
  delete m_spacer;
  delete m_ui;
}

auto TargetSetupPage::isComplete() const -> bool
{
  return anyOf(m_widgets, [](const TargetSetupWidget *w) {
    return w->isKitSelected();
  });
}

auto TargetSetupPage::setupWidgets(const QString &filterText) -> void
{
  const auto kitList = KitManager::sortKits(KitManager::kits());
  for (const auto k : kitList) {
    if (!filterText.isEmpty() && !k->displayName().contains(filterText, Qt::CaseInsensitive))
      continue;
    const auto widget = new TargetSetupWidget(k, m_projectPath);
    connect(widget, &TargetSetupWidget::selectedToggled, this, &TargetSetupPage::kitSelectionChanged);
    connect(widget, &TargetSetupWidget::selectedToggled, this, &QWizardPage::completeChanged);
    updateWidget(widget);
    m_widgets.push_back(widget);
    m_baseLayout->addWidget(widget);
  }
  addAdditionalWidgets();

  // Setup import widget:
  m_importWidget->setCurrentDirectory(importDirectory(m_projectPath));

  kitSelectionChanged();
  updateVisibility();
}

auto TargetSetupPage::reset() -> void
{
  removeAdditionalWidgets();
  while (m_widgets.size() > 0) {
    const auto w = m_widgets.back();

    const auto k = w->kit();
    if (k && m_importer)
      m_importer->removeProject(k);

    removeWidget(w);
  }

  m_ui->allKitsCheckBox->setChecked(false);
}

auto TargetSetupPage::widget(const Id kitId, TargetSetupWidget *fallback) const -> TargetSetupWidget*
{
  return findOr(m_widgets, fallback, [kitId](const TargetSetupWidget *w) {
    return w->kit() && w->kit()->id() == kitId;
  });
}

auto TargetSetupPage::setProjectPath(const FilePath &path) -> void
{
  m_projectPath = path;
  if (!m_projectPath.isEmpty()) {
    const QFileInfo fileInfo(QDir::cleanPath(path.toString()));
    auto subDirsList = fileInfo.absolutePath().split('/');
    m_ui->headerLabel->setText(tr("The following kits can be used for project <b>%1</b>:", "%1: Project name").arg(subDirsList.last()));
  }
  m_ui->headerLabel->setVisible(!m_projectPath.isEmpty());

  if (m_widgetsWereSetUp)
    initializePage();
}

auto TargetSetupPage::setProjectImporter(ProjectImporter *importer) -> void
{
  if (importer == m_importer)
    return;

  if (m_widgetsWereSetUp)
    reset(); // Reset before changing the importer!

  m_importer = importer;
  m_importWidget->setVisible(m_importer);

  if (m_widgetsWereSetUp)
    initializePage();
}

auto TargetSetupPage::importLineEditHasFocus() const -> bool
{
  return m_importWidget->ownsReturnKey();
}

auto TargetSetupPage::setupImports() -> void
{
  if (!m_importer || m_projectPath.isEmpty())
    return;

  const auto toImport = m_importer->importCandidates();
  for (const auto &path : toImport)
    import(FilePath::fromString(path), true);
}

auto TargetSetupPage::handleKitAddition(Kit *k) -> void
{
  if (isUpdating())
    return;

  Q_ASSERT(!widget(k));
  addWidget(k);
  kitSelectionChanged();
  updateVisibility();
}

auto TargetSetupPage::handleKitRemoval(Kit *k) -> void
{
  if (isUpdating())
    return;

  if (m_importer)
    m_importer->cleanupKit(k);

  removeWidget(k);
  kitSelectionChanged();
  updateVisibility();
}

auto TargetSetupPage::handleKitUpdate(Kit *k) -> void
{
  if (isUpdating())
    return;

  if (m_importer)
    m_importer->makePersistent(k);

  const auto newWidgetList = sortedWidgetList();
  if (newWidgetList != m_widgets) {
    // Sorting has changed.
    m_widgets = newWidgetList;
    reLayout();
  }
  updateWidget(widget(k));
  kitSelectionChanged();
  updateVisibility();
}

auto TargetSetupPage::selectAtLeastOneEnabledKit() -> void
{
  if (anyOf(m_widgets, [](const TargetSetupWidget *w) { return w->isKitSelected(); })) {
    // Something is already selected, we are done.
    return;
  }

  TargetSetupWidget *toCheckWidget = nullptr;

  const Kit *defaultKit = KitManager::defaultKit();

  auto isPreferred = [this](const TargetSetupWidget *w) {
    const auto tasks = m_tasksGenerator(w->kit());
    return w->isEnabled() && tasks.isEmpty();
  };

  // Use default kit if that is preferred:
  toCheckWidget = findOrDefault(m_widgets, [defaultKit, isPreferred](const TargetSetupWidget *w) {
    return isPreferred(w) && w->kit() == defaultKit;
  });

  if (!toCheckWidget) {
    // Use the first preferred widget:
    toCheckWidget = findOrDefault(m_widgets, isPreferred);
  }

  if (!toCheckWidget) {
    // Use default kit if it is enabled:
    toCheckWidget = findOrDefault(m_widgets, [defaultKit](const TargetSetupWidget *w) {
      return w->isEnabled() && w->kit() == defaultKit;
    });
  }

  if (!toCheckWidget) {
    // Use the first enabled widget:
    toCheckWidget = findOrDefault(m_widgets, [](const TargetSetupWidget *w) { return w->isEnabled(); });
  }

  if (toCheckWidget) {
    toCheckWidget->setKitSelected(true);

    emit completeChanged(); // Is this necessary?
  }
}

auto TargetSetupPage::updateVisibility() -> void
{
  // Always show the widgets, the import widget always makes sense to show.
  m_ui->scrollAreaWidget->setVisible(m_baseLayout == m_ui->scrollArea->widget()->layout());
  m_ui->centralWidget->setVisible(m_baseLayout == m_ui->centralWidget->layout());

  const bool hasUsableKits = KitManager::kit([this](const Kit *k) { return isUsable(k); });
  m_ui->noValidKitLabel->setVisible(!hasUsableKits);
  m_ui->allKitsCheckBox->setVisible(hasUsableKits);

  emit completeChanged();
}

auto TargetSetupPage::reLayout() -> void
{
  removeAdditionalWidgets();
  for (const auto w : qAsConst(m_widgets))
    m_baseLayout->removeWidget(w);
  for (const auto w : qAsConst(m_widgets))
    m_baseLayout->addWidget(w);
  addAdditionalWidgets();
}

auto TargetSetupPage::compareKits(const Kit *k1, const Kit *k2) -> bool
{
  const auto name1 = k1->displayName();
  const auto name2 = k2->displayName();
  if (name1 < name2)
    return true;
  if (name1 > name2)
    return false;
  return k1 < k2;
}

auto TargetSetupPage::sortedWidgetList() const -> std::vector<TargetSetupWidget*>
{
  auto list = m_widgets;
  sort(list, [](const TargetSetupWidget *w1, const TargetSetupWidget *w2) {
    return compareKits(w1->kit(), w2->kit());
  });
  return list;
}

auto TargetSetupPage::openOptions() -> void
{
  Orca::Plugin::Core::ICore::showOptionsDialog(Constants::KITS_SETTINGS_PAGE_ID, this);
}

auto TargetSetupPage::kitSelectionChanged() -> void
{
  auto selected = 0;
  auto deselected = 0;
  for (const TargetSetupWidget *widget : m_widgets) {
    if (widget->isKitSelected())
      ++selected;
    else
      ++deselected;
  }
  if (selected > 0 && deselected > 0)
    m_ui->allKitsCheckBox->setCheckState(Qt::PartiallyChecked);
  else if (selected > 0 && deselected == 0)
    m_ui->allKitsCheckBox->setCheckState(Qt::Checked);
  else
    m_ui->allKitsCheckBox->setCheckState(Qt::Unchecked);
}

auto TargetSetupPage::kitFilterChanged(const QString &filterText) -> void
{
  const QPointer<QWidget> focusWidget = QApplication::focusWidget();
  // Remember selected kits:
  const auto selectedWidgets = filtered(m_widgets, &TargetSetupWidget::isKitSelected);
  const auto selectedKitIds = transform<QVector>(selectedWidgets, [](const TargetSetupWidget *w) {
    return w->kit()->id();
  });

  // Reset currently shown kits
  reset();
  setupWidgets(filterText);

  // Re-select kits:
  for (const auto w : qAsConst(m_widgets))
    w->setKitSelected(selectedKitIds.contains(w->kit()->id()));

  emit completeChanged();

  if (focusWidget)
    focusWidget->setFocus();
}

auto TargetSetupPage::doInitializePage() -> void
{
  reset();
  setupWidgets();
  setupImports();

  selectAtLeastOneEnabledKit();

  updateVisibility();
}

auto TargetSetupPage::showEvent(QShowEvent *event) -> void
{
  WizardPage::showEvent(event);
  setFocus(); // Ensure "Configure Project" gets triggered on <Return>
}

auto TargetSetupPage::changeAllKitsSelections() -> void
{
  if (m_ui->allKitsCheckBox->checkState() == Qt::PartiallyChecked)
    m_ui->allKitsCheckBox->setCheckState(Qt::Checked);
  const auto checked = m_ui->allKitsCheckBox->isChecked();
  for (const auto widget : m_widgets)
    widget->setKitSelected(checked);
  emit completeChanged();
}

auto TargetSetupPage::isUpdating() const -> bool
{
  return m_importer && m_importer->isUpdating();
}

auto TargetSetupPage::import(const FilePath &path, bool silent) -> void
{
  if (!m_importer)
    return;

  for (const auto &info : m_importer->import(path, silent)) {
    auto w = widget(info.kitId);
    if (!w) {
      const auto k = KitManager::kit(info.kitId);
      Q_ASSERT(k);
      addWidget(k);
    }
    w = widget(info.kitId);
    if (!w)
      continue;

    w->addBuildInfo(info, true);
    w->setKitSelected(true);
    w->expandWidget();
    kitSelectionChanged();
  }
  emit completeChanged();
}

auto TargetSetupPage::removeWidget(TargetSetupWidget *w) -> void
{
  if (!w)
    return;
  w->deleteLater();
  w->clearKit();
  m_widgets.erase(std::find(m_widgets.begin(), m_widgets.end(), w));
}

auto TargetSetupPage::addWidget(Kit *k) -> TargetSetupWidget*
{
  const auto widget = new TargetSetupWidget(k, m_projectPath);
  updateWidget(widget);
  connect(widget, &TargetSetupWidget::selectedToggled, this, &TargetSetupPage::kitSelectionChanged);
  connect(widget, &TargetSetupWidget::selectedToggled, this, &QWizardPage::completeChanged);

  // Insert widget, sorted.
  const auto insertionPos = std::find_if(m_widgets.begin(), m_widgets.end(), [k](const TargetSetupWidget *w) {
    return compareKits(k, w->kit());
  });
  const auto addedToEnd = insertionPos == m_widgets.end();
  m_widgets.insert(insertionPos, widget);
  if (addedToEnd) {
    removeAdditionalWidgets();
    m_baseLayout->addWidget(widget);
    addAdditionalWidgets();
  } else {
    reLayout();
  }

  return widget;
}

auto TargetSetupPage::addAdditionalWidgets() -> void
{
  m_baseLayout->addWidget(m_importWidget);
  for (const auto widget : qAsConst(m_potentialWidgets))
    m_baseLayout->addWidget(widget);
  m_baseLayout->addItem(m_spacer);
}

auto TargetSetupPage::removeAdditionalWidgets(QLayout *layout) -> void
{
  layout->removeWidget(m_importWidget);
  for (const auto potentialWidget : qAsConst(m_potentialWidgets))
    layout->removeWidget(potentialWidget);
  layout->removeItem(m_spacer);
}

auto TargetSetupPage::updateWidget(TargetSetupWidget *widget) -> void
{
  QTC_ASSERT(widget, return);
  widget->update(m_tasksGenerator);
}

auto TargetSetupPage::isUsable(const Kit *kit) const -> bool
{
  return !containsType(m_tasksGenerator(kit), Task::Error);
}

auto TargetSetupPage::setupProject(Project *project) -> bool
{
  QList<BuildInfo> toSetUp;
  for (const auto widget : m_widgets) {
    if (!widget->isKitSelected())
      continue;

    const auto k = widget->kit();

    if (k && m_importer)
      m_importer->makePersistent(k);
    toSetUp << widget->selectedBuildInfoList();
    widget->clearKit();
  }

  project->setup(toSetUp);
  toSetUp.clear();

  // Only reset now that toSetUp has been cleared!
  reset();

  Target *activeTarget = nullptr;
  if (m_importer)
    activeTarget = m_importer->preferredTarget(project->targets());
  if (activeTarget)
    SessionManager::setActiveTarget(project, activeTarget, SetActive::NoCascade);

  return true;
}

auto TargetSetupPage::setUseScrollArea(bool b) -> void
{
  const auto oldBaseLayout = m_baseLayout;
  m_baseLayout = b ? m_ui->scrollArea->widget()->layout() : m_ui->centralWidget->layout();
  if (oldBaseLayout == m_baseLayout)
    return;
  m_ui->scrollAreaWidget->setVisible(b);
  m_ui->centralWidget->setVisible(!b);

  if (oldBaseLayout)
    removeAdditionalWidgets(oldBaseLayout);
  addAdditionalWidgets();
}

} // namespace ProjectExplorer
