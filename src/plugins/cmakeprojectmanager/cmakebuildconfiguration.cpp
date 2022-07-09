// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakebuildconfiguration.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakebuildstep.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakeconfigitem.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectplugin.hpp"
#include "cmakespecificsettings.hpp"
#include "configmodel.hpp"
#include "configmodelitemdelegate.hpp"
#include "fileapiparser.hpp"

#include <constants/android/androidconstants.hpp>
#include <constants/docker/dockerconstants.hpp>
#include <constants/ios/iosconstants.hpp>
#include <constants/qnx/qnxconstants.hpp>
#include <constants/webassembly/webassemblyconstants.hpp>

#include <core/find/itemviewfind.hpp>
#include <core/icore.hpp>

#include <projectexplorer/buildaspects.hpp>
#include <projectexplorer/buildinfo.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/namedwidget.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/taskhub.hpp>

#include <qtsupport/baseqtversion.hpp>
#include <qtsupport/qtbuildaspects.hpp>
#include <qtsupport/qtkitinformation.hpp>

#include <utils/algorithm.hpp>
#include <utils/categorysortfiltermodel.hpp>
#include <utils/checkablemessagebox.hpp>
#include <utils/detailswidget.hpp>
#include <utils/headerviewstretcher.hpp>
#include <utils/infolabel.hpp>
#include <utils/itemviews.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/progressindicator.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/variablechooser.hpp>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

using namespace ProjectExplorer;
using namespace Utils;
using namespace CMakeProjectManager::Internal;

namespace CMakeProjectManager {

static Q_LOGGING_CATEGORY(cmakeBuildConfigurationLog, "qtc.cmake.bc", QtWarningMsg);

constexpr char CONFIGURATION_KEY[] = "CMake.Configuration";
constexpr char DEVELOPMENT_TEAM_FLAG[] = "Ios:DevelopmentTeam:Flag";
constexpr char PROVISIONING_PROFILE_FLAG[] = "Ios:ProvisioningProfile:Flag";
constexpr char CMAKE_OSX_ARCHITECTURES_FLAG[] = "CMAKE_OSX_ARCHITECTURES:DefaultFlag";
constexpr char CMAKE_QT6_TOOLCHAIN_FILE_ARG[] = "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=%{Qt:QT_INSTALL_PREFIX}/lib/cmake/Qt6/qt.toolchain.cmake";

namespace Internal {

class CMakeBuildSettingsWidget : public NamedWidget {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeBuildSettingsWidget)
public:
  CMakeBuildSettingsWidget(CMakeBuildConfiguration *bc);

  auto setError(const QString &message) -> void;
  auto setWarning(const QString &message) -> void;

private:
  auto updateButtonState() -> void;
  auto updateAdvancedCheckBox() -> void;
  auto updateFromKit() -> void;
  auto updateConfigurationStateIndex(int index) -> void;
  auto getQmlDebugCxxFlags() -> CMakeConfig;
  auto getSigningFlagsChanges() -> CMakeConfig;

  auto updateSelection() -> void;
  auto updateConfigurationStateSelection() -> void;
  auto isInitialConfiguration() const -> bool;
  auto setVariableUnsetFlag(bool unsetFlag) -> void;
  auto createForceAction(int type, const QModelIndex &idx) -> QAction*;

  auto eventFilter(QObject *target, QEvent *event) -> bool override;

  auto batchEditConfiguration() -> void;
  auto reconfigureWithInitialParameters(CMakeBuildConfiguration *bc) -> void;
  auto updateInitialCMakeArguments() -> void;
  auto kitCMakeConfiguration() -> void;

  CMakeBuildConfiguration *m_buildConfiguration;
  QTreeView *m_configView;
  ConfigModel *m_configModel;
  CategorySortFilterModel *m_configFilterModel;
  CategorySortFilterModel *m_configTextFilterModel;
  ProgressIndicator *m_progressIndicator;
  QPushButton *m_addButton;
  QPushButton *m_editButton;
  QPushButton *m_setButton;
  QPushButton *m_unsetButton;
  QPushButton *m_resetButton;
  QCheckBox *m_showAdvancedCheckBox;
  QTabBar *m_configurationStates;
  QPushButton *m_reconfigureButton;
  QTimer m_showProgressTimer;
  FancyLineEdit *m_filterEdit;
  InfoLabel *m_warningMessageLabel;

  QPushButton *m_batchEditButton = nullptr;
  QPushButton *m_kitConfiguration = nullptr;
};

static auto mapToSource(const QAbstractItemView *view, const QModelIndex &idx) -> QModelIndex
{
  if (!idx.isValid())
    return idx;

  auto model = view->model();
  auto result = idx;
  while (auto proxy = qobject_cast<const QSortFilterProxyModel*>(model)) {
    result = proxy->mapToSource(result);
    model = proxy->sourceModel();
  }
  return result;
}

CMakeBuildSettingsWidget::CMakeBuildSettingsWidget(CMakeBuildConfiguration *bc) : NamedWidget(tr("CMake")), m_buildConfiguration(bc), m_configModel(new ConfigModel(this)), m_configFilterModel(new CategorySortFilterModel(this)), m_configTextFilterModel(new CategorySortFilterModel(this))
{
  QTC_CHECK(bc);

  auto vbox = new QVBoxLayout(this);
  vbox->setContentsMargins(0, 0, 0, 0);
  auto container = new DetailsWidget;
  container->setState(DetailsWidget::NoSummary);
  vbox->addWidget(container);

  auto details = new QWidget(container);
  container->setWidget(details);

  auto buildDirAspect = bc->buildDirectoryAspect();
  buildDirAspect->setAutoApplyOnEditingFinished(true);
  connect(buildDirAspect, &BaseAspect::changed, this, [this]() {
    m_configModel->flush(); // clear out config cache...;
  });

  auto buildTypeAspect = bc->aspect<BuildTypeAspect>();
  connect(buildTypeAspect, &BaseAspect::changed, this, [this, buildTypeAspect]() {
    if (!m_buildConfiguration->isMultiConfig()) {
      CMakeConfig config;
      config << CMakeConfigItem("CMAKE_BUILD_TYPE", buildTypeAspect->value().toUtf8());

      m_configModel->setBatchEditConfiguration(config);
    }
  });

  auto qmlDebugAspect = bc->aspect<QtSupport::QmlDebuggingAspect>();
  connect(qmlDebugAspect, &QtSupport::QmlDebuggingAspect::changed, this, [this]() {
    updateButtonState();
  });

  m_warningMessageLabel = new InfoLabel({}, InfoLabel::Warning);
  m_warningMessageLabel->setVisible(false);

  m_configurationStates = new QTabBar(this);
  m_configurationStates->addTab(tr("Initial Configuration"));
  m_configurationStates->addTab(tr("Current Configuration"));
  connect(m_configurationStates, &QTabBar::currentChanged, this, [this](int index) {
    updateConfigurationStateIndex(index);
  });

  m_kitConfiguration = new QPushButton(tr("Kit Configuration"));
  m_kitConfiguration->setToolTip(tr("Edit the current kit's CMake configuration."));
  m_kitConfiguration->setFixedWidth(m_kitConfiguration->sizeHint().width());
  connect(m_kitConfiguration, &QPushButton::clicked, this, [this]() { kitCMakeConfiguration(); });

  m_filterEdit = new FancyLineEdit;
  m_filterEdit->setPlaceholderText(tr("Filter"));
  m_filterEdit->setFiltering(true);
  auto tree = new TreeView;
  connect(tree, &TreeView::activated, tree, [tree](const QModelIndex &idx) { tree->edit(idx); });
  m_configView = tree;

  m_configView->viewport()->installEventFilter(this);

  m_configFilterModel->setSourceModel(m_configModel);
  m_configFilterModel->setFilterKeyColumn(0);
  m_configFilterModel->setFilterRole(ConfigModel::ItemIsAdvancedRole);
  m_configFilterModel->setFilterFixedString("0");

  m_configTextFilterModel->setSourceModel(m_configFilterModel);
  m_configTextFilterModel->setSortRole(Qt::DisplayRole);
  m_configTextFilterModel->setFilterKeyColumn(-1);

  connect(m_configTextFilterModel, &QAbstractItemModel::layoutChanged, this, [this]() {
    auto selectedIdx = m_configView->currentIndex();
    if (selectedIdx.isValid())
      m_configView->scrollTo(selectedIdx);
  });

  m_configView->setModel(m_configTextFilterModel);
  m_configView->setMinimumHeight(300);
  m_configView->setUniformRowHeights(true);
  m_configView->setSortingEnabled(true);
  m_configView->sortByColumn(0, Qt::AscendingOrder);
  (void)new HeaderViewStretcher(m_configView->header(), 0);
  m_configView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_configView->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_configView->setAlternatingRowColors(true);
  m_configView->setFrameShape(QFrame::NoFrame);
  m_configView->setItemDelegate(new ConfigModelItemDelegate(m_buildConfiguration->project()->projectDirectory(), m_configView));
  m_configView->setRootIsDecorated(false);
  auto findWrapper = Core::ItemViewFind::createSearchableWrapper(m_configView, Core::ItemViewFind::LightColored);
  findWrapper->setFrameStyle(QFrame::StyledPanel);

  m_progressIndicator = new ProgressIndicator(ProgressIndicatorSize::Large, findWrapper);
  m_progressIndicator->attachToWidget(findWrapper);
  m_progressIndicator->raise();
  m_progressIndicator->hide();
  m_showProgressTimer.setSingleShot(true);
  m_showProgressTimer.setInterval(50); // don't show progress for < 50ms tasks
  connect(&m_showProgressTimer, &QTimer::timeout, [this]() { m_progressIndicator->show(); });

  m_addButton = new QPushButton(tr("&Add"));
  m_addButton->setToolTip(tr("Add a new configuration value."));
  auto addButtonMenu = new QMenu(this);
  addButtonMenu->addAction(tr("&Boolean"))->setData(QVariant::fromValue(static_cast<int>(ConfigModel::DataItem::BOOLEAN)));
  addButtonMenu->addAction(tr("&String"))->setData(QVariant::fromValue(static_cast<int>(ConfigModel::DataItem::STRING)));
  addButtonMenu->addAction(tr("&Directory"))->setData(QVariant::fromValue(static_cast<int>(ConfigModel::DataItem::DIRECTORY)));
  addButtonMenu->addAction(tr("&File"))->setData(QVariant::fromValue(static_cast<int>(ConfigModel::DataItem::FILE)));
  m_addButton->setMenu(addButtonMenu);

  m_editButton = new QPushButton(tr("&Edit"));
  m_editButton->setToolTip(tr("Edit the current CMake configuration value."));

  m_setButton = new QPushButton(tr("&Set"));
  m_setButton->setToolTip(tr("Set a value in the CMake configuration."));

  m_unsetButton = new QPushButton(tr("&Unset"));
  m_unsetButton->setToolTip(tr("Unset a value in the CMake configuration."));

  m_resetButton = new QPushButton(tr("&Reset"));
  m_resetButton->setToolTip(tr("Reset all unapplied changes."));
  m_resetButton->setEnabled(false);

  m_batchEditButton = new QPushButton(tr("Batch Edit..."));
  m_batchEditButton->setToolTip(tr("Set or reset multiple values in the CMake configuration."));

  m_showAdvancedCheckBox = new QCheckBox(tr("Advanced"));

  connect(m_configView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &, const QItemSelection &) {
    updateSelection();
  });

