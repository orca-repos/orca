// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "runsettingspropertiespage.hpp"

#include "addrunconfigdialog.hpp"
#include "buildmanager.hpp"
#include "buildstepspage.hpp"
#include "deployconfiguration.hpp"
#include "projectconfigurationmodel.hpp"
#include "runconfiguration.hpp"
#include "session.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/infolabel.hpp>

#include <QAction>
#include <QComboBox>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpacerItem>
#include <QWidget>

namespace ProjectExplorer {
namespace Internal {

// RunSettingsWidget

RunSettingsWidget::RunSettingsWidget(Target *target) : m_target(target)
{
  Q_ASSERT(m_target);

  m_deployConfigurationCombo = new QComboBox(this);
  m_addDeployToolButton = new QPushButton(tr("Add"), this);
  m_removeDeployToolButton = new QPushButton(tr("Remove"), this);
  m_renameDeployButton = new QPushButton(tr("Rename..."), this);

  const auto deployWidget = new QWidget(this);

  m_runConfigurationCombo = new QComboBox(this);
  m_runConfigurationCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_runConfigurationCombo->setMinimumContentsLength(15);

  m_addRunToolButton = new QPushButton(tr("Add..."), this);
  m_removeRunToolButton = new QPushButton(tr("Remove"), this);
  m_renameRunButton = new QPushButton(tr("Rename..."), this);
  m_cloneRunButton = new QPushButton(tr("Clone..."), this);

  const auto spacer1 = new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum);
  const auto spacer2 = new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding);

  const auto runWidget = new QWidget(this);

  const auto deployTitle = new QLabel(tr("Deployment"), this);
  const auto deployLabel = new QLabel(tr("Method:"), this);
  const auto runTitle = new QLabel(tr("Run"), this);
  const auto runLabel = new QLabel(tr("Run configuration:"), this);

  runLabel->setBuddy(m_runConfigurationCombo);

  auto f = runLabel->font();
  f.setBold(true);
  f.setPointSizeF(f.pointSizeF() * 1.2);

  runTitle->setFont(f);
  deployTitle->setFont(f);

  m_gridLayout = new QGridLayout(this);
  m_gridLayout->setContentsMargins(0, 20, 0, 0);
  m_gridLayout->setHorizontalSpacing(6);
  m_gridLayout->setVerticalSpacing(8);
  m_gridLayout->addWidget(deployTitle, 0, 0, 1, -1);
  m_gridLayout->addWidget(deployLabel, 1, 0, 1, 1);
  m_gridLayout->addWidget(m_deployConfigurationCombo, 1, 1, 1, 1);
  m_gridLayout->addWidget(m_addDeployToolButton, 1, 2, 1, 1);
  m_gridLayout->addWidget(m_removeDeployToolButton, 1, 3, 1, 1);
  m_gridLayout->addWidget(m_renameDeployButton, 1, 4, 1, 1);
  m_gridLayout->addWidget(deployWidget, 2, 0, 1, -1);

  m_gridLayout->addWidget(runTitle, 3, 0, 1, -1);
  m_gridLayout->addWidget(runLabel, 4, 0, 1, 1);
  m_gridLayout->addWidget(m_runConfigurationCombo, 4, 1, 1, 1);
  m_gridLayout->addWidget(m_addRunToolButton, 4, 2, 1, 1);
  m_gridLayout->addWidget(m_removeRunToolButton, 4, 3, 1, 1);
  m_gridLayout->addWidget(m_renameRunButton, 4, 4, 1, 1);
  m_gridLayout->addWidget(m_cloneRunButton, 4, 5, 1, 1);
  m_gridLayout->addItem(spacer1, 4, 6, 1, 1);
  m_gridLayout->addWidget(runWidget, 5, 0, 1, -1);
  m_gridLayout->addItem(spacer2, 6, 0, 1, 1);

