// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectwindow.hpp"

#include "buildinfo.hpp"
#include "projectexplorerconstants.hpp"
#include "kit.hpp"
#include "kitmanager.hpp"
#include "kitoptionspage.hpp"
#include "panelswidget.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectimporter.hpp"
#include "projectpanelfactory.hpp"
#include "session.hpp"
#include "target.hpp"
#include "targetsettingspanel.hpp"

#include <core/core-action-manager.hpp>
#include <core/core-command-button.hpp>
#include <core/core-constants.hpp>
#include <core/core-icons.hpp>
#include <core/core-options-popup.hpp>
#include <core/core-find-placeholder.hpp>
#include <core/core-context-interface.hpp>
#include <core/core-interface.hpp>
#include <core/core-document-interface.hpp>
#include <core/core-output-window.hpp>

#include <utils/algorithm.hpp>
#include <utils/basetreeview.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/navigationtreeview.hpp>
#include <utils/qtcassert.hpp>
#include <utils/styledbar.hpp>
#include <utils/treemodel.hpp>
#include <utils/utilsicons.hpp>

#include <texteditor/fontsettings.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class MiscSettingsGroupItem;

constexpr char kBuildSystemOutputContext[] = "ProjectsMode.BuildSystemOutput";
constexpr char kRegExpActionId[] = "OutputFilter.RegularExpressions.BuildSystemOutput";
constexpr char kCaseSensitiveActionId[] = "OutputFilter.CaseSensitive.BuildSystemOutput";
constexpr char kInvertActionId[] = "OutputFilter.Invert.BuildSystemOutput";

class BuildSystemOutputWindow : public OutputWindow {
public:
  BuildSystemOutputWindow();

  auto toolBar() -> QWidget*;

private:
  auto updateFilter() -> void;

  QPointer<QWidget> m_toolBar;
  QPointer<FancyLineEdit> m_filterOutputLineEdit;
  QAction *m_clear;
  QAction *m_filterActionRegexp;
  QAction *m_filterActionCaseSensitive;
  QAction *m_invertFilterAction;
  QAction *m_zoomIn;
  QAction *m_zoomOut;
};

BuildSystemOutputWindow::BuildSystemOutputWindow() : OutputWindow(Context(kBuildSystemOutputContext), "ProjectsMode.BuildSystemOutput.Zoom"), m_clear(new QAction)
{
  setReadOnly(true);

  const auto clearCommand = ActionManager::command(Orca::Plugin::Core::OUTPUTPANE_CLEAR);
  m_clear->setIcon(Utils::Icons::CLEAN_TOOLBAR.icon());
  m_clear->setText(clearCommand->action()->text());
  ActionManager::registerAction(m_clear, Orca::Plugin::Core::OUTPUTPANE_CLEAR, Context(kBuildSystemOutputContext));
  connect(m_clear, &QAction::triggered, this, [this] { clear(); });

  m_filterActionRegexp = new QAction(this);
  m_filterActionRegexp->setCheckable(true);
  m_filterActionRegexp->setText(ProjectWindow::tr("Use Regular Expressions"));
  connect(m_filterActionRegexp, &QAction::toggled, this, &BuildSystemOutputWindow::updateFilter);
  ActionManager::registerAction(m_filterActionRegexp, kRegExpActionId, Context(Constants::C_PROJECTEXPLORER));

  m_filterActionCaseSensitive = new QAction(this);
  m_filterActionCaseSensitive->setCheckable(true);
  m_filterActionCaseSensitive->setText(ProjectWindow::tr("Case Sensitive"));
  connect(m_filterActionCaseSensitive, &QAction::toggled, this, &BuildSystemOutputWindow::updateFilter);
  ActionManager::registerAction(m_filterActionCaseSensitive, kCaseSensitiveActionId, Context(Constants::C_PROJECTEXPLORER));

  m_invertFilterAction = new QAction(this);
  m_invertFilterAction->setCheckable(true);
  m_invertFilterAction->setText(ProjectWindow::tr("Show Non-matching Lines"));
  connect(m_invertFilterAction, &QAction::toggled, this, &BuildSystemOutputWindow::updateFilter);
  ActionManager::registerAction(m_invertFilterAction, kInvertActionId, Context(Constants::C_PROJECTEXPLORER));

  connect(TextEditor::TextEditorSettings::instance(), &TextEditor::TextEditorSettings::fontSettingsChanged, this, [this] { setBaseFont(TextEditor::TextEditorSettings::fontSettings().font()); });
  setBaseFont(TextEditor::TextEditorSettings::fontSettings().font());

  m_zoomIn = new QAction;
  m_zoomIn->setIcon(Utils::Icons::PLUS_TOOLBAR.icon());
  connect(m_zoomIn, &QAction::triggered, this, [this] { zoomIn(); });
  ActionManager::registerAction(m_zoomIn, Orca::Plugin::Core::ZOOM_IN, Context(kBuildSystemOutputContext));

  m_zoomOut = new QAction;
  m_zoomOut->setIcon(Utils::Icons::MINUS.icon());
  connect(m_zoomOut, &QAction::triggered, this, [this] { zoomOut(); });
  ActionManager::registerAction(m_zoomOut, Orca::Plugin::Core::ZOOM_OUT, Context(kBuildSystemOutputContext));
}

