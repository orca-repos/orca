// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildsettingspropertiespage.hpp"
#include "buildinfo.hpp"
#include "buildstepspage.hpp"
#include "target.hpp"
#include "project.hpp"
#include "buildconfiguration.hpp"
#include "projectconfigurationmodel.hpp"
#include "session.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <core/core-interface.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <utils/stringutils.hpp>

#include <QMargins>
#include <QCoreApplication>
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace ProjectExplorer;
using namespace Internal;
using namespace Utils;

///
// BuildSettingsWidget
///

BuildSettingsWidget::~BuildSettingsWidget()
{
  clearWidgets();
}

BuildSettingsWidget::BuildSettingsWidget(Target *target) : m_target(target)
{
  Q_ASSERT(m_target);

  const auto vbox = new QVBoxLayout(this);
  vbox->setContentsMargins(0, 0, 0, 0);

  if (!BuildConfigurationFactory::find(m_target)) {
    const auto noSettingsLabel = new QLabel(this);
    noSettingsLabel->setText(tr("No build settings available"));
    auto f = noSettingsLabel->font();
    f.setPointSizeF(f.pointSizeF() * 1.2);
    noSettingsLabel->setFont(f);
    vbox->addWidget(noSettingsLabel);
    return;
  }

  {
    // Edit Build Configuration row
    const auto hbox = new QHBoxLayout();
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->addWidget(new QLabel(tr("Edit build configuration:"), this));
    m_buildConfigurationComboBox = new QComboBox(this);
    m_buildConfigurationComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_buildConfigurationComboBox->setModel(m_target->buildConfigurationModel());
    hbox->addWidget(m_buildConfigurationComboBox);

    m_addButton = new QPushButton(this);
    m_addButton->setText(tr("Add"));
    m_addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hbox->addWidget(m_addButton);
    m_addButtonMenu = new QMenu(this);
    m_addButton->setMenu(m_addButtonMenu);

    m_removeButton = new QPushButton(this);
    m_removeButton->setText(tr("Remove"));
    m_removeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hbox->addWidget(m_removeButton);

    m_renameButton = new QPushButton(this);
    m_renameButton->setText(tr("Rename..."));
    m_renameButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hbox->addWidget(m_renameButton);

    m_cloneButton = new QPushButton(this);
    m_cloneButton->setText(tr("Clone..."));
    m_cloneButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hbox->addWidget(m_cloneButton);

    hbox->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
    vbox->addLayout(hbox);
  }

  m_buildConfiguration = m_target->activeBuildConfiguration();
  m_buildConfigurationComboBox->setCurrentIndex(m_target->buildConfigurationModel()->indexFor(m_buildConfiguration));

  updateAddButtonMenu();
  updateBuildSettings();

  connect(m_buildConfigurationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BuildSettingsWidget::currentIndexChanged);

  connect(m_removeButton, &QAbstractButton::clicked, this, [this]() { deleteConfiguration(m_buildConfiguration); });

  connect(m_renameButton, &QAbstractButton::clicked, this, &BuildSettingsWidget::renameConfiguration);

  connect(m_cloneButton, &QAbstractButton::clicked, this, &BuildSettingsWidget::cloneConfiguration);

  connect(m_target, &Target::activeBuildConfigurationChanged, this, &BuildSettingsWidget::updateActiveConfiguration);

  connect(m_target, &Target::kitChanged, this, &BuildSettingsWidget::updateAddButtonMenu);
}

auto BuildSettingsWidget::addSubWidget(NamedWidget *widget) -> void
{
  widget->setParent(this);
  widget->setContentsMargins(0, 10, 0, 0);

  const auto label = new QLabel(this);
  label->setText(widget->displayName());
  auto f = label->font();
  f.setBold(true);
  f.setPointSizeF(f.pointSizeF() * 1.2);
  label->setFont(f);

  label->setContentsMargins(0, 10, 0, 0);

  layout()->addWidget(label);
  layout()->addWidget(widget);

  m_labels.append(label);
  m_subWidgets.append(widget);
}

auto BuildSettingsWidget::clearWidgets() -> void
{
  qDeleteAll(m_subWidgets);
  m_subWidgets.clear();
  qDeleteAll(m_labels);
  m_labels.clear();
}

auto BuildSettingsWidget::updateAddButtonMenu() -> void
{
  m_addButtonMenu->clear();

  if (m_target) {
    const auto factory = BuildConfigurationFactory::find(m_target);
    if (!factory)
      return;
    for (const auto &info : factory->allAvailableBuilds(m_target)) {
      const auto action = m_addButtonMenu->addAction(info.typeName);
      connect(action, &QAction::triggered, this, [this, info] {
        createConfiguration(info);
      });
    }
  }
}

auto BuildSettingsWidget::updateBuildSettings() -> void
{
  clearWidgets();

  // update buttons
  const auto bcs = m_target->buildConfigurations();
  m_removeButton->setEnabled(bcs.size() > 1);
  m_renameButton->setEnabled(!bcs.isEmpty());
  m_cloneButton->setEnabled(!bcs.isEmpty());

  if (m_buildConfiguration)
    m_buildConfiguration->addConfigWidgets([this](NamedWidget *w) { addSubWidget(w); });
}