  m_reconfigureButton = new QPushButton(tr("Run CMake"));
  m_reconfigureButton->setEnabled(false);

  using namespace Layouting;
  Grid cmakeConfiguration{m_filterEdit, Break(), findWrapper, Column{m_addButton, m_editButton, m_setButton, m_unsetButton, m_resetButton, m_batchEditButton, Space(10), m_showAdvancedCheckBox, Stretch()}};

  Column{Form{buildDirAspect, bc->aspect<BuildTypeAspect>(), qmlDebugAspect}, m_warningMessageLabel, m_kitConfiguration, Column{m_configurationStates, Group{cmakeConfiguration, Row{bc->aspect<InitialCMakeArgumentsAspect>(), bc->aspect<AdditionalCMakeOptionsAspect>()}, m_reconfigureButton,}}.setSpacing(0)}.attachTo(details, false);

  updateAdvancedCheckBox();
  setError(bc->error());
  setWarning(bc->warning());

  connect(bc->buildSystem(), &BuildSystem::parsingStarted, this, [this] {
    updateButtonState();
    m_configView->setEnabled(false);
    m_showProgressTimer.start();
  });

  m_configModel->setMacroExpander(m_buildConfiguration->macroExpander());

  if (bc->buildSystem()->isParsing())
    m_showProgressTimer.start();
  else {
    m_configModel->setConfiguration(m_buildConfiguration->configurationFromCMake());
    m_configModel->setInitialParametersConfiguration(m_buildConfiguration->initialCMakeConfiguration());
  }

  connect(bc->buildSystem(), &BuildSystem::parsingFinished, this, [this] {
    m_configModel->setConfiguration(m_buildConfiguration->configurationFromCMake());
    m_configModel->setInitialParametersConfiguration(m_buildConfiguration->initialCMakeConfiguration());
    m_buildConfiguration->filterConfigArgumentsFromAdditionalCMakeArguments();
    updateFromKit();
    m_configView->setEnabled(true);
    updateButtonState();
    m_showProgressTimer.stop();
    m_progressIndicator->hide();
    updateConfigurationStateSelection();
  });

  auto cbc = static_cast<CMakeBuildSystem*>(bc->buildSystem());
  connect(cbc, &CMakeBuildSystem::configurationCleared, this, [this]() {
    updateConfigurationStateSelection();
  });

  connect(m_buildConfiguration, &CMakeBuildConfiguration::errorOccurred, this, [this]() {
    m_showProgressTimer.stop();
    m_progressIndicator->hide();
    updateConfigurationStateSelection();
  });

  connect(m_configModel, &QAbstractItemModel::dataChanged, this, &CMakeBuildSettingsWidget::updateButtonState);
  connect(m_configModel, &QAbstractItemModel::modelReset, this, &CMakeBuildSettingsWidget::updateButtonState);

  connect(m_buildConfiguration, &CMakeBuildConfiguration::signingFlagsChanged, this, &CMakeBuildSettingsWidget::updateButtonState);

  connect(m_showAdvancedCheckBox, &QCheckBox::stateChanged, this, &CMakeBuildSettingsWidget::updateAdvancedCheckBox);

  connect(m_filterEdit, &QLineEdit::textChanged, m_configTextFilterModel, [this](const QString &txt) {
    m_configTextFilterModel->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(txt), QRegularExpression::CaseInsensitiveOption));
  });

  connect(m_resetButton, &QPushButton::clicked, this, [this]() {
    m_configModel->resetAllChanges(isInitialConfiguration());
  });
  connect(m_reconfigureButton, &QPushButton::clicked, this, [this, bc]() {
    auto buildSystem = static_cast<CMakeBuildSystem*>(m_buildConfiguration->buildSystem());
    if (!buildSystem->isParsing()) {
      if (isInitialConfiguration()) {
        reconfigureWithInitialParameters(bc);
      } else {
        buildSystem->runCMakeWithExtraArguments();
      }
    } else {
      buildSystem->stopCMakeRun();
      m_reconfigureButton->setEnabled(false);
    }
  });
  connect(m_setButton, &QPushButton::clicked, this, [this]() { setVariableUnsetFlag(false); });
  connect(m_unsetButton, &QPushButton::clicked, this, [this]() {
    setVariableUnsetFlag(true);
  });
  connect(m_editButton, &QPushButton::clicked, this, [this]() {
    auto idx = m_configView->currentIndex();
    if (idx.column() != 1)
      idx = idx.sibling(idx.row(), 1);
    m_configView->setCurrentIndex(idx);
    m_configView->edit(idx);
  });
  connect(addButtonMenu, &QMenu::triggered, this, [this](QAction *action) {
    auto type = static_cast<ConfigModel::DataItem::Type>(action->data().value<int>());
    auto value = tr("<UNSET>");
    if (type == ConfigModel::DataItem::BOOLEAN)
      value = QString::fromLatin1("OFF");

    m_configModel->appendConfiguration(tr("<UNSET>"), value, type, isInitialConfiguration());
    const TreeItem *item = m_configModel->findNonRootItem([&value, type](TreeItem *item) {
      auto dataItem = ConfigModel::dataItemFromIndex(item->index());
      return dataItem.key == tr("<UNSET>") && dataItem.type == type && dataItem.value == value;
    });
    auto idx = m_configModel->indexForItem(item);
    idx = m_configTextFilterModel->mapFromSource(m_configFilterModel->mapFromSource(idx));
    m_configView->setFocus();
    m_configView->scrollTo(idx);
    m_configView->setCurrentIndex(idx);
    m_configView->edit(idx);
  });
  connect(m_batchEditButton, &QAbstractButton::clicked, this, &CMakeBuildSettingsWidget::batchEditConfiguration);

  connect(bc, &CMakeBuildConfiguration::errorOccurred, this, &CMakeBuildSettingsWidget::setError);
  connect(bc, &CMakeBuildConfiguration::warningOccurred, this, &CMakeBuildSettingsWidget::setWarning);
  connect(bc, &CMakeBuildConfiguration::configurationChanged, this, [this](const CMakeConfig &config) {
    m_configModel->setBatchEditConfiguration(config);
  });

  updateFromKit();
  connect(m_buildConfiguration->target(), &Target::kitChanged, this, &CMakeBuildSettingsWidget::updateFromKit);
  connect(m_buildConfiguration, &CMakeBuildConfiguration::enabledChanged, this, [this]() {
    if (m_buildConfiguration->isEnabled())
      setError(QString());
  });
  connect(this, &QObject::destroyed, this, [this] {
    updateInitialCMakeArguments();
  });

  connect(bc->aspect<InitialCMakeArgumentsAspect>(), &Utils::BaseAspect::labelLinkActivated, this, [this](const QString &) {
    const CMakeTool *tool = CMakeKitAspect::cmakeTool(m_buildConfiguration->target()->kit());
    CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake.1.html#options");
  });
  connect(bc->aspect<AdditionalCMakeOptionsAspect>(), &Utils::BaseAspect::labelLinkActivated, this, [this](const QString &) {
    const CMakeTool *tool = CMakeKitAspect::cmakeTool(m_buildConfiguration->target()->kit());
    CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake.1.html#options");
  });

  updateSelection();
  updateConfigurationStateSelection();
}