auto BuildSystemOutputWindow::toolBar() -> QWidget*
{
  if (!m_toolBar) {
    m_toolBar = new StyledBar(this);
    const auto clearButton = new CommandButton(Orca::Plugin::Core::OUTPUTPANE_CLEAR);
    clearButton->setDefaultAction(m_clear);
    clearButton->setToolTipBase(m_clear->text());

    m_filterOutputLineEdit = new FancyLineEdit;
    m_filterOutputLineEdit->setButtonVisible(FancyLineEdit::Left, true);
    m_filterOutputLineEdit->setButtonIcon(FancyLineEdit::Left, Utils::Icons::MAGNIFIER.icon());
    m_filterOutputLineEdit->setFiltering(true);
    m_filterOutputLineEdit->setHistoryCompleter("ProjectsMode.BuildSystemOutput.Filter");
    connect(m_filterOutputLineEdit, &FancyLineEdit::textChanged, this, &BuildSystemOutputWindow::updateFilter);
    connect(m_filterOutputLineEdit, &FancyLineEdit::returnPressed, this, &BuildSystemOutputWindow::updateFilter);
    connect(m_filterOutputLineEdit, &FancyLineEdit::leftButtonClicked, this, [this] {
      const auto popup = new OptionsPopup(m_filterOutputLineEdit, {kRegExpActionId, kCaseSensitiveActionId, kInvertActionId});
      popup->show();
    });

    const auto zoomInButton = new CommandButton(Orca::Plugin::Core::ZOOM_IN);
    zoomInButton->setDefaultAction(m_zoomIn);
    const auto zoomOutButton = new CommandButton(Orca::Plugin::Core::ZOOM_OUT);
    zoomOutButton->setDefaultAction(m_zoomOut);

    const auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_toolBar->setLayout(layout);
    layout->addWidget(clearButton);
    layout->addWidget(m_filterOutputLineEdit);
    layout->addWidget(zoomInButton);
    layout->addWidget(zoomOutButton);
    layout->addStretch();
  }
  return m_toolBar;
}

auto BuildSystemOutputWindow::updateFilter() -> void
{
  if (!m_filterOutputLineEdit)
    return;
  updateFilterProperties(m_filterOutputLineEdit->text(), m_filterActionCaseSensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive, m_filterActionRegexp->isChecked(), m_invertFilterAction->isChecked());
}

// Standard third level for the generic case: i.e. all except for the Build/Run page
class MiscSettingsPanelItem : public TreeItem // TypedTreeItem<TreeItem, MiscSettingsGroupItem>
{
public:
  MiscSettingsPanelItem(ProjectPanelFactory *factory, Project *project) : m_factory(factory), m_project(project) {}

  ~MiscSettingsPanelItem() override { delete m_widget; }

  auto data(int column, int role) const -> QVariant override;
  auto flags(int column) const -> Qt::ItemFlags override;
  auto setData(int column, const QVariant &, int role) -> bool override;

  auto factory() const -> ProjectPanelFactory* { return m_factory; }

protected:
  ProjectPanelFactory *m_factory = nullptr;
  QPointer<Project> m_project;

  mutable QPointer<QWidget> m_widget = nullptr;
};

