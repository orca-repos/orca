// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakesettingspage.hpp"

#include "cmakeprojectconstants.hpp"
#include "cmaketool.hpp"
#include "cmaketoolmanager.hpp"

#include <core/core-options-page-interface.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/detailswidget.hpp>
#include <utils/fileutils.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/treemodel.hpp>
#include <utils/utilsicons.hpp>

#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTreeView>
#include <QUuid>

using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

class CMakeToolTreeItem;

// --------------------------------------------------------------------------
// CMakeToolItemModel
// --------------------------------------------------------------------------

class CMakeToolItemModel : public TreeModel<TreeItem, TreeItem, CMakeToolTreeItem> {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::CMakeSettingsPage)

public:
  CMakeToolItemModel();

  auto cmakeToolItem(const Utils::Id &id) const -> CMakeToolTreeItem*;
  auto cmakeToolItem(const QModelIndex &index) const -> CMakeToolTreeItem*;
  auto addCMakeTool(const QString &name, const FilePath &executable, const FilePath &qchFile, const bool autoRun, const bool isAutoDetected) -> QModelIndex;
  auto addCMakeTool(const CMakeTool *item, bool changed) -> void;
  auto autoGroupItem() const -> TreeItem*;
  auto manualGroupItem() const -> TreeItem*;
  auto reevaluateChangedFlag(CMakeToolTreeItem *item) const -> void;
  auto updateCMakeTool(const Utils::Id &id, const QString &displayName, const FilePath &executable, const FilePath &qchFile, bool autoRun) -> void;
  auto removeCMakeTool(const Utils::Id &id) -> void;
  auto apply() -> void;
  auto defaultItemId() const -> Utils::Id;
  auto setDefaultItemId(const Utils::Id &id) -> void;
  auto uniqueDisplayName(const QString &base) const -> QString;

private:
  Utils::Id m_defaultItemId;
  QList<Utils::Id> m_removedItems;
};

class CMakeToolTreeItem : public TreeItem {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::CMakeSettingsPage)