auto BuildSettingsWidget::currentIndexChanged(int index) -> void
{
  const auto buildConfiguration = qobject_cast<BuildConfiguration*>(m_target->buildConfigurationModel()->projectConfigurationAt(index));
  SessionManager::setActiveBuildConfiguration(m_target, buildConfiguration, SetActive::Cascade);
}

auto BuildSettingsWidget::updateActiveConfiguration() -> void
{
  if (!m_buildConfiguration || m_buildConfiguration == m_target->activeBuildConfiguration())
    return;

  m_buildConfiguration = m_target->activeBuildConfiguration();

  m_buildConfigurationComboBox->setCurrentIndex(m_target->buildConfigurationModel()->indexFor(m_buildConfiguration));

  updateBuildSettings();
}

auto BuildSettingsWidget::createConfiguration(const BuildInfo &info_) -> void
{
  auto info = info_;
  if (info.displayName.isEmpty()) {
    auto ok = false;
    info.displayName = QInputDialog::getText(Orca::Plugin::Core::ICore::dialogParent(), tr("New Configuration"), tr("New configuration name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || info.displayName.isEmpty())
      return;
  }

  const auto bc = info.factory->create(m_target, info);
  if (!bc)
    return;

  m_target->addBuildConfiguration(bc);
  SessionManager::setActiveBuildConfiguration(m_target, bc, SetActive::Cascade);
}

auto BuildSettingsWidget::uniqueName(const QString &name) -> QString
{
  auto result = name.trimmed();
  if (!result.isEmpty()) {
    QStringList bcNames;
    for (const auto bc : m_target->buildConfigurations()) {
      if (bc == m_buildConfiguration)
        continue;
      bcNames.append(bc->displayName());
    }
    result = makeUniquelyNumbered(result, bcNames);
  }
  return result;
}

auto BuildSettingsWidget::renameConfiguration() -> void
{
  QTC_ASSERT(m_buildConfiguration, return);
  bool ok;
  auto name = QInputDialog::getText(this, tr("Rename..."), tr("New name for build configuration <b>%1</b>:").arg(m_buildConfiguration->displayName()), QLineEdit::Normal, m_buildConfiguration->displayName(), &ok);
  if (!ok)
    return;

  name = uniqueName(name);
  if (name.isEmpty())
    return;

  m_buildConfiguration->setDisplayName(name);
}

auto BuildSettingsWidget::cloneConfiguration() -> void
{
  QTC_ASSERT(m_buildConfiguration, return);
  const auto factory = BuildConfigurationFactory::find(m_target);
  if (!factory)
    return;

  //: Title of a the cloned BuildConfiguration window, text of the window
  const auto name = uniqueName(QInputDialog::getText(this, tr("Clone Configuration"), tr("New configuration name:"), QLineEdit::Normal, m_buildConfiguration->displayName()));
  if (name.isEmpty())
    return;

  const auto bc = BuildConfigurationFactory::clone(m_target, m_buildConfiguration);
  if (!bc)
    return;

  bc->setDisplayName(name);
  const auto buildDirectory = bc->buildDirectory();
  if (buildDirectory != m_target->project()->projectDirectory()) {
    const std::function<bool(const FilePath &)> isBuildDirOk = [this](const FilePath &candidate) {
      if (candidate.exists())
        return false;
      return !anyOf(m_target->buildConfigurations(), [&candidate](const BuildConfiguration *bc) {
        return bc->buildDirectory() == candidate;
      });
    };
    bc->setBuildDirectory(makeUniquelyNumbered(buildDirectory, isBuildDirOk));
  }
  m_target->addBuildConfiguration(bc);
  SessionManager::setActiveBuildConfiguration(m_target, bc, SetActive::Cascade);
}

auto BuildSettingsWidget::deleteConfiguration(BuildConfiguration *deleteConfiguration) -> void
{
  if (!deleteConfiguration || m_target->buildConfigurations().size() <= 1)
    return;

  if (BuildManager::isBuilding(deleteConfiguration)) {
    QMessageBox box;
    const auto closeAnyway = box.addButton(tr("Cancel Build && Remove Build Configuration"), QMessageBox::AcceptRole);
    const auto cancelClose = box.addButton(tr("Do Not Remove"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelClose);
    box.setWindowTitle(tr("Remove Build Configuration %1?").arg(deleteConfiguration->displayName()));
    box.setText(tr("The build configuration <b>%1</b> is currently being built.").arg(deleteConfiguration->displayName()));
    box.setInformativeText(tr("Do you want to cancel the build process and remove the Build Configuration anyway?"));
    box.exec();
    if (box.clickedButton() != closeAnyway)
      return;
    BuildManager::cancel();
  } else {
    QMessageBox msgBox(QMessageBox::Question, tr("Remove Build Configuration?"), tr("Do you really want to delete build configuration <b>%1</b>?").arg(deleteConfiguration->displayName()), QMessageBox::Yes | QMessageBox::No, this);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setEscapeButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::No)
      return;
  }

  m_target->removeBuildConfiguration(deleteConfiguration);
}