auto MiscSettingsPanelItem::data(int column, int role) const -> QVariant
{
  Q_UNUSED(column)
  if (role == Qt::DisplayRole) {
    if (m_factory)
      return m_factory->displayName();
  }

  if (role == PanelWidgetRole) {
    if (!m_widget) {
      const auto widget = m_factory->createWidget(m_project);
      m_widget = new PanelsWidget(m_factory->displayName(), widget);
      m_widget->setFocusProxy(widget);
    }

    return QVariant::fromValue<QWidget*>(m_widget.data());
  }

  if (role == ActiveItemRole) // We are the active one.
    return QVariant::fromValue<TreeItem*>(const_cast<MiscSettingsPanelItem*>(this));

  return QVariant();
}

auto MiscSettingsPanelItem::flags(int column) const -> Qt::ItemFlags
{
  if (m_factory && m_project) {
    if (!m_factory->supports(m_project))
      return Qt::ItemIsSelectable;
  }
  return TreeItem::flags(column);
}

auto MiscSettingsPanelItem::setData(int column, const QVariant &, int role) -> bool
{
  if (role == ItemActivatedDirectlyRole) {
    // Bubble up
    return parent()->setData(column, QVariant::fromValue(static_cast<TreeItem*>(this)), ItemActivatedFromBelowRole);
  }

  return false;
}

// The lower part of the second tree level, i.e. the project settings list.
// The upper part is the TargetSettingsPanelItem .
class MiscSettingsGroupItem : public TreeItem // TypedTreeItem<MiscSettingsPanelItem, ProjectItem>
{
public:
  explicit MiscSettingsGroupItem(Project *project) : m_project(project)
  {
    QTC_ASSERT(m_project, return);
    foreach(ProjectPanelFactory *factory, ProjectPanelFactory::factories())
      appendChild(new MiscSettingsPanelItem(factory, project));
  }

  auto flags(int) const -> Qt::ItemFlags override
  {
    return Qt::NoItemFlags;
  }

  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DisplayRole:
      return ProjectWindow::tr("Project Settings");

    case PanelWidgetRole:
    case ActiveItemRole:
      if (0 <= m_currentPanelIndex && m_currentPanelIndex < childCount())
        return childAt(m_currentPanelIndex)->data(column, role);
    }
    return QVariant();
  }

  auto setData(int column, const QVariant &data, int role) -> bool override
  {
    Q_UNUSED(column)

    if (role == ItemActivatedFromBelowRole) {
      const auto *item = data.value<TreeItem*>();
      QTC_ASSERT(item, return false);
      m_currentPanelIndex = indexOf(item);
      QTC_ASSERT(m_currentPanelIndex != -1, return false);
      parent()->setData(0, QVariant::fromValue(static_cast<TreeItem*>(this)), ItemActivatedFromBelowRole);
      return true;
    }

    return false;
  }

  auto project() const -> Project* { return m_project; }

private:
  int m_currentPanelIndex = -1;

  Project *const m_project;
};

// The first tree level, i.e. projects.
class ProjectItem : public TreeItem {
public:
  ProjectItem() = default;

  ProjectItem(Project *project, const std::function<void()> &changeListener) : m_project(project), m_changeListener(changeListener)
  {
    QTC_ASSERT(m_project, return);
    const auto display = ProjectWindow::tr("Build & Run");
    appendChild(m_targetsItem = new TargetGroupItem(display, project));
    appendChild(m_miscItem = new MiscSettingsGroupItem(project));
  }

  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DisplayRole:
    case ProjectDisplayNameRole:
      return m_project->displayName();

    case Qt::FontRole: {
      QFont font;
      font.setBold(m_project == SessionManager::startupProject());
      return font;
    }

