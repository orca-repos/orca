// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangdiagnosticconfigswidget.hpp"

#include "cppcodemodelsettings.hpp"
#include "ui_clangdiagnosticconfigswidget.h"
#include "ui_clangbasechecks.h"

#include <utils/executeondestruction.hpp>
#include <utils/stringutils.hpp>
#include <utils/treemodel.hpp>

#include <QInputDialog>
#include <QPushButton>

namespace CppEditor {

class ConfigNode : public Utils::TreeItem {
public:
  ConfigNode(const ClangDiagnosticConfig &config) : config(config) {}

  auto data(int, int role) const -> QVariant override
  {
    if (role == Qt::DisplayRole)
      return config.displayName();
    return QVariant();
  }

  ClangDiagnosticConfig config;
};

class GroupNode : public Utils::StaticTreeItem {
public:
  GroupNode(const QString &text) : Utils::StaticTreeItem(text) {}

  auto flags(int) const -> Qt::ItemFlags final { return {}; }

  auto data(int column, int role) const -> QVariant final
  {
    if (role == Qt::ForegroundRole) {
      // Avoid disabled color.
      return QApplication::palette().color(QPalette::ColorGroup::Normal, QPalette::ColorRole::Text);
    }
    return Utils::StaticTreeItem::data(column, role);
  }
};

class ConfigsModel : public Utils::TreeModel<Utils::TreeItem, GroupNode, ConfigNode> {
  Q_OBJECT public:
  ConfigsModel(const ClangDiagnosticConfigs &configs)
  {
    m_builtinRoot = new GroupNode(tr("Built-in"));
    m_customRoot = new GroupNode(tr("Custom"));
    rootItem()->appendChild(m_builtinRoot);
    rootItem()->appendChild(m_customRoot);

    for (const auto &config : configs) {
      auto parent = config.isReadOnly() ? m_builtinRoot : m_customRoot;
      parent->appendChild(new ConfigNode(config));
    }
  }

  auto customConfigsCount() const -> int { return m_customRoot->childCount(); }
  auto fallbackConfigIndex() const -> QModelIndex { return m_builtinRoot->lastChild()->index(); }

  auto configs() const -> ClangDiagnosticConfigs
  {
    ClangDiagnosticConfigs configs;
    forItemsAtLevel<2>([&configs](const ConfigNode *node) {
      configs << node->config;
    });
    return configs;
  }

  auto appendCustomConfig(const ClangDiagnosticConfig &config) -> void
  {
    m_customRoot->appendChild(new ConfigNode(config));
  }

  auto removeConfig(const Utils::Id &id) -> void
  {
    auto node = itemForConfigId(id);
    node->parent()->removeChildAt(node->indexInParent());
  }