public:
  CMakeToolTreeItem(const CMakeTool *item, bool changed) : m_id(item->id()), m_name(item->displayName()), m_executable(item->filePath()), m_qchFile(item->qchFilePath()), m_versionDisplay(item->versionDisplay()), m_detectionSource(item->detectionSource()), m_isAutoRun(item->isAutoRun()), m_autodetected(item->isAutoDetected()), m_isSupported(item->hasFileApi()), m_changed(changed)
  {
    updateErrorFlags();
  }

  CMakeToolTreeItem(const QString &name, const FilePath &executable, const FilePath &qchFile, bool autoRun, bool autodetected) : m_id(Utils::Id::fromString(QUuid::createUuid().toString())), m_name(name), m_executable(executable), m_qchFile(qchFile), m_isAutoRun(autoRun), m_autodetected(autodetected)
  {
    updateErrorFlags();
  }

  auto updateErrorFlags() -> void
  {
    const auto filePath = CMakeTool::cmakeExecutable(m_executable);
    m_pathExists = filePath.exists();
    m_pathIsFile = filePath.isFile();
    m_pathIsExecutable = filePath.isExecutableFile();

    CMakeTool cmake(m_autodetected ? CMakeTool::AutoDetection : CMakeTool::ManualDetection, m_id);
    cmake.setFilePath(m_executable);
    m_isSupported = cmake.hasFileApi();

    m_tooltip = tr("Version: %1").arg(cmake.versionDisplay());
    m_tooltip += "<br>" + tr("Supports fileApi: %1").arg(m_isSupported ? tr("yes") : tr("no"));
    m_tooltip += "<br>" + tr("Detection source: \"%1\"").arg(m_detectionSource);

    m_versionDisplay = cmake.versionDisplay();
  }

  CMakeToolTreeItem() = default;

  auto model() const -> CMakeToolItemModel* { return static_cast<CMakeToolItemModel*>(TreeItem::model()); }

  auto data(int column, int role) const -> QVariant override
  {
    switch (role) {
    case Qt::DisplayRole: {
      switch (column) {
      case 0: {
        auto name = m_name;
        if (model()->defaultItemId() == m_id)
          name += tr(" (Default)");
        return name;
      }
      case 1: {
        return m_executable.toUserOutput();
      }
      } // switch (column)
      return QVariant();
    }
    case Qt::FontRole: {
      QFont font;
      font.setBold(m_changed);
      font.setItalic(model()->defaultItemId() == m_id);
      return font;
    }
    case Qt::ToolTipRole: {
      auto result = m_tooltip;
      QString error;
      if (!m_pathExists) {
        error = QCoreApplication::translate("CMakeProjectManager::Internal::CMakeToolTreeItem", "CMake executable path does not exist.");
      } else if (!m_pathIsFile) {
        error = QCoreApplication::translate("CMakeProjectManager::Internal::CMakeToolTreeItem", "CMake executable path is not a file.");
      } else if (!m_pathIsExecutable) {
        error = QCoreApplication::translate("CMakeProjectManager::Internal::CMakeToolTreeItem", "CMake executable path is not executable.");
      } else if (!m_isSupported) {
        error = QCoreApplication::translate("CMakeProjectManager::Internal::CMakeToolTreeItem", "CMake executable does not provide required IDE integration features.");
      }
      if (result.isEmpty() || error.isEmpty())
        return QString("%1%2").arg(result).arg(error);
      else
        return QString("%1<br><br><b>%2</b>").arg(result).arg(error);
    }
    case Qt::DecorationRole: {
      if (column != 0)
        return QVariant();

      const auto hasError = !m_isSupported || !m_pathExists || !m_pathIsFile || !m_pathIsExecutable;
      if (hasError)
        return Utils::Icons::CRITICAL.icon();
      return QVariant();
    }
    }
    return QVariant();
  }

  Utils::Id m_id;
  QString m_name;
  QString m_tooltip;
  FilePath m_executable;
  FilePath m_qchFile;
  QString m_versionDisplay;
  QString m_detectionSource;
  bool m_isAutoRun = true;
  bool m_pathExists = false;
  bool m_pathIsFile = false;
  bool m_pathIsExecutable = false;
  bool m_autodetected = false;
  bool m_isSupported = false;
  bool m_changed = true;
};

CMakeToolItemModel::CMakeToolItemModel()
{
  setHeader({tr("Name"), tr("Path")});
  rootItem()->appendChild(new StaticTreeItem({ProjectExplorer::Constants::msgAutoDetected()}, {ProjectExplorer::Constants::msgAutoDetectedToolTip()}));
  rootItem()->appendChild(new StaticTreeItem(tr("Manual")));

  foreach(const CMakeTool *item, CMakeToolManager::cmakeTools())
    addCMakeTool(item, false);

  auto defTool = CMakeToolManager::defaultCMakeTool();
  m_defaultItemId = defTool ? defTool->id() : Utils::Id();
  connect(CMakeToolManager::instance(), &CMakeToolManager::cmakeRemoved, this, &CMakeToolItemModel::removeCMakeTool);
  connect(CMakeToolManager::instance(), &CMakeToolManager::cmakeAdded, this, [this](const Utils::Id &id) { addCMakeTool(CMakeToolManager::findById(id), false); });
}

auto CMakeToolItemModel::addCMakeTool(const QString &name, const FilePath &executable, const FilePath &qchFile, const bool autoRun, const bool isAutoDetected) -> QModelIndex
{
  auto item = new CMakeToolTreeItem(name, executable, qchFile, autoRun, isAutoDetected);
  if (isAutoDetected)
    autoGroupItem()->appendChild(item);
  else
    manualGroupItem()->appendChild(item);

  return item->index();
}