    case PanelWidgetRole:
    case ActiveItemRole:
      if (m_currentChildIndex == 0)
        return m_targetsItem->data(column, role);
      if (m_currentChildIndex == 1)
        return m_miscItem->data(column, role);
    }
    return QVariant();
  }

  auto setData(int column, const QVariant &dat, int role) -> bool override
  {
    Q_UNUSED(column)

    if (role == ItemUpdatedFromBelowRole) {
      announceChange();
      return true;
    }

    if (role == ItemDeactivatedFromBelowRole) {
      announceChange();
      return true;
    }

    if (role == ItemActivatedFromBelowRole) {
      const TreeItem *item = dat.value<TreeItem*>();
      QTC_ASSERT(item, return false);
      const auto res = indexOf(item);
      QTC_ASSERT(res >= 0, return false);
      m_currentChildIndex = res;
      announceChange();
      return true;
    }

    if (role == ItemActivatedDirectlyRole) {
      // Someone selected the project using the combobox or similar.
      SessionManager::setStartupProject(m_project);
      m_currentChildIndex = 0;                                         // Use some Target page by defaults
      m_targetsItem->setData(column, dat, ItemActivatedFromAboveRole); // And propagate downwards.
      announceChange();
      return true;
    }

    return false;
  }

  auto announceChange() -> void
  {
    m_changeListener();
  }

  auto project() const -> Project* { return m_project; }

  auto activeIndex() const -> QModelIndex
  {
    const auto *activeItem = data(0, ActiveItemRole).value<TreeItem*>();
    return activeItem ? activeItem->index() : QModelIndex();
  }

  auto itemForProjectPanel(Id panelId) -> TreeItem*
  {
    return m_miscItem->findChildAtLevel(1, [panelId](const TreeItem *item) {
      return static_cast<const MiscSettingsPanelItem*>(item)->factory()->id() == panelId;
    });
  }

private:
  int m_currentChildIndex = 0; // Start with Build & Run.
  Project *m_project = nullptr;
  TargetGroupItem *m_targetsItem = nullptr;
  MiscSettingsGroupItem *m_miscItem = nullptr;
  const std::function<void ()> m_changeListener;
};

class SelectorDelegate : public QStyledItemDelegate {
public:
  SelectorDelegate() = default;

  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize final;

  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void final;
};

//
// SelectorTree
//

class SelectorTree : public BaseTreeView {
public:
  SelectorTree()
  {
    setWindowTitle("Project Kit Selector");

    header()->hide();
    setExpandsOnDoubleClick(false);
    setHeaderHidden(true);
    setItemsExpandable(false); // No user interaction.
    setRootIsDecorated(false);
    setUniformRowHeights(false); // sic!
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);
    setEditTriggers(NoEditTriggers);
    setActivationMode(SingleClickActivation);
    setObjectName("ProjectNavigation");
    setContextMenuPolicy(Qt::CustomContextMenu);
  }

private:
  // remove branch indicators
  auto drawBranches(QPainter *, const QRect &, const QModelIndex &) const -> void final
  {
    return;
  }

  auto userWantsContextMenu(const QMouseEvent *e) const -> bool
  {
    // On Windows, we get additional mouse events for the item view when right-clicking,
    // causing unwanted kit activation (QTCREATORBUG-24156). Let's suppress these.
    return HostOsInfo::isWindowsHost() && e->button() == Qt::RightButton;
  }

  auto mousePressEvent(QMouseEvent *e) -> void final
  {
    if (!userWantsContextMenu(e))
      BaseTreeView::mousePressEvent(e);
  }

  auto mouseReleaseEvent(QMouseEvent *e) -> void final
  {
    if (!userWantsContextMenu(e))
      BaseTreeView::mouseReleaseEvent(e);
  }
};

class ComboBoxItem : public TreeItem {
public:
  ComboBoxItem(ProjectItem *item) : m_projectItem(item) {}

  auto data(int column, int role) const -> QVariant final
  {
    return m_projectItem ? m_projectItem->data(column, role) : QVariant();
  }

  ProjectItem *m_projectItem;
};

using ProjectsModel = TreeModel<TypedTreeItem<ProjectItem>, ProjectItem>;
using ComboBoxModel = TreeModel<TypedTreeItem<ComboBoxItem>, ComboBoxItem>;

//
// ProjectWindowPrivate
//

class ProjectWindowPrivate : public QObject {
public:
  ProjectWindowPrivate(ProjectWindow *parent) : q(parent)
  {
    m_projectsModel.setHeader({ProjectWindow::tr("Projects")});

    m_selectorTree = new SelectorTree;
    m_selectorTree->setModel(&m_projectsModel);
    m_selectorTree->setItemDelegate(&m_selectorDelegate);
    m_selectorTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_selectorTree, &QAbstractItemView::activated, this, &ProjectWindowPrivate::itemActivated);
    connect(m_selectorTree, &QWidget::customContextMenuRequested, this, &ProjectWindowPrivate::openContextMenu);

    m_projectSelection = new QComboBox;
    m_projectSelection->setModel(&m_comboBoxModel);
    connect(m_projectSelection, QOverload<int>::of(&QComboBox::activated), this, &ProjectWindowPrivate::projectSelected, Qt::QueuedConnection);