auto CMakeBuildSettingsWidget::batchEditConfiguration() -> void
{
  auto dialog = new QDialog(this);
  dialog->setWindowTitle(tr("Edit CMake Configuration"));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setModal(true);
  auto layout = new QVBoxLayout(dialog);
  auto editor = new QPlainTextEdit(dialog);

  auto label = new QLabel(dialog);
  label->setText(tr("Enter one CMake <a href=\"variable\">variable</a> per line.<br/>" "To set or change a variable, use -D&lt;variable&gt;:&lt;type&gt;=&lt;value&gt;.<br/>" "&lt;type&gt; can have one of the following values: FILEPATH, PATH, BOOL, INTERNAL, or STRING.<br/>" "To unset a variable, use -U&lt;variable&gt;.<br/>"));
  connect(label, &QLabel::linkActivated, this, [this](const QString &) {
    const CMakeTool *tool = CMakeKitAspect::cmakeTool(m_buildConfiguration->target()->kit());
    CMakeTool::openCMakeHelpUrl(tool, "%1/manual/cmake-variables.7.html");
  });
  editor->setMinimumSize(800, 200);

  auto chooser = new Utils::VariableChooser(dialog);
  chooser->addSupportedWidget(editor);
  chooser->addMacroExpanderProvider([this]() { return m_buildConfiguration->macroExpander(); });

  auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  layout->addWidget(editor);
  layout->addWidget(label);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
  connect(dialog, &QDialog::accepted, this, [=] {
    const auto expander = m_buildConfiguration->macroExpander();

    const auto lines = editor->toPlainText().split('\n', Qt::SkipEmptyParts);
    const auto expandedLines = Utils::transform(lines, [expander](const QString &s) {
      return expander->expand(s);
    });
    const auto isInitial = isInitialConfiguration();
    QStringList unknownOptions;
    auto config = CMakeConfig::fromArguments(isInitial ? lines : expandedLines, unknownOptions);
    for (auto &ci : config)
      ci.isInitial = isInitial;

    m_configModel->setBatchEditConfiguration(config);
  });

  editor->setPlainText(m_buildConfiguration->configurationChangesArguments(isInitialConfiguration()).join('\n'));

  dialog->show();
}

auto CMakeBuildSettingsWidget::reconfigureWithInitialParameters(CMakeBuildConfiguration *bc) -> void
{
  auto settings = CMakeProjectPlugin::projectTypeSpecificSettings();
  auto doNotAsk = !settings->askBeforeReConfigureInitialParams.value();
  if (!doNotAsk) {
    auto reply = Utils::CheckableMessageBox::question(Core::ICore::dialogParent(), tr("Re-configure with Initial Parameters"), tr("Clear CMake configuration and configure with initial parameters?"), tr("Do not ask again"), &doNotAsk, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes);

    settings->askBeforeReConfigureInitialParams.setValue(!doNotAsk);
    settings->writeSettings(Core::ICore::settings());

    if (reply != QDialogButtonBox::Yes) {
      return;
    }
  }

  auto cbc = static_cast<CMakeBuildSystem*>(bc->buildSystem());
  cbc->clearCMakeCache();

  updateInitialCMakeArguments();

  if (ProjectExplorerPlugin::saveModifiedFiles())
    cbc->runCMake();
}

auto CMakeBuildSettingsWidget::updateInitialCMakeArguments() -> void
{
  auto initialList = m_buildConfiguration->initialCMakeConfiguration();

  for (const auto &ci : m_buildConfiguration->configurationChanges()) {
    if (!ci.isInitial)
      continue;
    auto it = std::find_if(initialList.begin(), initialList.end(), [ci](const CMakeConfigItem &item) {
      return item.key == ci.key;
    });
    if (it != initialList.end()) {
      *it = ci;
      if (ci.isUnset)
        initialList.erase(it);
    } else if (!ci.key.isEmpty()) {
      initialList.push_back(ci);
    }
  }

  m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->setCMakeConfiguration(initialList);

  // value() will contain only the unknown arguments (the non -D/-U arguments)
  // As the user would expect to have e.g. "--preset" from "Initial Configuration"
  // to "Current Configuration" as additional parameters
  m_buildConfiguration->setAdditionalCMakeArguments(ProcessArgs::splitArgs(m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->value()));
}

auto CMakeBuildSettingsWidget::kitCMakeConfiguration() -> void
{
  m_buildConfiguration->kit()->blockNotification();

  auto dialog = new QDialog(this);
  dialog->setWindowTitle(tr("Kit CMake Configuration"));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setModal(true);
  dialog->setSizeGripEnabled(true);
  connect(dialog, &QDialog::finished, this, [=] {
    m_buildConfiguration->kit()->unblockNotification();
  });

  CMakeKitAspect kitAspect;
  CMakeGeneratorKitAspect generatorAspect;
  CMakeConfigurationKitAspect configurationKitAspect;

  auto layout = new QGridLayout(dialog);

  kitAspect.createConfigWidget(m_buildConfiguration->kit())->addToLayoutWithLabel(layout->parentWidget());
  generatorAspect.createConfigWidget(m_buildConfiguration->kit())->addToLayoutWithLabel(layout->parentWidget());
  configurationKitAspect.createConfigWidget(m_buildConfiguration->kit())->addToLayoutWithLabel(layout->parentWidget());

  layout->setColumnStretch(1, 1);

  auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::clicked, dialog, &QDialog::close);
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::MinimumExpanding), 4, 0);
  layout->addWidget(buttons, 5, 0, 1, -1);

  dialog->setMinimumWidth(400);
  dialog->resize(800, 1);
  dialog->show();
}

auto CMakeBuildSettingsWidget::setError(const QString &message) -> void
{
  m_buildConfiguration->buildDirectoryAspect()->setProblem(message);
}

auto CMakeBuildSettingsWidget::setWarning(const QString &message) -> void
{
  auto showWarning = !message.isEmpty();
  m_warningMessageLabel->setVisible(showWarning);
  m_warningMessageLabel->setText(message);
}

