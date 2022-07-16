// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "targetsettingspanel.hpp"

#include "buildconfiguration.hpp"
#include "buildmanager.hpp"
#include "buildsettingspropertiespage.hpp"
#include "ipotentialkit.hpp"
#include "kit.hpp"
#include "kitmanager.hpp"
#include "panelswidget.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorericons.hpp"
#include "projectwindow.hpp"
#include "runsettingspropertiespage.hpp"
#include "session.hpp"
#include "target.hpp"
#include "targetsetuppage.hpp"
#include "task.hpp"

#include <app/app_version.hpp>

#include <core/core-interface.hpp>
#include <core/core-constants.hpp>
#include <core/core-mode-manager.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/treemodel.hpp>
#include <utils/utilsicons.hpp>

#include <QCoreApplication>
#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include <cmath>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class TargetSetupPageWrapper : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelWidget)

public:
  explicit TargetSetupPageWrapper(Project *project);

  auto ensureSetupPage() -> void
  {
    if (!m_targetSetupPage)
      addTargetSetupPage();
  }

protected:
  auto keyReleaseEvent(QKeyEvent *event) -> void override
  {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      event->accept();
  }

  auto keyPressEvent(QKeyEvent *event) -> void override
  {
    if ((m_targetSetupPage && m_targetSetupPage->importLineEditHasFocus()) || (m_configureButton && !m_configureButton->isEnabled())) {
      return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      event->accept();
      if (m_targetSetupPage)
        done();
    }
  }

private:
  auto done() -> void
  {
    QTC_ASSERT(m_targetSetupPage, return);
    m_targetSetupPage->disconnect();
    m_targetSetupPage->setupProject(m_project);
    m_targetSetupPage->deleteLater();
    m_targetSetupPage = nullptr;
    ModeManager::activateMode(Orca::Plugin::Core::MODE_EDIT);
  }

  auto completeChanged() -> void
  {
    m_configureButton->setEnabled(m_targetSetupPage && m_targetSetupPage->isComplete());
  }

  auto addTargetSetupPage() -> void;

  Project *const m_project;
  TargetSetupPage *m_targetSetupPage = nullptr;
  QPushButton *m_configureButton = nullptr;
  QVBoxLayout *m_setupPageContainer = nullptr;
};

TargetSetupPageWrapper::TargetSetupPageWrapper(Project *project) : m_project(project)
{
  const auto box = new QDialogButtonBox(this);

  m_configureButton = new QPushButton(this);
  m_configureButton->setText(tr("&Configure Project"));
  box->addButton(m_configureButton, QDialogButtonBox::AcceptRole);

  const auto hbox = new QHBoxLayout;
  hbox->addStretch();
  hbox->addWidget(box);

  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_setupPageContainer = new QVBoxLayout;
  layout->addLayout(m_setupPageContainer);
  layout->addLayout(hbox);
  layout->addStretch(10);
  completeChanged();
  connect(m_configureButton, &QAbstractButton::clicked, this, &TargetSetupPageWrapper::done);
}

auto TargetSetupPageWrapper::addTargetSetupPage() -> void
{
  m_targetSetupPage = new TargetSetupPage(this);
  m_targetSetupPage->setUseScrollArea(false);
  m_targetSetupPage->setProjectPath(m_project->projectFilePath());
  m_targetSetupPage->setTasksGenerator([this](const Kit *k) { return m_project->projectIssues(k); });
  m_targetSetupPage->setProjectImporter(m_project->projectImporter());
  m_targetSetupPage->initializePage();
  m_targetSetupPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_setupPageContainer->addWidget(m_targetSetupPage);

  completeChanged();

  connect(m_targetSetupPage, &QWizardPage::completeChanged, this, &TargetSetupPageWrapper::completeChanged);
}

//
// TargetSettingsPanelItem
//

class TargetGroupItemPrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelItem)