    const auto switchProjectAction = new QAction(this);
    ActionManager::registerAction(switchProjectAction, Orca::Plugin::Core::GOTOPREVINHISTORY, Context(Constants::C_PROJECTEXPLORER));
    connect(switchProjectAction, &QAction::triggered, this, [this] {
      if (m_projectSelection->count() > 1)
        m_projectSelection->showPopup();
    });

    const auto sessionManager = SessionManager::instance();
    connect(sessionManager, &SessionManager::projectAdded, this, &ProjectWindowPrivate::registerProject);
    connect(sessionManager, &SessionManager::aboutToRemoveProject, this, &ProjectWindowPrivate::deregisterProject);
    connect(sessionManager, &SessionManager::startupProjectChanged, this, &ProjectWindowPrivate::startupProjectChanged);

    m_importBuild = new QPushButton(ProjectWindow::tr("Import Existing Build..."));
    connect(m_importBuild, &QPushButton::clicked, this, &ProjectWindowPrivate::handleImportBuild);
    connect(sessionManager, &SessionManager::startupProjectChanged, this, [this](Project *project) {
      m_importBuild->setEnabled(project && project->projectImporter());
    });

    m_manageKits = new QPushButton(ProjectWindow::tr("Manage Kits..."));
    connect(m_manageKits, &QPushButton::clicked, this, &ProjectWindowPrivate::handleManageKits);

    const auto styledBar = new StyledBar; // The black blob on top of the side bar
    styledBar->setObjectName("ProjectModeStyledBar");

    const auto selectorView = new QWidget;          // Black blob + Combobox + Project tree below.
    selectorView->setObjectName("ProjectSelector"); // Needed for dock widget state saving
    selectorView->setWindowTitle(ProjectWindow::tr("Project Selector"));
    selectorView->setAutoFillBackground(true);
    selectorView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(selectorView, &QWidget::customContextMenuRequested, this, &ProjectWindowPrivate::openContextMenu);