auto CMakeBuildSettingsWidget::updateButtonState() -> void
{
  const auto isParsing = m_buildConfiguration->buildSystem()->isParsing();

  // Update extra data in buildconfiguration
  const auto changes = m_configModel->configurationForCMake();

  const CMakeConfig configChanges = getQmlDebugCxxFlags() + getSigningFlagsChanges() + Utils::transform(changes, [](const ConfigModel::DataItem &i) {
    CMakeConfigItem ni;
    ni.key = i.key.toUtf8();
    ni.value = i.value.toUtf8();
    ni.documentation = i.description.toUtf8();
    ni.isAdvanced = i.isAdvanced;
    ni.isInitial = i.isInitial;
    ni.isUnset = i.isUnset;
    ni.inCMakeCache = i.inCMakeCache;
    ni.values = i.values;
    switch (i.type) {
    case ConfigModel::DataItem::BOOLEAN:
      ni.type = CMakeConfigItem::BOOL;
      break;
    case ConfigModel::DataItem::FILE:
      ni.type = CMakeConfigItem::FILEPATH;
      break;
    case ConfigModel::DataItem::DIRECTORY:
      ni.type = CMakeConfigItem::PATH;
      break;
    case ConfigModel::DataItem::STRING:
      ni.type = CMakeConfigItem::STRING;
      break;
    case ConfigModel::DataItem::UNKNOWN: default:
      ni.type = CMakeConfigItem::UNINITIALIZED;
      break;
    }
    return ni;
  });

  const auto isInitial = isInitialConfiguration();
  m_resetButton->setEnabled(m_configModel->hasChanges(isInitial) && !isParsing);

  m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->setVisible(isInitialConfiguration());
  m_buildConfiguration->aspect<AdditionalCMakeOptionsAspect>()->setVisible(!isInitialConfiguration());

  m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->setEnabled(!isParsing);
  m_buildConfiguration->aspect<AdditionalCMakeOptionsAspect>()->setEnabled(!isParsing);

  // Update label and text boldness of the reconfigure button
  auto reconfigureButtonFont = m_reconfigureButton->font();
  if (isParsing) {
    m_reconfigureButton->setText(tr("Stop CMake"));
    reconfigureButtonFont.setBold(false);
  } else {
    m_reconfigureButton->setEnabled(true);
    if (isInitial) {
      m_reconfigureButton->setText(tr("Re-configure with Initial Parameters"));
    } else {
      m_reconfigureButton->setText(tr("Run CMake"));
    }
    reconfigureButtonFont.setBold(m_configModel->hasChanges(isInitial));
  }
  m_reconfigureButton->setFont(reconfigureButtonFont);

  m_buildConfiguration->setConfigurationChanges(configChanges);

  // Update the tooltip with the changes
  m_reconfigureButton->setToolTip(m_buildConfiguration->configurationChangesArguments(isInitialConfiguration()).join('\n'));
}

auto CMakeBuildSettingsWidget::updateAdvancedCheckBox() -> void
{
  if (m_showAdvancedCheckBox->isChecked()) {
    m_configFilterModel->setFilterRole(ConfigModel::ItemIsAdvancedRole);
    m_configFilterModel->setFilterRegularExpression("[01]");

  } else {
    m_configFilterModel->setFilterRole(ConfigModel::ItemIsAdvancedRole);
    m_configFilterModel->setFilterFixedString("0");
  }
  updateButtonState();
}

auto CMakeBuildSettingsWidget::updateFromKit() -> void
{
  const Kit *k = m_buildConfiguration->kit();
  auto config = CMakeConfigurationKitAspect::configuration(k);

  config.append(CMakeGeneratorKitAspect::generatorCMakeConfig(k));

  // First the key value parameters
  ConfigModel::KitConfiguration configHash;
  for (const auto &i : config)
    configHash.insert(QString::fromUtf8(i.key), i);

  m_configModel->setConfigurationFromKit(configHash);

  // Then the additional parameters
  const auto additionalKitCMake = ProcessArgs::splitArgs(CMakeConfigurationKitAspect::additionalConfiguration(k));
  const auto additionalInitialCMake = ProcessArgs::splitArgs(m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->value());

  QStringList mergedArgumentList;
  std::set_union(additionalInitialCMake.begin(), additionalInitialCMake.end(), additionalKitCMake.begin(), additionalKitCMake.end(), std::back_inserter(mergedArgumentList));
  m_buildConfiguration->aspect<InitialCMakeArgumentsAspect>()->setValue(ProcessArgs::joinArgs(mergedArgumentList));
}

auto CMakeBuildSettingsWidget::updateConfigurationStateIndex(int index) -> void
{
  if (index == 0) {
    m_configFilterModel->setFilterRole(ConfigModel::ItemIsInitialRole);
    m_configFilterModel->setFilterFixedString("1");
  } else {
    updateAdvancedCheckBox();
  }

  m_showAdvancedCheckBox->setEnabled(index != 0);

  updateButtonState();
}

auto CMakeBuildSettingsWidget::getQmlDebugCxxFlags() -> CMakeConfig
{
  const auto aspect = m_buildConfiguration->aspect<QtSupport::QmlDebuggingAspect>();
  const auto qmlDebuggingState = aspect->value();
  if (qmlDebuggingState == TriState::Default) // don't touch anything
    return {};
  const auto enable = aspect->value() == TriState::Enabled;

  const auto configList = m_buildConfiguration->configurationFromCMake();
  const QByteArrayList cxxFlags{"CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_DEBUG", "CMAKE_CXX_FLAGS_RELWITHDEBINFO"};
  const QByteArray qmlDebug("-DQT_QML_DEBUG");

  CMakeConfig changedConfig;

  for (const auto &item : configList) {
    if (!cxxFlags.contains(item.key))
      continue;

    auto it(item);
    if (enable) {
      if (!it.value.contains(qmlDebug)) {
        it.value = it.value.append(' ').append(qmlDebug).trimmed();
        changedConfig.append(it);
      }
    } else {
      int index = it.value.indexOf(qmlDebug);
      if (index != -1) {
        it.value.remove(index, qmlDebug.length());
        it.value = it.value.trimmed();
        changedConfig.append(it);
      }
    }
  }
  return changedConfig;
}

auto CMakeBuildSettingsWidget::getSigningFlagsChanges() -> CMakeConfig
{
  const auto flags = m_buildConfiguration->signingFlags();
  if (flags.isEmpty())
    return {};
  const auto configList = m_buildConfiguration->configurationFromCMake();
  if (configList.isEmpty()) {
    // we don't have any configuration --> initial configuration takes care of this itself
    return {};
  }
  CMakeConfig changedConfig;
  for (const auto &signingFlag : flags) {
    const auto existingFlag = Utils::findOrDefault(configList, Utils::equal(&CMakeConfigItem::key, signingFlag.key));
    const auto notInConfig = existingFlag.key.isEmpty();
    if (notInConfig != signingFlag.isUnset || existingFlag.value != signingFlag.value)
      changedConfig.append(signingFlag);
  }
  return changedConfig;
}

auto CMakeBuildSettingsWidget::updateSelection() -> void
{
  const auto selectedIndexes = m_configView->selectionModel()->selectedIndexes();
  unsigned int setableCount = 0;
  unsigned int unsetableCount = 0;
  unsigned int editableCount = 0;

  for (const auto &index : selectedIndexes) {
    if (index.isValid() && index.flags().testFlag(Qt::ItemIsSelectable)) {
      const auto di = ConfigModel::dataItemFromIndex(index);
      if (di.isUnset)
        setableCount++;
      else
        unsetableCount++;
    }
    if (index.isValid() && index.flags().testFlag(Qt::ItemIsEditable))
      editableCount++;
  }

  m_setButton->setEnabled(setableCount > 0);
  m_unsetButton->setEnabled(unsetableCount > 0);
  m_editButton->setEnabled(editableCount == 1);
}

auto CMakeBuildSettingsWidget::updateConfigurationStateSelection() -> void
{
  const auto hasReplyFile = FileApiParser::scanForCMakeReplyFile(m_buildConfiguration->buildDirectory()).exists();

  const auto switchToIndex = hasReplyFile ? 1 : 0;
  if (m_configurationStates->currentIndex() != switchToIndex)
    m_configurationStates->setCurrentIndex(switchToIndex);
  else emit m_configurationStates->currentChanged(switchToIndex);
}

auto CMakeBuildSettingsWidget::isInitialConfiguration() const -> bool
{
  return m_configurationStates->currentIndex() == 0;
}

auto CMakeBuildSettingsWidget::setVariableUnsetFlag(bool unsetFlag) -> void
{
  const auto selectedIndexes = m_configView->selectionModel()->selectedIndexes();
  auto unsetFlagToggled = false;
  for (const auto &index : selectedIndexes) {
    if (index.isValid()) {
      const auto di = ConfigModel::dataItemFromIndex(index);
      if (di.isUnset != unsetFlag) {
        m_configModel->toggleUnsetFlag(mapToSource(m_configView, index));
        unsetFlagToggled = true;
      }
    }
  }

  if (unsetFlagToggled)
    updateSelection();
}

auto CMakeBuildSettingsWidget::createForceAction(int type, const QModelIndex &idx) -> QAction*
{
  auto t = static_cast<ConfigModel::DataItem::Type>(type);
  QString typeString;
  switch (type) {
  case ConfigModel::DataItem::BOOLEAN:
    typeString = tr("bool", "display string for cmake type BOOLEAN");
    break;
  case ConfigModel::DataItem::FILE:
    typeString = tr("file", "display string for cmake type FILE");
    break;
  case ConfigModel::DataItem::DIRECTORY:
    typeString = tr("directory", "display string for cmake type DIRECTORY");
    break;
  case ConfigModel::DataItem::STRING:
    typeString = tr("string", "display string for cmake type STRING");
    break;
  case ConfigModel::DataItem::UNKNOWN:
    return nullptr;
  }
  auto forceAction = new QAction(tr("Force to %1").arg(typeString), nullptr);
  forceAction->setEnabled(m_configModel->canForceTo(idx, t));
  connect(forceAction, &QAction::triggered, this, [this, idx, t]() { m_configModel->forceTo(idx, t); });
  return forceAction;
}