auto CMakeToolItemModel::addCMakeTool(const CMakeTool *item, bool changed) -> void
{
  QTC_ASSERT(item, return);

  if (cmakeToolItem(item->id()))
    return;

  auto treeItem = new CMakeToolTreeItem(item, changed);
  if (item->isAutoDetected())
    autoGroupItem()->appendChild(treeItem);
  else
    manualGroupItem()->appendChild(treeItem);
}

auto CMakeToolItemModel::autoGroupItem() const -> TreeItem*
{
  return rootItem()->childAt(0);
}

auto CMakeToolItemModel::manualGroupItem() const -> TreeItem*
{
  return rootItem()->childAt(1);
}

auto CMakeToolItemModel::reevaluateChangedFlag(CMakeToolTreeItem *item) const -> void
{
  auto orig = CMakeToolManager::findById(item->m_id);
  item->m_changed = !orig || orig->displayName() != item->m_name || orig->filePath() != item->m_executable || orig->qchFilePath() != item->m_qchFile;

  //make sure the item is marked as changed when the default cmake was changed
  auto origDefTool = CMakeToolManager::defaultCMakeTool();
  auto origDefault = origDefTool ? origDefTool->id() : Utils::Id();
  if (origDefault != m_defaultItemId) {
    if (item->m_id == origDefault || item->m_id == m_defaultItemId)
      item->m_changed = true;
  }

  item->update(); // Notify views.
}

auto CMakeToolItemModel::updateCMakeTool(const Utils::Id &id, const QString &displayName, const FilePath &executable, const FilePath &qchFile, bool autoRun) -> void
{
  auto treeItem = cmakeToolItem(id);
  QTC_ASSERT(treeItem, return);

  treeItem->m_name = displayName;
  treeItem->m_executable = executable;
  treeItem->m_qchFile = qchFile;
  treeItem->m_isAutoRun = autoRun;

  treeItem->updateErrorFlags();

  reevaluateChangedFlag(treeItem);
}

auto CMakeToolItemModel::cmakeToolItem(const Utils::Id &id) const -> CMakeToolTreeItem*
{
  return findItemAtLevel<2>([id](CMakeToolTreeItem *n) { return n->m_id == id; });
}

auto CMakeToolItemModel::cmakeToolItem(const QModelIndex &index) const -> CMakeToolTreeItem*
{
  return itemForIndexAtLevel<2>(index);
}

auto CMakeToolItemModel::removeCMakeTool(const Utils::Id &id) -> void
{
  if (m_removedItems.contains(id))
    return; // Item has already been removed in the model!

  auto treeItem = cmakeToolItem(id);
  QTC_ASSERT(treeItem, return);

  m_removedItems.append(id);
  destroyItem(treeItem);
}

auto CMakeToolItemModel::apply() -> void
{
  foreach(const Utils::Id &id, m_removedItems)
    CMakeToolManager::deregisterCMakeTool(id);

  QList<CMakeToolTreeItem*> toRegister;
  forItemsAtLevel<2>([&toRegister](CMakeToolTreeItem *item) {
    item->m_changed = false;
    if (auto cmake = CMakeToolManager::findById(item->m_id)) {
      cmake->setDisplayName(item->m_name);
      cmake->setFilePath(item->m_executable);
      cmake->setQchFilePath(item->m_qchFile);
      cmake->setDetectionSource(item->m_detectionSource);
      cmake->setAutorun(item->m_isAutoRun);
    } else {
      toRegister.append(item);
    }
  });

  foreach(CMakeToolTreeItem *item, toRegister) {
    auto detection = item->m_autodetected ? CMakeTool::AutoDetection : CMakeTool::ManualDetection;
    auto cmake = std::make_unique<CMakeTool>(detection, item->m_id);
    cmake->setDisplayName(item->m_name);
    cmake->setFilePath(item->m_executable);
    cmake->setQchFilePath(item->m_qchFile);
    cmake->setDetectionSource(item->m_detectionSource);
    if (!CMakeToolManager::registerCMakeTool(std::move(cmake)))
      item->m_changed = true;
  }

  CMakeToolManager::setDefaultCMakeTool(defaultItemId());
}