    const auto activeLabel = new QLabel(ProjectWindow::tr("Active Project"));
    auto font = activeLabel->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.2);
    activeLabel->setFont(font);

    const auto innerLayout = new QVBoxLayout;
    innerLayout->setSpacing(10);
    innerLayout->setContentsMargins(PanelsWidget::PanelVMargin, innerLayout->spacing(), PanelsWidget::PanelVMargin, 0);
    innerLayout->addWidget(m_manageKits);
    innerLayout->addSpacerItem(new QSpacerItem(10, 30, QSizePolicy::Maximum, QSizePolicy::Maximum));
    innerLayout->addWidget(activeLabel);
    innerLayout->addWidget(m_projectSelection);
    innerLayout->addWidget(m_importBuild);
    innerLayout->addWidget(m_selectorTree);

    const auto selectorLayout = new QVBoxLayout(selectorView);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->addWidget(styledBar);
    selectorLayout->addLayout(innerLayout);

    const auto selectorDock = q->addDockForWidget(selectorView, true);
    q->addDockWidget(Qt::LeftDockWidgetArea, selectorDock);

    m_buildSystemOutput = new BuildSystemOutputWindow;
    const auto output = new QWidget;
    // ProjectWindow sets background role to Base which is wrong for the output window,
    // especially the find tool bar (resulting in wrong label color)
    output->setBackgroundRole(QPalette::Window);
    output->setObjectName("BuildSystemOutput");
    output->setWindowTitle(ProjectWindow::tr("Build System Output"));
    const auto outputLayout = new QVBoxLayout;
    output->setLayout(outputLayout);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    outputLayout->addWidget(m_buildSystemOutput->toolBar());
    outputLayout->addWidget(m_buildSystemOutput);
    outputLayout->addWidget(new FindToolBarPlaceHolder(m_buildSystemOutput));
    const auto outputDock = q->addDockForWidget(output, true);
    q->addDockWidget(Qt::RightDockWidgetArea, outputDock);
  }

  auto updatePanel() -> void
  {
    const auto projectItem = m_projectsModel.rootItem()->childAt(0);
    if (!projectItem)
      return;
    setPanel(projectItem->data(0, PanelWidgetRole).value<QWidget*>());

    const auto activeIndex = projectItem->activeIndex();
    m_selectorTree->expandAll();
    m_selectorTree->selectionModel()->clear();
    m_selectorTree->selectionModel()->select(activeIndex, QItemSelectionModel::Select);
  }

  auto registerProject(Project *project) -> void
  {
    QTC_ASSERT(itemForProject(project) == nullptr, return);
    const auto projectItem = new ProjectItem(project, [this] { updatePanel(); });
    m_comboBoxModel.rootItem()->appendChild(new ComboBoxItem(projectItem));
  }

  auto deregisterProject(Project *project) -> void
  {
    const auto item = itemForProject(project);
    QTC_ASSERT(item, return);
    if (item->m_projectItem->parent())
      m_projectsModel.takeItem(item->m_projectItem);
    delete item->m_projectItem;
    item->m_projectItem = nullptr;
    m_comboBoxModel.destroyItem(item);
  }

  auto projectSelected(int index) -> void
  {
    const auto project = m_comboBoxModel.rootItem()->childAt(index)->m_projectItem->project();
    SessionManager::setStartupProject(project);
  }

  auto itemForProject(Project *project) const -> ComboBoxItem*
  {
    return m_comboBoxModel.findItemAtLevel<1>([project](ComboBoxItem *item) {
      return item->m_projectItem->project() == project;
    });
  }

  auto startupProjectChanged(Project *project) -> void
  {
    if (const auto current = m_projectsModel.rootItem()->childAt(0))
      m_projectsModel.takeItem(current); // Keep item as such alive.
    if (!project)                        // Shutting down.
      return;
    const auto comboboxItem = itemForProject(project);
    QTC_ASSERT(comboboxItem, return);
    m_projectsModel.rootItem()->appendChild(comboboxItem->m_projectItem);
    m_projectSelection->setCurrentIndex(comboboxItem->indexInParent());
    m_selectorTree->expandAll();
    m_selectorTree->setRootIndex(m_projectsModel.index(0, 0, QModelIndex()));
    updatePanel();
  }

  auto itemActivated(const QModelIndex &index) -> void
  {
    if (const auto item = m_projectsModel.itemForIndex(index))
      item->setData(0, QVariant(), ItemActivatedDirectlyRole);
  }

  auto activateProjectPanel(Id panelId) -> void
  {
    if (const auto projectItem = m_projectsModel.rootItem()->childAt(0)) {
      if (const auto item = projectItem->itemForProjectPanel(panelId))
        itemActivated(item->index());
    }
  }

  auto openContextMenu(const QPoint &pos) -> void
  {
    QMenu menu;

    const auto projectItem = m_projectsModel.rootItem()->childAt(0);
    const auto project = projectItem ? projectItem->project() : nullptr;

    const auto index = m_selectorTree->indexAt(pos);
    const auto item = m_projectsModel.itemForIndex(index);
    if (item)
      item->setData(0, QVariant::fromValue(&menu), ContextMenuItemAdderRole);

    if (!menu.actions().isEmpty())
      menu.addSeparator();

    const auto importBuild = menu.addAction(ProjectWindow::tr("Import Existing Build..."));
    importBuild->setEnabled(project && project->projectImporter());
    const auto manageKits = menu.addAction(ProjectWindow::tr("Manage Kits..."));

    const auto act = menu.exec(m_selectorTree->mapToGlobal(pos));

    if (act == importBuild)
      handleImportBuild();
    else if (act == manageKits)
      handleManageKits();
  }

  auto handleManageKits() -> void
  {
    if (const auto projectItem = m_projectsModel.rootItem()->childAt(0)) {
      if (const auto kitPage = KitOptionsPage::instance())
        kitPage->showKit(KitManager::kit(Id::fromSetting(projectItem->data(0, KitIdRole))));
    }
    ICore::showOptionsDialog(Constants::KITS_SETTINGS_PAGE_ID);
  }

  auto handleImportBuild() -> void
  {
    const auto projectItem = m_projectsModel.rootItem()->childAt(0);
    const auto project = projectItem ? projectItem->project() : nullptr;
    const auto projectImporter = project ? project->projectImporter() : nullptr;
    QTC_ASSERT(projectImporter, return);

    const auto importDir = FileUtils::getExistingDirectory(nullptr, ProjectWindow::tr("Import Directory"), project->projectDirectory());

    Target *lastTarget = nullptr;
    BuildConfiguration *lastBc = nullptr;
    for (const auto &info : projectImporter->import(importDir, false)) {
      auto target = project->target(info.kitId);
      if (!target)
        target = project->addTargetForKit(KitManager::kit(info.kitId));
      if (target) {
        projectImporter->makePersistent(target->kit());
        const auto bc = info.factory->create(target, info);
        QTC_ASSERT(bc, continue);
        target->addBuildConfiguration(bc);

        lastTarget = target;
        lastBc = bc;
      }
    }
    if (lastTarget && lastBc) {
      SessionManager::setActiveBuildConfiguration(lastTarget, lastBc, SetActive::Cascade);
      SessionManager::setActiveTarget(project, lastTarget, SetActive::Cascade);
    }
  }

  auto setPanel(QWidget *panel) -> void
  {
    q->savePersistentSettings();
    if (const auto widget = q->centralWidget()) {
      q->takeCentralWidget();
      widget->hide(); // Don't delete.
    }
    if (panel) {
      q->setCentralWidget(panel);
      panel->show();
      if (q->hasFocus()) // we get assigned focus from setFocusToCurrentMode, pass that on
        panel->setFocus();
    }
    q->loadPersistentSettings();
  }

  ProjectWindow *q;
  ProjectsModel m_projectsModel;
  ComboBoxModel m_comboBoxModel;
  SelectorDelegate m_selectorDelegate;
  QComboBox *m_projectSelection;
  SelectorTree *m_selectorTree;
  QPushButton *m_importBuild;
  QPushButton *m_manageKits;
  BuildSystemOutputWindow *m_buildSystemOutput;
};