auto CMakeBuildSettingsWidget::eventFilter(QObject *target, QEvent *event) -> bool
{
  // handle context menu events:
  if (target != m_configView->viewport() || event->type() != QEvent::ContextMenu)
    return false;

  auto e = static_cast<QContextMenuEvent*>(event);
  const auto idx = mapToSource(m_configView, m_configView->indexAt(e->pos()));
  if (!idx.isValid())
    return false;

  auto menu = new QMenu(this);
  connect(menu, &QMenu::triggered, menu, &QMenu::deleteLater);

  auto help = new QAction(tr("Help"), this);
  menu->addAction(help);
  connect(help, &QAction::triggered, this, [=] {
    const auto item = ConfigModel::dataItemFromIndex(idx).toCMakeConfigItem();

    const CMakeTool *tool = CMakeKitAspect::cmakeTool(m_buildConfiguration->target()->kit());
    const QString linkUrl = "%1/variable/" + QString::fromUtf8(item.key) + ".html";
    CMakeTool::openCMakeHelpUrl(tool, linkUrl);
  });

  menu->addSeparator();

  QAction *action = nullptr;
  if ((action = createForceAction(ConfigModel::DataItem::BOOLEAN, idx)))
    menu->addAction(action);
  if ((action = createForceAction(ConfigModel::DataItem::FILE, idx)))
    menu->addAction(action);
  if ((action = createForceAction(ConfigModel::DataItem::DIRECTORY, idx)))
    menu->addAction(action);
  if ((action = createForceAction(ConfigModel::DataItem::STRING, idx)))
    menu->addAction(action);

  menu->addSeparator();

  auto applyKitOrInitialValue = new QAction(isInitialConfiguration() ? tr("Apply Kit Value") : tr("Apply Initial Configuration Value"), this);
  menu->addAction(applyKitOrInitialValue);
  connect(applyKitOrInitialValue, &QAction::triggered, this, [this] {
    const auto selectedIndexes = m_configView->selectionModel()->selectedIndexes();

    const auto validIndexes = Utils::filtered(selectedIndexes, [](const QModelIndex &index) {
      return index.isValid() && index.flags().testFlag(Qt::ItemIsSelectable);
    });

    for (const auto &index : validIndexes) {
      if (isInitialConfiguration())
        m_configModel->applyKitValue(mapToSource(m_configView, index));
      else
        m_configModel->applyInitialValue(mapToSource(m_configView, index));
    }
  });

  menu->addSeparator();

  auto copy = new QAction(tr("Copy"), this);
  menu->addAction(copy);
  connect(copy, &QAction::triggered, this, [this] {
    const auto selectedIndexes = m_configView->selectionModel()->selectedIndexes();

    const auto validIndexes = Utils::filtered(selectedIndexes, [](const QModelIndex &index) {
      return index.isValid() && index.flags().testFlag(Qt::ItemIsSelectable);
    });

    const auto variableList = Utils::transform(validIndexes, [this](const QModelIndex &index) {
      return ConfigModel::dataItemFromIndex(index).toCMakeConfigItem().toArgument(isInitialConfiguration() ? nullptr : m_buildConfiguration->macroExpander());
    });

    QApplication::clipboard()->setText(variableList.join('\n'), QClipboard::Clipboard);
  });

  menu->move(e->globalPos());
  menu->show();

  return true;
}

static auto isIos(const Kit *k) -> bool
{
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);
  return deviceType == Ios::Constants::IOS_DEVICE_TYPE || deviceType == Ios::Constants::IOS_SIMULATOR_TYPE;
}

static auto isWebAssembly(const Kit *k) -> bool
{
  return DeviceTypeKitAspect::deviceTypeId(k) == WebAssembly::Constants::WEBASSEMBLY_DEVICE_TYPE;
}

static auto isQnx(const Kit *k) -> bool
{
  return DeviceTypeKitAspect::deviceTypeId(k) == Qnx::Constants::QNX_QNX_OS_TYPE;
}

static auto isDocker(const Kit *k) -> bool
{
  return DeviceTypeKitAspect::deviceTypeId(k) == Docker::Constants::DOCKER_DEVICE_TYPE;
}

static auto isWindowsARM64(const Kit *k) -> bool
{
  auto toolchain = ToolChainKitAspect::cxxToolChain(k);
  if (!toolchain)
    return false;
  const auto targetAbi = toolchain->targetAbi();
  return targetAbi.os() == Abi::WindowsOS && targetAbi.architecture() == Abi::ArmArchitecture && targetAbi.wordWidth() == 64;
}

static auto defaultInitialCMakeCommand(const Kit *k, const QString buildType) -> CommandLine
{
  // Generator:
  auto tool = CMakeKitAspect::cmakeTool(k);
  QTC_ASSERT(tool, return {});

  CommandLine cmd{tool->cmakeExecutable()};
  cmd.addArgs(CMakeGeneratorKitAspect::generatorArguments(k));

  // CMAKE_BUILD_TYPE:
  if (!buildType.isEmpty() && !CMakeGeneratorKitAspect::isMultiConfigGenerator(k))
    cmd.addArg("-DCMAKE_BUILD_TYPE:STRING=" + buildType);

  auto settings = Internal::CMakeProjectPlugin::projectTypeSpecificSettings();

  // Package manager
  if (!isDocker(k) && settings->packageManagerAutoSetup.value()) {
    cmd.addArg("-DCMAKE_PROJECT_INCLUDE_BEFORE:FILEPATH=" "%{IDE:ResourcePath}/package-manager/auto-setup.cmake");
  }

  // Cross-compilation settings:
  if (!isIos(k)) {
    // iOS handles this differently
    const auto sysRoot = SysRootKitAspect::sysRoot(k).path();
    if (!sysRoot.isEmpty()) {
      cmd.addArg("-DCMAKE_SYSROOT:PATH=" + sysRoot);
      if (auto tc = ToolChainKitAspect::cxxToolChain(k)) {
        const auto targetTriple = tc->originalTargetTriple();
        cmd.addArg("-DCMAKE_C_COMPILER_TARGET:STRING=" + targetTriple);
        cmd.addArg("-DCMAKE_CXX_COMPILER_TARGET:STRING=" + targetTriple);
      }
    }
  }

  cmd.addArgs(CMakeConfigurationKitAspect::toArgumentsList(k));
  cmd.addArgs(CMakeConfigurationKitAspect::additionalConfiguration(k), CommandLine::Raw);

  return cmd;
}

} // namespace Internal

// -----------------------------------------------------------------------------
// CMakeBuildConfiguration:
// -----------------------------------------------------------------------------