  // deploy part
  deployWidget->setContentsMargins(0, 10, 0, 25);
  m_deployLayout = new QVBoxLayout(deployWidget);
  m_deployLayout->setContentsMargins(0, 0, 0, 0);
  m_deployLayout->setSpacing(5);

  m_deployConfigurationCombo->setModel(m_target->deployConfigurationModel());

  m_addDeployMenu = new QMenu(m_addDeployToolButton);
  m_addDeployToolButton->setMenu(m_addDeployMenu);

  updateDeployConfiguration(m_target->activeDeployConfiguration());

  // Some projects may not support deployment, so we need this:
  m_addDeployToolButton->setEnabled(m_target->activeDeployConfiguration());
  m_deployConfigurationCombo->setEnabled(m_target->activeDeployConfiguration());

  m_removeDeployToolButton->setEnabled(m_target->deployConfigurations().count() > 1);
  m_renameDeployButton->setEnabled(m_target->activeDeployConfiguration());

  connect(m_addDeployMenu, &QMenu::aboutToShow, this, &RunSettingsWidget::aboutToShowDeployMenu);
  connect(m_deployConfigurationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RunSettingsWidget::currentDeployConfigurationChanged);
  connect(m_removeDeployToolButton, &QAbstractButton::clicked, this, &RunSettingsWidget::removeDeployConfiguration);
  connect(m_renameDeployButton, &QAbstractButton::clicked, this, &RunSettingsWidget::renameDeployConfiguration);

  connect(m_target, &Target::activeDeployConfigurationChanged, this, &RunSettingsWidget::activeDeployConfigurationChanged);

  // run part
  runWidget->setContentsMargins(0, 10, 0, 0);
  m_runLayout = new QVBoxLayout(runWidget);
  m_runLayout->setContentsMargins(0, 0, 0, 0);
  m_runLayout->setSpacing(5);

  m_disabledText = new Utils::InfoLabel({}, Utils::InfoLabel::Warning);
  m_runLayout->addWidget(m_disabledText);

  const auto model = m_target->runConfigurationModel();
  const auto rc = m_target->activeRunConfiguration();
  m_runConfigurationCombo->setModel(model);
  m_runConfigurationCombo->setCurrentIndex(model->indexFor(rc));

  m_removeRunToolButton->setEnabled(m_target->runConfigurations().size() > 1);
  m_renameRunButton->setEnabled(rc);
  m_cloneRunButton->setEnabled(rc);

  setConfigurationWidget(rc);

  connect(m_addRunToolButton, &QAbstractButton::clicked, this, &RunSettingsWidget::showAddRunConfigDialog);
  connect(m_runConfigurationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RunSettingsWidget::currentRunConfigurationChanged);
  connect(m_removeRunToolButton, &QAbstractButton::clicked, this, &RunSettingsWidget::removeRunConfiguration);
  connect(m_renameRunButton, &QAbstractButton::clicked, this, &RunSettingsWidget::renameRunConfiguration);
  connect(m_cloneRunButton, &QAbstractButton::clicked, this, &RunSettingsWidget::cloneRunConfiguration);

  connect(m_target, &Target::addedRunConfiguration, this, &RunSettingsWidget::updateRemoveToolButton);
  connect(m_target, &Target::removedRunConfiguration, this, &RunSettingsWidget::updateRemoveToolButton);

  connect(m_target, &Target::addedDeployConfiguration, this, &RunSettingsWidget::updateRemoveToolButton);
  connect(m_target, &Target::removedDeployConfiguration, this, &RunSettingsWidget::updateRemoveToolButton);

  connect(m_target, &Target::activeRunConfigurationChanged, this, &RunSettingsWidget::activeRunConfigurationChanged);
}