public:
  TargetGroupItemPrivate(TargetGroupItem *q, Project *project);
  ~TargetGroupItemPrivate() override;

  auto handleRemovedKit(Kit *kit) -> void;
  auto handleAddedKit(Kit *kit) -> void;
  auto handleUpdatedKit(Kit *kit) -> void;
  auto handleTargetAdded(Target *target) -> void;
  auto handleTargetRemoved(Target *target) -> void;
  auto handleTargetChanged(Target *target) -> void;
  auto ensureWidget() -> void;
  auto rebuildContents() -> void;

  TargetGroupItem *q;
  QString m_displayName;
  Project *m_project;
  QPointer<QWidget> m_noKitLabel;
  QPointer<QWidget> m_configurePage;
  QPointer<QWidget> m_configuredPage;
  TargetSetupPageWrapper *m_targetSetupPageWrapper = nullptr;
};

auto TargetGroupItemPrivate::ensureWidget() -> void
{
  if (!m_noKitLabel) {
    m_noKitLabel = new QWidget;
    m_noKitLabel->setFocusPolicy(Qt::NoFocus);

    const auto label = new QLabel;
    label->setText(tr("No kit defined in this project."));
    auto f = label->font();
    f.setPointSizeF(f.pointSizeF() * 1.4);
    f.setBold(true);
    label->setFont(f);
    label->setContentsMargins(10, 10, 10, 10);
    label->setAlignment(Qt::AlignTop);

    const auto layout = new QVBoxLayout(m_noKitLabel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(label);
    layout->addStretch(10);
  }

  if (!m_configurePage) {
    m_targetSetupPageWrapper = new TargetSetupPageWrapper(m_project);
    m_configurePage = new PanelsWidget(tr("Configure Project"), m_targetSetupPageWrapper);
    m_configurePage->setFocusProxy(m_targetSetupPageWrapper);
  }
  m_targetSetupPageWrapper->ensureSetupPage();

  if (!m_configuredPage) {
    const auto widget = new QWidget;
    const auto label = new QLabel("This project is already configured.");
    const auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    layout->addStretch(10);
    m_configuredPage = new PanelsWidget(tr("Configure Project"), widget);
  }
}

//
// Third level: The per-kit entries (inactive or with a 'Build' and a 'Run' subitem)
//
class TargetItem : public TypedTreeItem<TreeItem, TargetGroupItem> {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelWidget)
public:
  enum { DefaultPage = 0 }; // Build page.

  TargetItem(Project *project, Id kitId, const Tasks &issues) : m_project(project), m_kitId(kitId), m_kitIssues(issues)
  {
    m_kitWarningForProject = containsType(m_kitIssues, Task::TaskType::Warning);
    m_kitErrorsForProject = containsType(m_kitIssues, Task::TaskType::Error);

    updateSubItems();
  }

  auto target() const -> Target*
  {
    return m_project->target(m_kitId);
  }

  auto updateSubItems() -> void;

  auto flags(int column) const -> Qt::ItemFlags override
  {
    Q_UNUSED(column)
    return m_kitErrorsForProject ? Qt::ItemFlags({}) : Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  }

  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DisplayRole: {
      if (const auto kit = KitManager::kit(m_kitId))
        return kit->displayName();
      break;
    }

    case Qt::DecorationRole: {
      const Kit *k = KitManager::kit(m_kitId);
      QTC_ASSERT(k, return QVariant());
      if (m_kitErrorsForProject)
        return kitIconWithOverlay(*k, IconOverlay::Error);
      if (!isEnabled())
        return kitIconWithOverlay(*k, IconOverlay::Add);
      if (m_kitWarningForProject)
        return kitIconWithOverlay(*k, IconOverlay::Warning);
      return k->icon();
    }

    case Qt::ForegroundRole: {
      if (!isEnabled())
        return Utils::orcaTheme()->color(Theme::TextColorDisabled);
      break;
    }

    case Qt::FontRole: {
      auto font = parent()->data(column, role).value<QFont>();
      if (const auto targetItem = parent()->currentTargetItem()) {
        const auto t = targetItem->target();
        if (t && t->id() == m_kitId && m_project == SessionManager::startupProject())
          font.setBold(true);
      }
      return font;
    }

    case Qt::ToolTipRole: {
      const auto k = KitManager::kit(m_kitId);
      QTC_ASSERT(k, return QVariant());
      const auto extraText = [this]() {
        if (m_kitErrorsForProject)
          return QString("<h3>" + tr("Kit is unsuited for project") + "</h3>");
        if (!isEnabled())
          return QString("<h3>" + tr("Click to activate") + "</h3>");
        return QString();
      }();
      return k->toHtml(m_kitIssues, extraText);
    }

    case PanelWidgetRole:
    case ActiveItemRole: {
      if (m_currentChild >= 0 && m_currentChild < childCount())
        return childAt(m_currentChild)->data(column, role);
      break;
    }

    default:
      break;
    }

    return QVariant();
  }

  auto setData(int column, const QVariant &data, int role) -> bool override
  {
    Q_UNUSED(column)

    if (role == ContextMenuItemAdderRole) {
      auto *menu = data.value<QMenu*>();
      addToContextMenu(menu, flags(column) & Qt::ItemIsSelectable);
      return true;
    }

    if (role == ItemActivatedDirectlyRole) {
      QTC_ASSERT(!data.isValid(), return false);
      if (!isEnabled()) {
        m_currentChild = DefaultPage;
        m_project->addTargetForKit(KitManager::kit(m_kitId));
      } else {
        // Go to Run page, when on Run previously etc.
        const auto previousItem = parent()->currentTargetItem();
        m_currentChild = previousItem ? previousItem->m_currentChild : DefaultPage;
        SessionManager::setActiveTarget(m_project, target(), SetActive::Cascade);
        parent()->setData(column, QVariant::fromValue(static_cast<TreeItem*>(this)), ItemActivatedFromBelowRole);
      }
      return true;
    }

    if (role == ItemActivatedFromBelowRole) {
      // I.e. 'Build' and 'Run' items were present and user clicked on them.
      const auto child = indexOf(data.value<TreeItem*>());
      QTC_ASSERT(child != -1, return false);
      m_currentChild = child; // Triggered from sub-item.
      SessionManager::setActiveTarget(m_project, target(), SetActive::Cascade);
      // Propagate Build/Run selection up.
      parent()->setData(column, QVariant::fromValue(static_cast<TreeItem*>(this)), ItemActivatedFromBelowRole);
      return true;
    }

    if (role == ItemActivatedFromAboveRole) {
      // Usually programmatic activation, e.g. after opening the Project mode.
      SessionManager::setActiveTarget(m_project, target(), SetActive::Cascade);
      return true;
    }
    return false;
  }

  auto addToContextMenu(QMenu *menu, bool isSelectable) -> void
  {
    auto kit = KitManager::kit(m_kitId);
    QTC_ASSERT(kit, return);
    const auto projectName = m_project->displayName();

    const auto enableAction = menu->addAction(tr("Enable Kit for Project \"%1\"").arg(projectName));
    enableAction->setEnabled(isSelectable && m_kitId.isValid() && !isEnabled());
    QObject::connect(enableAction, &QAction::triggered, [this, kit] {
      m_project->addTargetForKit(kit);
    });

    const auto enableForAllAction = menu->addAction(tr("Enable Kit for All Projects"));
    enableForAllAction->setEnabled(isSelectable);
    QObject::connect(enableForAllAction, &QAction::triggered, [kit] {
      for (const auto p : SessionManager::projects()) {
        if (!p->target(kit))
          p->addTargetForKit(kit);
      }
    });

    const auto disableAction = menu->addAction(tr("Disable Kit for Project \"%1\"").arg(projectName));
    disableAction->setEnabled(isSelectable && m_kitId.isValid() && isEnabled());
    QObject::connect(disableAction, &QAction::triggered, m_project, [this] {
      const auto t = target();
      QTC_ASSERT(t, return);
      const auto kitName = t->displayName();
      if (BuildManager::isBuilding(t)) {
        QMessageBox box;
        const auto closeAnyway = box.addButton(tr("Cancel Build and Disable Kit in This Project"), QMessageBox::AcceptRole);
        const auto cancelClose = box.addButton(tr("Do Not Remove"), QMessageBox::RejectRole);
        box.setDefaultButton(cancelClose);
        box.setWindowTitle(tr("Disable Kit \"%1\" in This Project?").arg(kitName));
        box.setText(tr("The kit <b>%1</b> is currently being built.").arg(kitName));
        box.setInformativeText(tr("Do you want to cancel the build process and remove the kit anyway?"));
        box.exec();
        if (box.clickedButton() != closeAnyway)
          return;
        BuildManager::cancel();
      }

      QCoreApplication::processEvents();

      m_project->removeTarget(t);
    });

    const auto disableForAllAction = menu->addAction(tr("Disable Kit for All Projects"));
    disableForAllAction->setEnabled(isSelectable);
    QObject::connect(disableForAllAction, &QAction::triggered, [kit] {
      for (const auto p : SessionManager::projects()) {
        const auto t = p->target(kit);
        if (!t)
          continue;
        if (BuildManager::isBuilding(t))
          BuildManager::cancel();
        p->removeTarget(t);
      }
    });

    const auto copyMenu = menu->addMenu(tr("Copy Steps From Another Kit..."));
    if (m_kitId.isValid() && m_project->target(m_kitId)) {
      const auto kits = KitManager::kits();
      for (auto kit : kits) {
        const auto copyAction = copyMenu->addAction(kit->displayName());
        if (kit->id() == m_kitId || !m_project->target(kit->id())) {
          copyAction->setEnabled(false);
        } else {
          QObject::connect(copyAction, &QAction::triggered, [this, kit] {
            const auto thisTarget = m_project->target(m_kitId);
            const auto sourceTarget = m_project->target(kit->id());
            Project::copySteps(sourceTarget, thisTarget);
          });
        }
      }
    } else {
      copyMenu->setEnabled(false);
    }
  }

  auto isEnabled() const -> bool { return target() != nullptr; }