CMakeBuildConfiguration::CMakeBuildConfiguration(Target *target, Id id) : BuildConfiguration(target, id)
{
  m_buildSystem = new CMakeBuildSystem(this);

  const auto buildDirAspect = aspect<BuildDirectoryAspect>();
  buildDirAspect->setValueAcceptor([](const QString &oldDir, const QString &newDir) -> Utils::optional<QString> {
    if (oldDir.isEmpty())
      return newDir;

    if (QDir(oldDir).exists("CMakeCache.txt") && !QDir(newDir).exists("CMakeCache.txt")) {
      if (QMessageBox::information(Core::ICore::dialogParent(), tr("Changing Build Directory"), tr("Change the build directory to \"%1\" and start with a " "basic CMake configuration?").arg(newDir), QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok) {
        return newDir;
      }
      return Utils::nullopt;
    }
    return newDir;
  });

  auto initialCMakeArgumentsAspect = addAspect<InitialCMakeArgumentsAspect>();
  initialCMakeArgumentsAspect->setMacroExpanderProvider([this] { return macroExpander(); });

  auto additionalCMakeArgumentsAspect = addAspect<AdditionalCMakeOptionsAspect>();
  additionalCMakeArgumentsAspect->setMacroExpanderProvider([this] { return macroExpander(); });

  macroExpander()->registerVariable(DEVELOPMENT_TEAM_FLAG, tr("The CMake flag for the development team"), [this] {
    const auto flags = signingFlags();
    if (!flags.isEmpty())
      return flags.first().toArgument();
    return QString();
  });
  macroExpander()->registerVariable(PROVISIONING_PROFILE_FLAG, tr("The CMake flag for the provisioning profile"), [this] {
    const auto flags = signingFlags();
    if (flags.size() > 1 && !flags.at(1).isUnset) {
      return flags.at(1).toArgument();
    }
    return QString();
  });

  macroExpander()->registerVariable(CMAKE_OSX_ARCHITECTURES_FLAG, tr("The CMake flag for the architecture on macOS"), [target] {
    if (HostOsInfo::isRunningUnderRosetta()) {
      if (auto *qt = QtSupport::QtKitAspect::qtVersion(target->kit())) {
        const auto abis = qt->qtAbis();
        for (const auto &abi : abis) {
          if (abi.architecture() == Abi::ArmArchitecture)
            return QLatin1String("-DCMAKE_OSX_ARCHITECTURES=arm64");
        }
      }
    }
    return QLatin1String();
  });

  addAspect<SourceDirectoryAspect>();
  addAspect<BuildTypeAspect>();

  appendInitialBuildStep(Constants::CMAKE_BUILD_STEP_ID);
  appendInitialCleanStep(Constants::CMAKE_BUILD_STEP_ID);

  setInitializer([this, target](const BuildInfo &info) {
    const Kit *k = target->kit();

    auto cmd = defaultInitialCMakeCommand(k, info.typeName);
    setIsMultiConfig(CMakeGeneratorKitAspect::isMultiConfigGenerator(k));

    // Android magic:
    if (DeviceTypeKitAspect::deviceTypeId(k) == Android::Constants::ANDROID_DEVICE_TYPE) {
      buildSteps()->appendStep(Android::Constants::ANDROID_BUILD_APK_ID);
      const auto &bs = buildSteps()->steps().constLast();
      cmd.addArg("-DANDROID_NATIVE_API_LEVEL:STRING=" + bs->data(Android::Constants::AndroidNdkPlatform).toString());
      auto ndkLocation = bs->data(Android::Constants::NdkLocation).value<FilePath>();
      cmd.addArg("-DANDROID_NDK:PATH=" + ndkLocation.path());

      cmd.addArg("-DCMAKE_TOOLCHAIN_FILE:FILEPATH=" + ndkLocation.pathAppended("build/cmake/android.toolchain.cmake").path());

      auto androidAbis = bs->data(Android::Constants::AndroidMkSpecAbis).toStringList();
      QString preferredAbi;
      if (androidAbis.contains(ProjectExplorer::Constants::ANDROID_ABI_ARMEABI_V7A)) {
        preferredAbi = ProjectExplorer::Constants::ANDROID_ABI_ARMEABI_V7A;
      } else if (androidAbis.isEmpty() || androidAbis.contains(ProjectExplorer::Constants::ANDROID_ABI_ARM64_V8A)) {
        preferredAbi = ProjectExplorer::Constants::ANDROID_ABI_ARM64_V8A;
      } else {
        preferredAbi = androidAbis.first();
      }
      cmd.addArg("-DANDROID_ABI:STRING=" + preferredAbi);
      cmd.addArg("-DANDROID_STL:STRING=c++_shared");
      cmd.addArg("-DCMAKE_FIND_ROOT_PATH:PATH=%{Qt:QT_INSTALL_PREFIX}");

      auto qt = QtSupport::QtKitAspect::qtVersion(k);
      auto sdkLocation = bs->data(Android::Constants::SdkLocation).value<FilePath>();

      if (qt && qt->qtVersion() >= QtSupport::QtVersionNumber{6, 0, 0}) {
        // Don't build apk under ALL target because Qt Creator will handle it
        if (qt->qtVersion() >= QtSupport::QtVersionNumber{6, 1, 0})
          cmd.addArg("-DQT_NO_GLOBAL_APK_TARGET_PART_OF_ALL:BOOL=ON");
        cmd.addArg("-DQT_HOST_PATH:PATH=%{Qt:QT_HOST_PREFIX}");
        cmd.addArg("-DANDROID_SDK_ROOT:PATH=" + sdkLocation.path());
      } else {
        cmd.addArg("-DANDROID_SDK:PATH=" + sdkLocation.path());
      }
    }

    const auto device = DeviceKitAspect::device(k);
    if (isIos(k)) {
      auto qt = QtSupport::QtKitAspect::qtVersion(k);
      if (qt && qt->qtVersion().majorVersion >= 6) {
        // TODO it would be better if we could set
        // CMAKE_SYSTEM_NAME=iOS and CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=YES
        // and build with "cmake --build . -- -arch <arch>" instead of setting the architecture
        // and sysroot in the CMake configuration, but that currently doesn't work with Qt/CMake
        // https://gitlab.kitware.com/cmake/cmake/-/issues/21276
        const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);
        // TODO the architectures are probably not correct with Apple Silicon in the mix...
        const QString architecture = deviceType == Ios::Constants::IOS_DEVICE_TYPE ? QLatin1String("arm64") : QLatin1String("x86_64");
        const QString sysroot = deviceType == Ios::Constants::IOS_DEVICE_TYPE ? QLatin1String("iphoneos") : QLatin1String("iphonesimulator");
        cmd.addArg(CMAKE_QT6_TOOLCHAIN_FILE_ARG);
        cmd.addArg("-DCMAKE_OSX_ARCHITECTURES:STRING=" + architecture);
        cmd.addArg("-DCMAKE_OSX_SYSROOT:STRING=" + sysroot);
        cmd.addArg("%{" + QLatin1String(DEVELOPMENT_TEAM_FLAG) + "}");
        cmd.addArg("%{" + QLatin1String(PROVISIONING_PROFILE_FLAG) + "}");
      }
    } else if (device && device->osType() == Utils::OsTypeMac) {
      cmd.addArg("%{" + QLatin1String(CMAKE_OSX_ARCHITECTURES_FLAG) + "}");
    }

    if (isWebAssembly(k) || isQnx(k) || isWindowsARM64(k)) {
      const QtSupport::QtVersion *qt = QtSupport::QtKitAspect::qtVersion(k);
      if (qt && qt->qtVersion().majorVersion >= 6)
        cmd.addArg(CMAKE_QT6_TOOLCHAIN_FILE_ARG);
    }

    if (info.buildDirectory.isEmpty()) {
      setBuildDirectory(shadowBuildDirectory(target->project()->projectFilePath(), k, info.displayName, info.buildType));
    }

    if (info.extraInfo.isValid()) {
      setSourceDirectory(FilePath::fromVariant(info.extraInfo.value<QVariantMap>().value(Constants::CMAKE_HOME_DIR)));
    }

    setInitialCMakeArguments(cmd.splitArguments());
    setCMakeBuildType(info.typeName);
  });

  const auto qmlDebuggingAspect = addAspect<QtSupport::QmlDebuggingAspect>();
  qmlDebuggingAspect->setKit(target->kit());
  setIsMultiConfig(CMakeGeneratorKitAspect::isMultiConfigGenerator(target->kit()));
}

CMakeBuildConfiguration::~CMakeBuildConfiguration()
{
  delete m_buildSystem;
}

auto CMakeBuildConfiguration::toMap() const -> QVariantMap
{
  auto map(BuildConfiguration::toMap());
  return map;
}

auto CMakeBuildConfiguration::fromMap(const QVariantMap &map) -> bool
{
  if (!BuildConfiguration::fromMap(map))
    return false;

  const CMakeConfig conf = Utils::filtered(Utils::transform(map.value(QLatin1String(CONFIGURATION_KEY)).toStringList(), [](const QString &v) { return CMakeConfigItem::fromString(v); }), [](const CMakeConfigItem &c) { return !c.isNull(); });

  // TODO: Upgrade from Qt Creator < 4.13: Remove when no longer supported!
  const auto buildTypeName = [this]() {
    switch (buildType()) {
    case Debug:
      return QString("Debug");
    case Profile:
      return QString("RelWithDebInfo");
    case Release:
      return QString("Release");
    case Unknown: default:
      return QString("");
    }
  }();
  if (initialCMakeArguments().isEmpty()) {
    auto cmd = defaultInitialCMakeCommand(kit(), buildTypeName);
    for (const auto &item : conf)
      cmd.addArg(item.toArgument(macroExpander()));
    setInitialCMakeArguments(cmd.splitArguments());
  }

  return true;
}

auto CMakeBuildConfiguration::shadowBuildDirectory(const FilePath &projectFilePath, const Kit *k, const QString &bcName, BuildConfiguration::BuildType buildType) -> FilePath
{
  if (projectFilePath.isEmpty())
    return FilePath();

  const auto projectName = projectFilePath.parentDir().fileName();
  const auto projectDir = Project::projectDirectory(projectFilePath);
  auto buildPath = BuildConfiguration::buildDirectoryFromTemplate(projectDir, projectFilePath, projectName, k, bcName, buildType, BuildConfiguration::ReplaceSpaces);

  if (CMakeGeneratorKitAspect::isMultiConfigGenerator(k)) {
    auto path = buildPath.path();
    path = path.left(path.lastIndexOf(QString("-%1").arg(bcName)));
    buildPath.setPath(path);
  }

  return buildPath;
}