auto RunSettingsWidget::showAddRunConfigDialog() -> void
{
  AddRunConfigDialog dlg(m_target, this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  const auto rci = dlg.creationInfo();
  QTC_ASSERT(rci.factory, return);
  const auto newRC = rci.create(m_target);
  if (!newRC)
    return;
  QTC_CHECK(newRC->id() == rci.factory->runConfigurationId());
  m_target->addRunConfiguration(newRC);
  m_target->setActiveRunConfiguration(newRC);
  m_removeRunToolButton->setEnabled(m_target->runConfigurations().size() > 1);
}

auto RunSettingsWidget::cloneRunConfiguration() -> void
{
  const auto activeRunConfiguration = m_target->activeRunConfiguration();

  //: Title of a the cloned RunConfiguration window, text of the window
  const auto name = uniqueRCName(QInputDialog::getText(this, tr("Clone Configuration"), tr("New configuration name:"), QLineEdit::Normal, activeRunConfiguration->displayName()));
  if (name.isEmpty())
    return;

  const auto newRc = RunConfigurationFactory::clone(m_target, activeRunConfiguration);
  if (!newRc)
    return;

  newRc->setDisplayName(name);
  m_target->addRunConfiguration(newRc);
  m_target->setActiveRunConfiguration(newRc);
}

auto RunSettingsWidget::removeRunConfiguration() -> void
{
  const auto rc = m_target->activeRunConfiguration();
  QMessageBox msgBox(QMessageBox::Question, tr("Remove Run Configuration?"), tr("Do you really want to delete the run configuration <b>%1</b>?").arg(rc->displayName()), QMessageBox::Yes | QMessageBox::No, this);
  msgBox.setDefaultButton(QMessageBox::No);
  msgBox.setEscapeButton(QMessageBox::No);
  if (msgBox.exec() == QMessageBox::No)
    return;

  m_target->removeRunConfiguration(rc);
  m_removeRunToolButton->setEnabled(m_target->runConfigurations().size() > 1);
  m_renameRunButton->setEnabled(m_target->activeRunConfiguration());
  m_cloneRunButton->setEnabled(m_target->activeRunConfiguration());
}

auto RunSettingsWidget::activeRunConfigurationChanged() -> void
{
  if (m_ignoreChange)
    return;

  const auto model = m_target->runConfigurationModel();
  const auto index = model->indexFor(m_target->activeRunConfiguration());
  m_ignoreChange = true;
  m_runConfigurationCombo->setCurrentIndex(index);
  setConfigurationWidget(qobject_cast<RunConfiguration*>(model->projectConfigurationAt(index)));
  m_ignoreChange = false;
  m_renameRunButton->setEnabled(m_target->activeRunConfiguration());
  m_cloneRunButton->setEnabled(m_target->activeRunConfiguration());
}

auto RunSettingsWidget::renameRunConfiguration() -> void
{
  bool ok;
  auto name = QInputDialog::getText(this, tr("Rename..."), tr("New name for run configuration <b>%1</b>:").arg(m_target->activeRunConfiguration()->displayName()), QLineEdit::Normal, m_target->activeRunConfiguration()->displayName(), &ok);
  if (!ok)
    return;

  name = uniqueRCName(name);
  if (name.isEmpty())
    return;

  m_target->activeRunConfiguration()->setDisplayName(name);
}

auto RunSettingsWidget::currentRunConfigurationChanged(int index) -> void
{
  if (m_ignoreChange)
    return;

  RunConfiguration *selectedRunConfiguration = nullptr;
  if (index >= 0)
    selectedRunConfiguration = qobject_cast<RunConfiguration*>(m_target->runConfigurationModel()->projectConfigurationAt(index));

  if (selectedRunConfiguration == m_runConfiguration)
    return;

  m_ignoreChange = true;
  m_target->setActiveRunConfiguration(selectedRunConfiguration);
  m_ignoreChange = false;

  // Update the run configuration configuration widget
  setConfigurationWidget(selectedRunConfiguration);
}

auto RunSettingsWidget::currentDeployConfigurationChanged(int index) -> void
{
  if (m_ignoreChange)
    return;
  if (index == -1)
    SessionManager::setActiveDeployConfiguration(m_target, nullptr, SetActive::Cascade);
  else
    SessionManager::setActiveDeployConfiguration(m_target, qobject_cast<DeployConfiguration*>(m_target->deployConfigurationModel()->projectConfigurationAt(index)), SetActive::Cascade);
}

auto RunSettingsWidget::aboutToShowDeployMenu() -> void
{
  m_addDeployMenu->clear();

  for (auto factory : DeployConfigurationFactory::find(m_target)) {
    const auto action = m_addDeployMenu->addAction(factory->defaultDisplayName());
    connect(action, &QAction::triggered, [factory, this]() {
      const auto newDc = factory->create(m_target);
      if (!newDc)
        return;
      m_target->addDeployConfiguration(newDc);
      SessionManager::setActiveDeployConfiguration(m_target, newDc, SetActive::Cascade);
      m_removeDeployToolButton->setEnabled(m_target->deployConfigurations().size() > 1);
    });
  }
}

auto RunSettingsWidget::removeDeployConfiguration() -> void
{
  const auto dc = m_target->activeDeployConfiguration();
  if (BuildManager::isBuilding(dc)) {
    QMessageBox box;
    const auto closeAnyway = box.addButton(tr("Cancel Build && Remove Deploy Configuration"), QMessageBox::AcceptRole);
    const auto cancelClose = box.addButton(tr("Do Not Remove"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelClose);
    box.setWindowTitle(tr("Remove Deploy Configuration %1?").arg(dc->displayName()));
    box.setText(tr("The deploy configuration <b>%1</b> is currently being built.").arg(dc->displayName()));
    box.setInformativeText(tr("Do you want to cancel the build process and remove the Deploy Configuration anyway?"));
    box.exec();
    if (box.clickedButton() != closeAnyway)
      return;
    BuildManager::cancel();
  } else {
    QMessageBox msgBox(QMessageBox::Question, tr("Remove Deploy Configuration?"), tr("Do you really want to delete deploy configuration <b>%1</b>?").arg(dc->displayName()), QMessageBox::Yes | QMessageBox::No, this);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setEscapeButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::No)
      return;
  }

  m_target->removeDeployConfiguration(dc);

  m_removeDeployToolButton->setEnabled(m_target->deployConfigurations().size() > 1);
}

auto RunSettingsWidget::activeDeployConfigurationChanged() -> void
{
  updateDeployConfiguration(m_target->activeDeployConfiguration());
}

auto RunSettingsWidget::renameDeployConfiguration() -> void
{
  bool ok;
  auto name = QInputDialog::getText(this, tr("Rename..."), tr("New name for deploy configuration <b>%1</b>:").arg(m_target->activeDeployConfiguration()->displayName()), QLineEdit::Normal, m_target->activeDeployConfiguration()->displayName(), &ok);
  if (!ok)
    return;

  name = uniqueDCName(name);
  if (name.isEmpty())
    return;
  m_target->activeDeployConfiguration()->setDisplayName(name);
}

auto RunSettingsWidget::updateRemoveToolButton() -> void
{
  m_removeDeployToolButton->setEnabled(m_target->deployConfigurations().count() > 1);
  m_removeRunToolButton->setEnabled(m_target->runConfigurations().size() > 1);
}

auto RunSettingsWidget::updateDeployConfiguration(DeployConfiguration *dc) -> void
{
  delete m_deployConfigurationWidget;
  m_deployConfigurationWidget = nullptr;
  delete m_deploySteps;
  m_deploySteps = nullptr;

  m_ignoreChange = true;
  m_deployConfigurationCombo->setCurrentIndex(-1);
  m_ignoreChange = false;

  m_renameDeployButton->setEnabled(dc);

  if (!dc)
    return;

  const auto index = m_target->deployConfigurationModel()->indexFor(dc);
  m_ignoreChange = true;
  m_deployConfigurationCombo->setCurrentIndex(index);
  m_ignoreChange = false;

  m_deployConfigurationWidget = dc->createConfigWidget();
  if (m_deployConfigurationWidget)
    m_deployLayout->addWidget(m_deployConfigurationWidget);

  m_deploySteps = new BuildStepListWidget(dc->stepList());
  m_deployLayout->addWidget(m_deploySteps);
}

auto RunSettingsWidget::setConfigurationWidget(RunConfiguration *rc) -> void
{
  if (rc == m_runConfiguration)
    return;

  delete m_runConfigurationWidget;
  m_runConfigurationWidget = nullptr;
  removeSubWidgets();
  if (!rc)
    return;
  m_runConfigurationWidget = rc->createConfigurationWidget();
  m_runConfiguration = rc;
  if (m_runConfigurationWidget) {
    m_runLayout->addWidget(m_runConfigurationWidget);
    updateEnabledState();
    connect(m_runConfiguration, &RunConfiguration::enabledChanged, m_runConfigurationWidget, [this]() { updateEnabledState(); });
  }
  addRunControlWidgets();
}

auto RunSettingsWidget::uniqueDCName(const QString &name) -> QString
{
  auto result = name.trimmed();
  if (!result.isEmpty()) {
    QStringList dcNames;
    foreach(DeployConfiguration *dc, m_target->deployConfigurations()) {
      if (dc == m_target->activeDeployConfiguration())
        continue;
      dcNames.append(dc->displayName());
    }
    result = Utils::makeUniquelyNumbered(result, dcNames);
  }
  return result;
}

auto RunSettingsWidget::uniqueRCName(const QString &name) -> QString
{
  auto result = name.trimmed();
  if (!result.isEmpty()) {
    QStringList rcNames;
    foreach(RunConfiguration *rc, m_target->runConfigurations()) {
      if (rc == m_target->activeRunConfiguration())
        continue;
      rcNames.append(rc->displayName());
    }
    result = Utils::makeUniquelyNumbered(result, rcNames);
  }
  return result;
}

auto RunSettingsWidget::addRunControlWidgets() -> void
{
  for (auto aspect : m_runConfiguration->aspects()) {
    if (const auto rcw = aspect->createConfigWidget()) {
      auto label = new QLabel(this);
      label->setText(aspect->displayName());
      connect(aspect, &GlobalOrProjectAspect::changed, label, [label, aspect] {
        label->setText(aspect->displayName());
      });
      addSubWidget(rcw, label);
    }
  }
}

auto RunSettingsWidget::addSubWidget(QWidget *widget, QLabel *label) -> void
{
  widget->setContentsMargins(0, 10, 0, 0);

  auto f = label->font();
  f.setBold(true);
  f.setPointSizeF(f.pointSizeF() * 1.2);
  label->setFont(f);

  label->setContentsMargins(0, 10, 0, 0);

  const auto l = m_gridLayout;
  l->addWidget(label, l->rowCount(), 0, 1, -1);
  l->addWidget(widget, l->rowCount(), 0, 1, -1);

  m_subWidgets.append(qMakePair(widget, label));
}

auto RunSettingsWidget::removeSubWidgets() -> void
{
  for (const auto &item : qAsConst(m_subWidgets)) {
    delete item.first;
    delete item.second;
  }
  m_subWidgets.clear();
}

auto RunSettingsWidget::updateEnabledState() -> void
{
  const auto enable = m_runConfiguration ? m_runConfiguration->isEnabled() : false;
  const auto reason = m_runConfiguration ? m_runConfiguration->disabledReason() : QString();

  m_runConfigurationWidget->setEnabled(enable);

  m_disabledText->setVisible(!enable && !reason.isEmpty());
  m_disabledText->setText(reason);
}

} // Internal
} // ProjectExplorer