public:
  QPointer<Project> m_project; // Not owned.
  Id m_kitId;
  int m_currentChild = DefaultPage;
  bool m_kitErrorsForProject = false;
  bool m_kitWarningForProject = false;
  Tasks m_kitIssues;

private:
  enum class IconOverlay {
    Add,
    Warning,
    Error
  };

  static auto kitIconWithOverlay(const Kit &kit, IconOverlay overlayType) -> QIcon
  {
    QIcon overlayIcon;
    switch (overlayType) {
    case IconOverlay::Add: {
      static const auto add = Utils::Icons::OVERLAY_ADD.icon();
      overlayIcon = add;
      break;
    }
    case IconOverlay::Warning: {
      static const auto warning = Utils::Icons::OVERLAY_WARNING.icon();
      overlayIcon = warning;
      break;
    }
    case IconOverlay::Error: {
      static const auto err = Utils::Icons::OVERLAY_ERROR.icon();
      overlayIcon = err;
      break;
    }
    }
    const QSize iconSize(16, 16);
    const QRect iconRect(QPoint(), iconSize);
    QPixmap result(iconSize * qApp->devicePixelRatio());
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(qApp->devicePixelRatio());
    QPainter p(&result);
    kit.icon().paint(&p, iconRect, Qt::AlignCenter, overlayType == IconOverlay::Add ? QIcon::Disabled : QIcon::Normal);
    overlayIcon.paint(&p, iconRect);
    return result;
  }
};