  auto itemForConfigId(const Utils::Id &id) const -> ConfigNode*
  {
    return findItemAtLevel<2>([id](const ConfigNode *node) {
      return node->config.id() == id;
    });
  }

private:
  Utils::TreeItem *m_builtinRoot = nullptr;
  Utils::TreeItem *m_customRoot = nullptr;
};

ClangDiagnosticConfigsWidget::ClangDiagnosticConfigsWidget(const ClangDiagnosticConfigs &configs, const Utils::Id &configToSelect, QWidget *parent) : QWidget(parent), m_ui(new Ui::ClangDiagnosticConfigsWidget), m_configsModel(new ConfigsModel(configs))
{
  m_ui->setupUi(this);
  m_ui->configsView->setHeaderHidden(true);
  m_ui->configsView->setUniformRowHeights(true);
  m_ui->configsView->setRootIsDecorated(false);
  m_ui->configsView->setModel(m_configsModel);
  m_ui->configsView->setCurrentIndex(m_configsModel->itemForConfigId(configToSelect)->index());
  m_ui->configsView->setItemsExpandable(false);
  m_ui->configsView->expandAll();
  connect(m_ui->configsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &ClangDiagnosticConfigsWidget::sync);

  m_clangBaseChecks = std::make_unique<CppEditor::Ui::ClangBaseChecks>();
  m_clangBaseChecksWidget = new QWidget();
  m_clangBaseChecks->setupUi(m_clangBaseChecksWidget);

  m_ui->tabWidget->addTab(m_clangBaseChecksWidget, tr("Clang Warnings"));
  m_ui->tabWidget->setCurrentIndex(0);

  connect(m_ui->copyButton, &QPushButton::clicked, this, &ClangDiagnosticConfigsWidget::onCopyButtonClicked);
  connect(m_ui->renameButton, &QPushButton::clicked, this, &ClangDiagnosticConfigsWidget::onRenameButtonClicked);
  connect(m_ui->removeButton, &QPushButton::clicked, this, &ClangDiagnosticConfigsWidget::onRemoveButtonClicked);
  connectClangOnlyOptionsChanged();
}

ClangDiagnosticConfigsWidget::~ClangDiagnosticConfigsWidget()
{
  delete m_ui;
}

auto ClangDiagnosticConfigsWidget::onCopyButtonClicked() -> void
{
  const auto &config = currentConfig();
  auto dialogAccepted = false;
  const auto newName = QInputDialog::getText(this, tr("Copy Diagnostic Configuration"), tr("Diagnostic configuration name:"), QLineEdit::Normal, tr("%1 (Copy)").arg(config.displayName()), &dialogAccepted);
  if (dialogAccepted) {
    const auto customConfig = ClangDiagnosticConfigsModel::createCustomConfig(config, newName);

    m_configsModel->appendCustomConfig(customConfig);
    m_ui->configsView->setCurrentIndex(m_configsModel->itemForConfigId(customConfig.id())->index());
    sync();
    m_clangBaseChecks->diagnosticOptionsTextEdit->setFocus();
  }
}

auto ClangDiagnosticConfigsWidget::onRenameButtonClicked() -> void
{
  const auto &config = currentConfig();

  auto dialogAccepted = false;
  const auto newName = QInputDialog::getText(this, tr("Rename Diagnostic Configuration"), tr("New name:"), QLineEdit::Normal, config.displayName(), &dialogAccepted);
  if (dialogAccepted) {
    auto configNode = m_configsModel->itemForConfigId(config.id());
    configNode->config.setDisplayName(newName);
  }
}

auto ClangDiagnosticConfigsWidget::currentConfig() const -> const ClangDiagnosticConfig
{
  auto item = m_configsModel->itemForIndex(m_ui->configsView->currentIndex());
  return static_cast<ConfigNode*>(item)->config;
}

auto ClangDiagnosticConfigsWidget::onRemoveButtonClicked() -> void
{
  const auto configToRemove = currentConfig().id();
  if (m_configsModel->customConfigsCount() == 1)
    m_ui->configsView->setCurrentIndex(m_configsModel->fallbackConfigIndex());
  m_configsModel->removeConfig(configToRemove);
  sync();
}

static auto isAcceptedWarningOption(const QString &option) -> bool
{
  return option == "-w" || option == "-pedantic" || option == "-pedantic-errors";
}

// Reference:
// https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
// https://clang.llvm.org/docs/DiagnosticsReference.html
static auto isValidOption(const QString &option) -> bool
{
  if (option == "-Werror")
    return false; // Avoid errors due to unknown or misspelled warnings.
  return option.startsWith("-W") || isAcceptedWarningOption(option);
}

static auto validateDiagnosticOptions(const QStringList &options) -> QString
{
  // This is handy for testing, allow disabling validation.
  if (qEnvironmentVariableIntValue("QTC_CLANG_NO_DIAGNOSTIC_CHECK"))
    return QString();

  for (const auto &option : options) {
    if (!isValidOption(option))
      return ClangDiagnosticConfigsWidget::tr("Option \"%1\" is invalid.").arg(option);
  }

  return QString();
}

static auto normalizeDiagnosticInputOptions(const QString &options) -> QStringList
{
  return options.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

auto ClangDiagnosticConfigsWidget::onClangOnlyOptionsChanged() -> void
{
  const bool useBuildSystemWarnings = m_clangBaseChecks->useFlagsFromBuildSystemCheckBox->isChecked();

  // Clean up options input
  const QString diagnosticOptions = m_clangBaseChecks->diagnosticOptionsTextEdit->document()->toPlainText();
  const auto normalizedOptions = normalizeDiagnosticInputOptions(diagnosticOptions);

  // Validate options input
  const auto errorMessage = validateDiagnosticOptions(normalizedOptions);
  updateValidityWidgets(errorMessage);
  if (!errorMessage.isEmpty()) {
    // Remember the entered options in case the user will switch back.
    m_notAcceptedOptions.insert(currentConfig().id(), diagnosticOptions);
    return;
  }
  m_notAcceptedOptions.remove(currentConfig().id());

  // Commit valid changes
  auto updatedConfig = currentConfig();
  updatedConfig.setClangOptions(normalizedOptions);
  updatedConfig.setUseBuildSystemWarnings(useBuildSystemWarnings);
  updateConfig(updatedConfig);
}

auto ClangDiagnosticConfigsWidget::sync() -> void
{
  if (!m_ui->configsView->currentIndex().isValid())
    return;

  disconnectClangOnlyOptionsChanged();
  Utils::ExecuteOnDestruction e([this]() { connectClangOnlyOptionsChanged(); });

  // Update main button row
  const auto &config = currentConfig();
  m_ui->removeButton->setEnabled(!config.isReadOnly());
  m_ui->renameButton->setEnabled(!config.isReadOnly());

  // Update check box
  m_clangBaseChecks->useFlagsFromBuildSystemCheckBox->setChecked(config.useBuildSystemWarnings());

  // Update Text Edit
  const auto options = m_notAcceptedOptions.contains(config.id()) ? m_notAcceptedOptions.value(config.id()) : config.clangOptions().join(QLatin1Char(' '));
  setDiagnosticOptions(options);
  m_clangBaseChecksWidget->setEnabled(!config.isReadOnly());

  if (config.isReadOnly()) {
    m_ui->infoLabel->setType(Utils::InfoLabel::Information);
    m_ui->infoLabel->setText(tr("Copy this configuration to customize it."));
    m_ui->infoLabel->setFilled(false);
  }

  syncExtraWidgets(config);
}

auto ClangDiagnosticConfigsWidget::updateConfig(const ClangDiagnosticConfig &config) -> void
{
  m_configsModel->itemForConfigId(config.id())->config = config;
}

auto ClangDiagnosticConfigsWidget::setDiagnosticOptions(const QString &options) -> void
{
  if (options != m_clangBaseChecks->diagnosticOptionsTextEdit->document()->toPlainText())
    m_clangBaseChecks->diagnosticOptionsTextEdit->document()->setPlainText(options);

  const auto errorMessage = validateDiagnosticOptions(normalizeDiagnosticInputOptions(options));
  updateValidityWidgets(errorMessage);
}

auto ClangDiagnosticConfigsWidget::updateValidityWidgets(const QString &errorMessage) -> void
{
  if (errorMessage.isEmpty()) {
    m_ui->infoLabel->setType(Utils::InfoLabel::Information);
    m_ui->infoLabel->setText(tr("Configuration passes sanity checks."));
    m_ui->infoLabel->setFilled(false);
  } else {
    m_ui->infoLabel->setType(Utils::InfoLabel::Error);
    m_ui->infoLabel->setText(tr("%1").arg(errorMessage));
    m_ui->infoLabel->setFilled(true);
  }
}

auto ClangDiagnosticConfigsWidget::connectClangOnlyOptionsChanged() -> void
{
  connect(m_clangBaseChecks->useFlagsFromBuildSystemCheckBox, &QCheckBox::stateChanged, this, &ClangDiagnosticConfigsWidget::onClangOnlyOptionsChanged);
  connect(m_clangBaseChecks->diagnosticOptionsTextEdit->document(), &QTextDocument::contentsChanged, this, &ClangDiagnosticConfigsWidget::onClangOnlyOptionsChanged);
}

auto ClangDiagnosticConfigsWidget::disconnectClangOnlyOptionsChanged() -> void
{
  disconnect(m_clangBaseChecks->useFlagsFromBuildSystemCheckBox, &QCheckBox::stateChanged, this, &ClangDiagnosticConfigsWidget::onClangOnlyOptionsChanged);
  disconnect(m_clangBaseChecks->diagnosticOptionsTextEdit->document(), &QTextDocument::contentsChanged, this, &ClangDiagnosticConfigsWidget::onClangOnlyOptionsChanged);
}

auto ClangDiagnosticConfigsWidget::configs() const -> ClangDiagnosticConfigs
{
  return m_configsModel->configs();
}

auto ClangDiagnosticConfigsWidget::tabWidget() const -> QTabWidget*
{
  return m_ui->tabWidget;
}

} // CppEditor namespace

#include "clangdiagnosticconfigswidget.moc"