auto CMakeBuildConfiguration::buildTarget(const QString &buildTarget) -> void
{
  auto cmBs = qobject_cast<CMakeBuildStep*>(findOrDefault(buildSteps()->steps(), [](const BuildStep *bs) {
    return bs->id() == Constants::CMAKE_BUILD_STEP_ID;
  }));

  QStringList originalBuildTargets;
  if (cmBs) {
    originalBuildTargets = cmBs->buildTargets();
    cmBs->setBuildTargets({buildTarget});
  }

  BuildManager::buildList(buildSteps());

  if (cmBs)
    cmBs->setBuildTargets(originalBuildTargets);
}

auto CMakeBuildConfiguration::configurationFromCMake() const -> CMakeConfig
{
  return m_configurationFromCMake;
}

auto CMakeBuildConfiguration::configurationChanges() const -> CMakeConfig
{
  return m_configurationChanges;
}

auto CMakeBuildConfiguration::configurationChangesArguments(bool initialParameters) const -> QStringList
{
  const QList<CMakeConfigItem> filteredInitials = Utils::filtered(m_configurationChanges, [initialParameters](const CMakeConfigItem &ci) {
    return initialParameters ? ci.isInitial : !ci.isInitial;
  });
  return Utils::transform(filteredInitials, &CMakeConfigItem::toArgument);
}

auto CMakeBuildConfiguration::initialCMakeArguments() const -> QStringList
{
  return aspect<InitialCMakeArgumentsAspect>()->allValues();
}

auto CMakeBuildConfiguration::initialCMakeConfiguration() const -> CMakeConfig
{
  return aspect<InitialCMakeArgumentsAspect>()->cmakeConfiguration();
}

auto CMakeBuildConfiguration::setConfigurationFromCMake(const CMakeConfig &config) -> void
{
  m_configurationFromCMake = config;
}

auto CMakeBuildConfiguration::setConfigurationChanges(const CMakeConfig &config) -> void
{
  qCDebug(cmakeBuildConfigurationLog) << "Configuration changes before:" << configurationChangesArguments();

  m_configurationChanges = config;

  qCDebug(cmakeBuildConfigurationLog) << "Configuration changes after:" << configurationChangesArguments();
}

// FIXME: Run clean steps when a setting starting with "ANDROID_BUILD_ABI_" is changed.
// FIXME: Warn when kit settings are overridden by a project.

auto CMakeBuildConfiguration::clearError(ForceEnabledChanged fec) -> void
{
  if (!m_error.isEmpty()) {
    m_error.clear();
    fec = ForceEnabledChanged::True;
  }
  if (fec == ForceEnabledChanged::True) {
    qCDebug(cmakeBuildConfigurationLog) << "Emitting enabledChanged signal";
    emit enabledChanged();
  }
}

auto CMakeBuildConfiguration::setInitialCMakeArguments(const QStringList &args) -> void
{
  QStringList additionalArguments;
  aspect<InitialCMakeArgumentsAspect>()->setAllValues(args.join('\n'), additionalArguments);

  // Set the unknown additional arguments also for the "Current Configuration"
  setAdditionalCMakeArguments(additionalArguments);
}

auto CMakeBuildConfiguration::additionalCMakeArguments() const -> QStringList
{
  return ProcessArgs::splitArgs(aspect<AdditionalCMakeOptionsAspect>()->value());
}

auto CMakeBuildConfiguration::setAdditionalCMakeArguments(const QStringList &args) -> void
{
  const auto expandedAdditionalArguments = Utils::transform(args, [this](const QString &s) {
    return macroExpander()->expand(s);
  });
  const auto nonEmptyAdditionalArguments = Utils::filtered(expandedAdditionalArguments, [](const QString &s) {
    return !s.isEmpty();
  });
  aspect<AdditionalCMakeOptionsAspect>()->setValue(ProcessArgs::joinArgs(nonEmptyAdditionalArguments));
}

auto CMakeBuildConfiguration::filterConfigArgumentsFromAdditionalCMakeArguments() -> void
{
  // On iOS the %{Ios:DevelopmentTeam:Flag} evalues to something like
  // -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM:STRING=MAGICSTRING
  // which is already part of the CMake variables and should not be also
  // in the addtional CMake options
  const auto arguments = ProcessArgs::splitArgs(aspect<AdditionalCMakeOptionsAspect>()->value());
  QStringList unknownOptions;
  const auto config = CMakeConfig::fromArguments(arguments, unknownOptions);

  aspect<AdditionalCMakeOptionsAspect>()->setValue(ProcessArgs::joinArgs(unknownOptions));
}

auto CMakeBuildConfiguration::setError(const QString &message) -> void
{
  qCDebug(cmakeBuildConfigurationLog) << "Setting error to" << message;
  QTC_ASSERT(!message.isEmpty(), return);

  const auto oldMessage = m_error;
  if (m_error != message)
    m_error = message;
  if (oldMessage.isEmpty() != !message.isEmpty()) {
    qCDebug(cmakeBuildConfigurationLog) << "Emitting enabledChanged signal";
    emit enabledChanged();
  }
  TaskHub::addTask(BuildSystemTask(Task::TaskType::Error, message));
  emit errorOccurred(m_error);
}

auto CMakeBuildConfiguration::setWarning(const QString &message) -> void
{
  if (m_warning == message)
    return;
  m_warning = message;
  TaskHub::addTask(BuildSystemTask(Task::TaskType::Warning, message));
  emit warningOccurred(m_warning);
}

auto CMakeBuildConfiguration::error() const -> QString
{
  return m_error;
}

auto CMakeBuildConfiguration::warning() const -> QString
{
  return m_warning;
}

auto CMakeBuildConfiguration::createConfigWidget() -> NamedWidget*
{
  return new CMakeBuildSettingsWidget(this);
}

auto CMakeBuildConfiguration::signingFlags() const -> CMakeConfig
{
  return {};
}

/*!
  \class CMakeBuildConfigurationFactory
*/

CMakeBuildConfigurationFactory::CMakeBuildConfigurationFactory()
{
  registerBuildConfiguration<CMakeBuildConfiguration>(Constants::CMAKE_BUILDCONFIGURATION_ID);

  setSupportedProjectType(CMakeProjectManager::Constants::CMAKE_PROJECT_ID);
  setSupportedProjectMimeTypeName(Constants::CMAKE_PROJECT_MIMETYPE);

  setBuildGenerator([](const Kit *k, const FilePath &projectPath, bool forSetup) {
    QList<BuildInfo> result;

    auto path = forSetup ? Project::projectDirectory(projectPath) : projectPath;

    for (int type = BuildTypeDebug; type != BuildTypeLast; ++type) {
      auto info = createBuildInfo(BuildType(type));
      if (forSetup) {
        info.buildDirectory = CMakeBuildConfiguration::shadowBuildDirectory(projectPath, k, info.typeName, info.buildType);
      }
      result << info;
    }
    return result;
  });
}

auto CMakeBuildConfigurationFactory::buildTypeFromByteArray(const QByteArray &in) -> CMakeBuildConfigurationFactory::BuildType
{
  const auto bt = in.toLower();
  if (bt == "debug")
    return BuildTypeDebug;
  if (bt == "release")
    return BuildTypeRelease;
  if (bt == "relwithdebinfo")
    return BuildTypeRelWithDebInfo;
  if (bt == "minsizerel")
    return BuildTypeMinSizeRel;
  return BuildTypeNone;
}

auto CMakeBuildConfigurationFactory::cmakeBuildTypeToBuildType(const CMakeBuildConfigurationFactory::BuildType &in) -> BuildConfiguration::BuildType
{
  // Cover all common CMake build types
  if (in == BuildTypeRelease || in == BuildTypeMinSizeRel)
    return BuildConfiguration::Release;
  else if (in == BuildTypeDebug)
    return BuildConfiguration::Debug;
  else if (in == BuildTypeRelWithDebInfo)
    return BuildConfiguration::Profile;
  else
    return BuildConfiguration::Unknown;
}