auto CMakeToolItemModel::defaultItemId() const -> Utils::Id
{
  return m_defaultItemId;
}

auto CMakeToolItemModel::setDefaultItemId(const Utils::Id &id) -> void
{
  if (m_defaultItemId == id)
    return;

  auto oldDefaultId = m_defaultItemId;
  m_defaultItemId = id;

  auto newDefault = cmakeToolItem(id);
  if (newDefault)
    reevaluateChangedFlag(newDefault);

  auto oldDefault = cmakeToolItem(oldDefaultId);
  if (oldDefault)
    reevaluateChangedFlag(oldDefault);
}

auto CMakeToolItemModel::uniqueDisplayName(const QString &base) const -> QString
{
  QStringList names;
  forItemsAtLevel<2>([&names](CMakeToolTreeItem *item) { names << item->m_name; });
  return Utils::makeUniquelyNumbered(base, names);
}

// -----------------------------------------------------------------------
// CMakeToolItemConfigWidget
// -----------------------------------------------------------------------

class CMakeToolItemConfigWidget : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::CMakeSettingsPage)
public:
  explicit CMakeToolItemConfigWidget(CMakeToolItemModel *model);
  auto load(const CMakeToolTreeItem *item) -> void;
  auto store() const -> void;

private:
  auto updateQchFilePath() -> void;

  CMakeToolItemModel *m_model;
  QLineEdit *m_displayNameLineEdit;
  QCheckBox *m_autoRunCheckBox;
  PathChooser *m_binaryChooser;
  PathChooser *m_qchFileChooser;
  QLabel *m_versionLabel;
  Utils::Id m_id;
  bool m_loadingItem;
};

CMakeToolItemConfigWidget::CMakeToolItemConfigWidget(CMakeToolItemModel *model) : m_model(model), m_loadingItem(false)
{
  m_displayNameLineEdit = new QLineEdit(this);

  m_binaryChooser = new PathChooser(this);
  m_binaryChooser->setExpectedKind(PathChooser::ExistingCommand);
  m_binaryChooser->setMinimumWidth(400);
  m_binaryChooser->setHistoryCompleter(QLatin1String("Cmake.Command.History"));
  m_binaryChooser->setCommandVersionArguments({"--version"});

  m_qchFileChooser = new PathChooser(this);
  m_qchFileChooser->setExpectedKind(PathChooser::File);
  m_qchFileChooser->setMinimumWidth(400);
  m_qchFileChooser->setHistoryCompleter(QLatin1String("Cmake.qchFile.History"));
  m_qchFileChooser->setPromptDialogFilter("*.qch");
  m_qchFileChooser->setPromptDialogTitle(tr("CMake .qch File"));

  m_versionLabel = new QLabel(this);

  m_autoRunCheckBox = new QCheckBox;
  m_autoRunCheckBox->setText(tr("Autorun CMake"));
  m_autoRunCheckBox->setToolTip(tr("Automatically run CMake after changes to CMake project files."));

  auto formLayout = new QFormLayout(this);
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  formLayout->addRow(new QLabel(tr("Name:")), m_displayNameLineEdit);
  formLayout->addRow(new QLabel(tr("Path:")), m_binaryChooser);
  formLayout->addRow(new QLabel(tr("Version:")), m_versionLabel);
  formLayout->addRow(new QLabel(tr("Help file:")), m_qchFileChooser);
  formLayout->addRow(m_autoRunCheckBox);

  connect(m_binaryChooser, &PathChooser::rawPathChanged, this, [this]() {
    updateQchFilePath();
    m_qchFileChooser->setBaseDirectory(m_binaryChooser->filePath().parentDir());
    store();
  });
  connect(m_qchFileChooser, &PathChooser::rawPathChanged, this, &CMakeToolItemConfigWidget::store);
  connect(m_displayNameLineEdit, &QLineEdit::textChanged, this, &CMakeToolItemConfigWidget::store);
  connect(m_autoRunCheckBox, &QCheckBox::toggled, this, &CMakeToolItemConfigWidget::store);
}