//
// Fourth level: The 'Build' and 'Run' sub-items.
//

class BuildOrRunItem : public TreeItem {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelWidget)
public:
  enum SubIndex {
    BuildPage = 0,
    RunPage = 1
  };

  BuildOrRunItem(Project *project, Id kitId, SubIndex subIndex) : m_project(project), m_kitId(kitId), m_subIndex(subIndex) { }

  ~BuildOrRunItem() override
  {
    delete m_panel;
  }

  auto target() const -> Target*
  {
    return m_project->target(m_kitId);
  }

  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DisplayRole: {
      switch (m_subIndex) {
      case BuildPage:
        return tr("Build");
      case RunPage:
        return tr("Run");
      }
      break;
    }

    case Qt::ToolTipRole:
      return parent()->data(column, role);

    case PanelWidgetRole:
      return QVariant::fromValue(panel());

    case ActiveItemRole:
      return QVariant::fromValue<TreeItem*>(const_cast<BuildOrRunItem*>(this));

    case KitIdRole:
      return m_kitId.toSetting();

    case Qt::DecorationRole: {
      switch (m_subIndex) {
      case BuildPage: {
        static const auto buildIcon = Icons::BUILD_SMALL.icon();
        return buildIcon;
      }
      case RunPage: {
        static const auto runIcon = Utils::Icons::RUN_SMALL.icon();
        return runIcon;
      }
      }
      break;
    }

    default:
      break;
    }

    return QVariant();
  }

  auto flags(int column) const -> Qt::ItemFlags override
  {
    return parent()->flags(column);
  }

  auto setData(int column, const QVariant &data, int role) -> bool override
  {
    if (role == ItemActivatedDirectlyRole) {
      parent()->setData(column, QVariant::fromValue(static_cast<TreeItem*>(this)), ItemActivatedFromBelowRole);
      return true;
    }

    return parent()->setData(column, data, role);
  }

  auto panel() const -> QWidget*
  {
    if (!m_panel) {
      m_panel = (m_subIndex == RunPage) ? new PanelsWidget(RunSettingsWidget::tr("Run Settings"), new RunSettingsWidget(target())) : new PanelsWidget(QCoreApplication::translate("BuildSettingsPanel", "Build Settings"), new BuildSettingsWidget(target()));
    }
    return m_panel;
  }