auto CMakeBuildConfigurationFactory::createBuildInfo(BuildType buildType) -> BuildInfo
{
  BuildInfo info;

  switch (buildType) {
  case BuildTypeNone:
    info.typeName = "Build";
    info.displayName = BuildConfiguration::tr("Build");
    info.buildType = BuildConfiguration::Unknown;
    break;
  case BuildTypeDebug:
    info.typeName = "Debug";
    info.displayName = BuildConfiguration::tr("Debug");
    info.buildType = BuildConfiguration::Debug;
    break;
  case BuildTypeRelease:
    info.typeName = "Release";
    info.displayName = BuildConfiguration::tr("Release");
    info.buildType = BuildConfiguration::Release;
    break;
  case BuildTypeMinSizeRel:
    info.typeName = "MinSizeRel";
    info.displayName = CMakeBuildConfiguration::tr("Minimum Size Release");
    info.buildType = BuildConfiguration::Release;
    break;
  case BuildTypeRelWithDebInfo:
    info.typeName = "RelWithDebInfo";
    info.displayName = CMakeBuildConfiguration::tr("Release with Debug Information");
    info.buildType = BuildConfiguration::Profile;
    break;
  default: QTC_CHECK(false);
    break;
  }

  return info;
}

auto CMakeBuildConfiguration::buildType() const -> BuildConfiguration::BuildType
{
  auto cmakeBuildTypeName = m_configurationFromCMake.valueOf("CMAKE_BUILD_TYPE");
  if (cmakeBuildTypeName.isEmpty()) {
    auto cmakeCfgTypes = m_configurationFromCMake.valueOf("CMAKE_CONFIGURATION_TYPES");
    if (!cmakeCfgTypes.isEmpty())
      cmakeBuildTypeName = cmakeBuildType().toUtf8();
  }
  // Cover all common CMake build types
  const auto cmakeBuildType = CMakeBuildConfigurationFactory::buildTypeFromByteArray(cmakeBuildTypeName);
  return CMakeBuildConfigurationFactory::cmakeBuildTypeToBuildType(cmakeBuildType);
}

auto CMakeBuildConfiguration::buildSystem() const -> BuildSystem*
{
  return m_buildSystem;
}

auto CMakeBuildConfiguration::setSourceDirectory(const FilePath &path) -> void
{
  aspect<SourceDirectoryAspect>()->setFilePath(path);
}

auto CMakeBuildConfiguration::sourceDirectory() const -> FilePath
{
  return aspect<SourceDirectoryAspect>()->filePath();
}

auto CMakeBuildConfiguration::cmakeBuildType() const -> QString
{
  auto setBuildTypeFromConfig = [this](const CMakeConfig &config) {
    auto it = std::find_if(config.begin(), config.end(), [](const CMakeConfigItem &item) {
      return item.key == "CMAKE_BUILD_TYPE" && !item.isInitial;
    });
    if (it != config.end())
      const_cast<CMakeBuildConfiguration*>(this)->setCMakeBuildType(QString::fromUtf8(it->value));
  };

  if (!isMultiConfig())
    setBuildTypeFromConfig(configurationChanges());

  auto cmakeBuildType = aspect<BuildTypeAspect>()->value();

  const auto cmakeCacheTxt = buildDirectory().pathAppended("CMakeCache.txt");
  const auto hasCMakeCache = QFile::exists(cmakeCacheTxt.toString());
  CMakeConfig config;

  if (cmakeBuildType == "Unknown") {
    // The "Unknown" type is the case of loading of an existing project
    // that doesn't have the "CMake.Build.Type" aspect saved
    if (hasCMakeCache) {
      QString errorMessage;
      config = CMakeBuildSystem::parseCMakeCacheDotTxt(cmakeCacheTxt, &errorMessage);
    } else {
      config = initialCMakeConfiguration();
    }
  } else if (!hasCMakeCache) {
    config = initialCMakeConfiguration();
  }

  if (!config.isEmpty() && !isMultiConfig())
    setBuildTypeFromConfig(config);

  return cmakeBuildType;
}

auto CMakeBuildConfiguration::setCMakeBuildType(const QString &cmakeBuildType, bool quiet) -> void
{
  if (quiet) {
    aspect<BuildTypeAspect>()->setValueQuietly(cmakeBuildType);
    aspect<BuildTypeAspect>()->update();
  } else {
    aspect<BuildTypeAspect>()->setValue(cmakeBuildType);
  }
}

auto CMakeBuildConfiguration::isMultiConfig() const -> bool
{
  return m_isMultiConfig;
}

auto CMakeBuildConfiguration::setIsMultiConfig(bool isMultiConfig) -> void
{
  m_isMultiConfig = isMultiConfig;
}

namespace Internal {

// ----------------------------------------------------------------------
// - InitialCMakeParametersAspect:
// ----------------------------------------------------------------------

auto InitialCMakeArgumentsAspect::cmakeConfiguration() const -> const CMakeConfig&
{
  return m_cmakeConfiguration;
}

auto InitialCMakeArgumentsAspect::allValues() const -> const QStringList
{
  auto initialCMakeArguments = Utils::transform(m_cmakeConfiguration.toList(), [](const CMakeConfigItem &ci) {
    return ci.toArgument(nullptr);
  });

  initialCMakeArguments.append(ProcessArgs::splitArgs(value()));

  return initialCMakeArguments;
}

auto InitialCMakeArgumentsAspect::setAllValues(const QString &values, QStringList &additionalOptions) -> void
{
  auto arguments = values.split('\n', Qt::SkipEmptyParts);
  QString cmakeGenerator;
  for (auto &arg : arguments) {
    if (arg.startsWith("-G")) {
      const QString strDash(" - ");
      const int idxDash = arg.indexOf(strDash);
      if (idxDash > 0) {
        // -GCodeBlocks - Ninja
        cmakeGenerator = "-DCMAKE_GENERATOR:STRING=" + arg.mid(idxDash + strDash.length());

        arg = arg.left(idxDash);
        arg.replace("-G", "-DCMAKE_EXTRA_GENERATOR:STRING=");

      } else {
        // -GNinja
        arg.replace("-G", "-DCMAKE_GENERATOR:STRING=");
      }
    }
    if (arg.startsWith("-A"))
      arg.replace("-A", "-DCMAKE_GENERATOR_PLATFORM:STRING=");
    if (arg.startsWith("-T"))
      arg.replace("-T", "-DCMAKE_GENERATOR_TOOLSET:STRING=");
  }
  if (!cmakeGenerator.isEmpty())
    arguments.append(cmakeGenerator);

  m_cmakeConfiguration = CMakeConfig::fromArguments(arguments, additionalOptions);
  for (auto &ci : m_cmakeConfiguration)
    ci.isInitial = true;

  // Display the unknown arguments in "Additional CMake Options"
  const auto additionalOptionsValue = ProcessArgs::joinArgs(additionalOptions);
  BaseAspect::setValueQuietly(additionalOptionsValue);
}

auto InitialCMakeArgumentsAspect::setCMakeConfiguration(const CMakeConfig &config) -> void
{
  m_cmakeConfiguration = config;
  for (auto &ci : m_cmakeConfiguration)
    ci.isInitial = true;
}

auto InitialCMakeArgumentsAspect::fromMap(const QVariantMap &map) -> void
{
  const auto value = map.value(settingsKey(), defaultValue()).toString();
  QStringList additionalArguments;
  setAllValues(value, additionalArguments);
}

auto InitialCMakeArgumentsAspect::toMap(QVariantMap &map) const -> void
{
  saveToMap(map, allValues().join('\n'), defaultValue(), settingsKey());
}

InitialCMakeArgumentsAspect::InitialCMakeArgumentsAspect()
{
  setSettingsKey("CMake.Initial.Parameters");
  setLabelText(tr("Additional CMake <a href=\"options\">options</a>:"));
  setDisplayStyle(LineEditDisplay);
}

// ----------------------------------------------------------------------
// - AdditionalCMakeOptionsAspect:
// ----------------------------------------------------------------------

AdditionalCMakeOptionsAspect::AdditionalCMakeOptionsAspect()
{
  setSettingsKey("CMake.Additional.Options");
  setLabelText(tr("Additional CMake <a href=\"options\">options</a>:"));
  setDisplayStyle(LineEditDisplay);
}

// -----------------------------------------------------------------------------
// SourceDirectoryAspect:
// -----------------------------------------------------------------------------
SourceDirectoryAspect::SourceDirectoryAspect()
{
  // Will not be displayed, only persisted
  setSettingsKey("CMake.Source.Directory");
}

// -----------------------------------------------------------------------------
// BuildTypeAspect:
// -----------------------------------------------------------------------------
BuildTypeAspect::BuildTypeAspect()
{
  setSettingsKey("CMake.Build.Type");
  setLabelText(tr("Build type:"));
  setDisplayStyle(LineEditDisplay);
  setDefaultValue("Unknown");
}

} // namespace Internal
} // namespace CMakeProjectManager