auto CMakeToolItemConfigWidget::store() const -> void
{
  if (!m_loadingItem && m_id.isValid())
    m_model->updateCMakeTool(m_id, m_displayNameLineEdit->text(), m_binaryChooser->filePath(), m_qchFileChooser->filePath(), m_autoRunCheckBox->checkState() == Qt::Checked);
}

auto CMakeToolItemConfigWidget::updateQchFilePath() -> void
{
  if (m_qchFileChooser->filePath().isEmpty())
    m_qchFileChooser->setFilePath(CMakeTool::searchQchFile(m_binaryChooser->filePath()));
}

auto CMakeToolItemConfigWidget::load(const CMakeToolTreeItem *item) -> void
{
  m_loadingItem = true; // avoid intermediate signal handling
  m_id = Utils::Id();
  if (!item) {
    m_loadingItem = false;
    return;
  }

  // Set values:
  m_displayNameLineEdit->setEnabled(!item->m_autodetected);
  m_displayNameLineEdit->setText(item->m_name);

  m_binaryChooser->setReadOnly(item->m_autodetected);
  m_binaryChooser->setFilePath(item->m_executable);

  m_qchFileChooser->setReadOnly(item->m_autodetected);
  m_qchFileChooser->setBaseDirectory(item->m_executable.parentDir());
  m_qchFileChooser->setFilePath(item->m_qchFile);

  m_versionLabel->setText(item->m_versionDisplay);

  m_autoRunCheckBox->setChecked(item->m_isAutoRun);

  m_id = item->m_id;
  m_loadingItem = false;
}

// --------------------------------------------------------------------------
// CMakeToolConfigWidget
// --------------------------------------------------------------------------

class CMakeToolConfigWidget : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeToolConfigWidget)
public:
  CMakeToolConfigWidget()
  {
    m_addButton = new QPushButton(tr("Add"), this);

    m_cloneButton = new QPushButton(tr("Clone"), this);
    m_cloneButton->setEnabled(false);

    m_delButton = new QPushButton(tr("Remove"), this);
    m_delButton->setEnabled(false);

    m_makeDefButton = new QPushButton(tr("Make Default"), this);
    m_makeDefButton->setEnabled(false);
    m_makeDefButton->setToolTip(tr("Set as the default CMake Tool to use when creating a new kit or when no value is set."));

    m_container = new DetailsWidget(this);
    m_container->setState(DetailsWidget::NoSummary);
    m_container->setVisible(false);

    m_cmakeToolsView = new QTreeView(this);
    m_cmakeToolsView->setModel(&m_model);
    m_cmakeToolsView->setUniformRowHeights(true);
    m_cmakeToolsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cmakeToolsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cmakeToolsView->expandAll();

    auto header = m_cmakeToolsView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);

    auto buttonLayout = new QVBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_cloneButton);
    buttonLayout->addWidget(m_delButton);
    buttonLayout->addWidget(m_makeDefButton);
    buttonLayout->addItem(new QSpacerItem(10, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    auto verticalLayout = new QVBoxLayout();
    verticalLayout->addWidget(m_cmakeToolsView);
    verticalLayout->addWidget(m_container);

    auto horizontalLayout = new QHBoxLayout(this);
    horizontalLayout->addLayout(verticalLayout);
    horizontalLayout->addLayout(buttonLayout);

    connect(m_cmakeToolsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CMakeToolConfigWidget::currentCMakeToolChanged, Qt::QueuedConnection);

    connect(m_addButton, &QAbstractButton::clicked, this, &CMakeToolConfigWidget::addCMakeTool);
    connect(m_cloneButton, &QAbstractButton::clicked, this, &CMakeToolConfigWidget::cloneCMakeTool);
    connect(m_delButton, &QAbstractButton::clicked, this, &CMakeToolConfigWidget::removeCMakeTool);
    connect(m_makeDefButton, &QAbstractButton::clicked, this, &CMakeToolConfigWidget::setDefaultCMakeTool);

    m_itemConfigWidget = new CMakeToolItemConfigWidget(&m_model);
    m_container->setWidget(m_itemConfigWidget);
  }

  auto apply() -> void final;

  auto cloneCMakeTool() -> void;
  auto addCMakeTool() -> void;
  auto removeCMakeTool() -> void;
  auto setDefaultCMakeTool() -> void;
  auto currentCMakeToolChanged(const QModelIndex &newCurrent) -> void;

  CMakeToolItemModel m_model;
  QTreeView *m_cmakeToolsView;
  QPushButton *m_addButton;
  QPushButton *m_cloneButton;
  QPushButton *m_delButton;
  QPushButton *m_makeDefButton;
  DetailsWidget *m_container;
  CMakeToolItemConfigWidget *m_itemConfigWidget;
  CMakeToolTreeItem *m_currentItem = nullptr;
};