//
// ProjectWindow
//

ProjectWindow::ProjectWindow() : d(std::make_unique<ProjectWindowPrivate>(this))
{
  setBackgroundRole(QPalette::Base);

  // Request custom context menu but do not provide any to avoid
  // the creation of the dock window selection menu.
  setContextMenuPolicy(Qt::CustomContextMenu);
}

auto ProjectWindow::activateProjectPanel(Id panelId) -> void
{
  d->activateProjectPanel(panelId);
}

auto ProjectWindow::buildSystemOutput() const -> OutputWindow*
{
  return d->m_buildSystemOutput;
}

auto ProjectWindow::hideEvent(QHideEvent *event) -> void
{
  savePersistentSettings();
  FancyMainWindow::hideEvent(event);
}

auto ProjectWindow::showEvent(QShowEvent *event) -> void
{
  FancyMainWindow::showEvent(event);
  loadPersistentSettings();
}

ProjectWindow::~ProjectWindow() = default;

constexpr char PROJECT_WINDOW_KEY[] = "ProjectExplorer.ProjectWindow";

auto ProjectWindow::savePersistentSettings() const -> void
{
  if (!centralWidget())
    return;
  QSettings *const settings = ICore::settings();
  settings->beginGroup(PROJECT_WINDOW_KEY);
  saveSettings(settings);
  settings->endGroup();
}

auto ProjectWindow::loadPersistentSettings() -> void
{
  if (!centralWidget())
    return;
  QSettings *const settings = ICore::settings();
  settings->beginGroup(PROJECT_WINDOW_KEY);
  restoreSettings(settings);
  settings->endGroup();
}

auto SelectorDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize
{
  auto s = QStyledItemDelegate::sizeHint(option, index);
  const auto model = static_cast<const ProjectsModel*>(index.model());
  if (const auto item = model->itemForIndex(index)) {
    switch (item->level()) {
    case 2:
      s = QSize(s.width(), 3 * s.height());
      break;
    case 3:
    case 4:
      s = QSize(s.width(), s.height() * 1.2);
      break;
    }
  }
  return s;
}

auto SelectorDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  const auto model = static_cast<const ProjectsModel*>(index.model());
  auto opt = option;
  if (const auto item = model->itemForIndex(index)) {
    switch (item->level()) {
    case 2: {
      const QColor col = orcaTheme()->color(Theme::TextColorNormal);
      opt.palette.setColor(QPalette::Text, col);
      opt.font.setBold(true);
      opt.font.setPointSizeF(opt.font.pointSizeF() * 1.2);
      break;
    }
    }
  }
  QStyledItemDelegate::paint(painter, opt, index);
}

} // namespace Internal
} // namespace ProjectExplorer