public:
  Project *m_project; // Not owned.
  Id m_kitId;
  mutable QPointer<QWidget> m_panel; // Owned.
  const SubIndex m_subIndex;
};

//
// Also third level:
//
class PotentialKitItem : public TypedTreeItem<TreeItem, TargetGroupItem> {
  Q_DECLARE_TR_FUNCTIONS(TargetSettingsPanelWidget)
public:
  PotentialKitItem(Project *project, IPotentialKit *potentialKit) : m_project(project), m_potentialKit(potentialKit) {}

  auto data(int column, int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole)
      return m_potentialKit->displayName();

    if (role == Qt::FontRole) {
      auto font = parent()->data(column, role).value<QFont>();
      font.setItalic(true);
      return font;
    }

    return QVariant();
  }

  auto setData(int column, const QVariant &data, int role) -> bool override
  {
    Q_UNUSED(column)
    if (role == ContextMenuItemAdderRole) {
      auto *menu = data.value<QMenu*>();
      const auto enableAction = menu->addAction(tr("Enable Kit"));
      enableAction->setEnabled(!isEnabled());
      QObject::connect(enableAction, &QAction::triggered, [this] {
        m_potentialKit->executeFromMenu();
      });
      return true;
    }

    return false;
  }

  auto flags(int column) const -> Qt::ItemFlags override
  {
    Q_UNUSED(column)
    if (isEnabled())
      return Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return Qt::ItemIsSelectable;
  }

  auto isEnabled() const -> bool { return m_potentialKit->isEnabled(); }

  Project *m_project;
  IPotentialKit *m_potentialKit;
};

TargetGroupItem::TargetGroupItem(const QString &displayName, Project *project) : d(std::make_unique<TargetGroupItemPrivate>(this, project))
{
  d->m_displayName = displayName;
  QObject::connect(project, &Project::addedTarget, d.get(), &TargetGroupItemPrivate::handleTargetAdded);
  QObject::connect(project, &Project::removedTarget, d.get(), &TargetGroupItemPrivate::handleTargetRemoved);
  QObject::connect(project, &Project::activeTargetChanged, d.get(), &TargetGroupItemPrivate::handleTargetChanged);
}

TargetGroupItem::~TargetGroupItem() = default;

TargetGroupItemPrivate::TargetGroupItemPrivate(TargetGroupItem *q, Project *project) : q(q), m_project(project)
{
  // force a signal since the index has changed
  connect(KitManager::instance(), &KitManager::kitAdded, this, &TargetGroupItemPrivate::handleAddedKit);
  connect(KitManager::instance(), &KitManager::kitRemoved, this, &TargetGroupItemPrivate::handleRemovedKit);
  connect(KitManager::instance(), &KitManager::kitUpdated, this, &TargetGroupItemPrivate::handleUpdatedKit);

  rebuildContents();
}