auto CMakeToolConfigWidget::apply() -> void
{
  m_itemConfigWidget->store();
  m_model.apply();
}

auto CMakeToolConfigWidget::cloneCMakeTool() -> void
{
  if (!m_currentItem)
    return;

  auto newItem = m_model.addCMakeTool(tr("Clone of %1").arg(m_currentItem->m_name), m_currentItem->m_executable, m_currentItem->m_qchFile, m_currentItem->m_isAutoRun, false);

  m_cmakeToolsView->setCurrentIndex(newItem);
}

auto CMakeToolConfigWidget::addCMakeTool() -> void
{
  auto newItem = m_model.addCMakeTool(m_model.uniqueDisplayName(tr("New CMake")), FilePath(), FilePath(), true, false);

  m_cmakeToolsView->setCurrentIndex(newItem);
}

auto CMakeToolConfigWidget::removeCMakeTool() -> void
{
  auto delDef = m_model.defaultItemId() == m_currentItem->m_id;
  m_model.removeCMakeTool(m_currentItem->m_id);
  m_currentItem = nullptr;

  if (delDef) {
    auto it = static_cast<CMakeToolTreeItem*>(m_model.autoGroupItem()->firstChild());
    if (!it)
      it = static_cast<CMakeToolTreeItem*>(m_model.manualGroupItem()->firstChild());
    if (it)
      m_model.setDefaultItemId(it->m_id);
  }

  auto newCurrent = m_model.manualGroupItem()->lastChild();
  if (!newCurrent)
    newCurrent = m_model.autoGroupItem()->lastChild();

  if (newCurrent)
    m_cmakeToolsView->setCurrentIndex(newCurrent->index());
}

auto CMakeToolConfigWidget::setDefaultCMakeTool() -> void
{
  if (!m_currentItem)
    return;

  m_model.setDefaultItemId(m_currentItem->m_id);
  m_makeDefButton->setEnabled(false);
}

auto CMakeToolConfigWidget::currentCMakeToolChanged(const QModelIndex &newCurrent) -> void
{
  m_currentItem = m_model.cmakeToolItem(newCurrent);
  m_itemConfigWidget->load(m_currentItem);
  m_container->setVisible(m_currentItem);
  m_cloneButton->setEnabled(m_currentItem);
  m_delButton->setEnabled(m_currentItem && !m_currentItem->m_autodetected);
  m_makeDefButton->setEnabled(m_currentItem && (!m_model.defaultItemId().isValid() || m_currentItem->m_id != m_model.defaultItemId()));
}

/////
// CMakeSettingsPage
////

CMakeSettingsPage::CMakeSettingsPage()
{
  setId(Constants::CMAKE_SETTINGS_PAGE_ID);
  setDisplayName(CMakeToolConfigWidget::tr("CMake"));
  setCategory(ProjectExplorer::Constants::KITS_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new CMakeToolConfigWidget; });
}

} // namespace Internal
} // namespace CMakeProjectManager