TargetGroupItemPrivate::~TargetGroupItemPrivate()
{
  disconnect();

  delete m_noKitLabel;
  delete m_configurePage;
  delete m_configuredPage;
}

auto TargetGroupItem::data(int column, int role) const -> QVariant
{
  if (role == Qt::DisplayRole)
    return d->m_displayName;

  if (role == ActiveItemRole) {
    if (const auto item = currentTargetItem())
      return item->data(column, role);
    return QVariant::fromValue<TreeItem*>(const_cast<TargetGroupItem*>(this));
  }

  if (role == PanelWidgetRole) {
    if (const auto item = currentTargetItem())
      return item->data(column, role);

    d->ensureWidget();
    return QVariant::fromValue<QWidget*>(d->m_configurePage.data());
  }

  return QVariant();
}

auto TargetGroupItem::setData(int column, const QVariant &data, int role) -> bool
{
  Q_UNUSED(data)
  if (role == ItemActivatedFromBelowRole || role == ItemUpdatedFromBelowRole) {
    // Bubble up to trigger setting the active project.
    parent()->setData(column, QVariant::fromValue(static_cast<TreeItem*>(this)), role);
    return true;
  }

  return false;
}

auto TargetGroupItem::flags(int) const -> Qt::ItemFlags
{
  return Qt::NoItemFlags;
}

auto TargetGroupItem::currentTargetItem() const -> TargetItem*
{
  return targetItem(d->m_project->activeTarget());
}

auto TargetGroupItem::targetItem(Target *target) const -> TargetItem*
{
  if (target) {
    auto needle = target->id(); // Unconfigured project have no active target.
    return findFirstLevelChild([needle](TargetItem *item) { return item->m_kitId == needle; });
  }
  return nullptr;
}

auto TargetGroupItemPrivate::handleRemovedKit(Kit *kit) -> void
{
  Q_UNUSED(kit)
  rebuildContents();
}

auto TargetGroupItemPrivate::handleUpdatedKit(Kit *kit) -> void
{
  Q_UNUSED(kit)
  rebuildContents();
}

auto TargetGroupItemPrivate::handleAddedKit(Kit *kit) -> void
{
  q->appendChild(new TargetItem(m_project, kit->id(), m_project->projectIssues(kit)));
}

auto TargetItem::updateSubItems() -> void
{
  if (childCount() == 0 && isEnabled())
    m_currentChild = DefaultPage; // We will add children below.
  removeChildren();
  if (isEnabled() && !m_kitErrorsForProject) {
    if (m_project->needsBuildConfigurations())
      appendChild(new BuildOrRunItem(m_project, m_kitId, BuildOrRunItem::BuildPage));
    appendChild(new BuildOrRunItem(m_project, m_kitId, BuildOrRunItem::RunPage));
  }
}

auto TargetGroupItemPrivate::rebuildContents() -> void
{
  q->removeChildren();

  const auto kits = KitManager::sortKits(KitManager::kits());
  for (const auto kit : kits)
    q->appendChild(new TargetItem(m_project, kit->id(), m_project->projectIssues(kit)));

  if (q->parent())
    q->parent()->setData(0, QVariant::fromValue(static_cast<TreeItem*>(q)), ItemUpdatedFromBelowRole);
}

auto TargetGroupItemPrivate::handleTargetAdded(Target *target) -> void
{
  if (const auto item = q->targetItem(target))
    item->updateSubItems();
  q->update();
}

auto TargetGroupItemPrivate::handleTargetRemoved(Target *target) -> void
{
  if (const auto item = q->targetItem(target))
    item->updateSubItems();
  q->parent()->setData(0, QVariant::fromValue(static_cast<TreeItem*>(q)), ItemDeactivatedFromBelowRole);
}

auto TargetGroupItemPrivate::handleTargetChanged(Target *target) -> void
{
  if (const auto item = q->targetItem(target))
    item->updateSubItems();
  q->setData(0, QVariant(), ItemActivatedFromBelowRole);
}

} // Internal
} // ProjectExplorer
