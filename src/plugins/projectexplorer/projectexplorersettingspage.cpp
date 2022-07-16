// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectexplorersettingspage.hpp"
#include "projectexplorersettings.hpp"
#include "projectexplorer.hpp"
#include "ui_projectexplorersettingspage.h"

#include <core/core-constants.hpp>
#include <core/core-document-manager.hpp>
#include <utils/hostosinfo.hpp>

#include <QCoreApplication>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

enum {
  UseCurrentDirectory,
  UseProjectDirectory
};

class ProjectExplorerSettingsWidget : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjextExplorer::Internal::ProjectExplorerSettings)

public:
  explicit ProjectExplorerSettingsWidget(QWidget *parent = nullptr);

  auto settings() const -> ProjectExplorerSettings;
  auto setSettings(const ProjectExplorerSettings &s) -> void;
  auto projectsDirectory() const -> FilePath;
  auto setProjectsDirectory(const FilePath &pd) -> void;
  auto useProjectsDirectory() -> bool;
  auto setUseProjectsDirectory(bool v) -> void;

private:
  auto slotDirectoryButtonGroupChanged() -> void;
  auto setJomVisible(bool) -> void;

  Ui::ProjectExplorerSettingsPageUi m_ui;
  mutable ProjectExplorerSettings m_settings;
};

ProjectExplorerSettingsWidget::ProjectExplorerSettingsWidget(QWidget *parent) : QWidget(parent)
{
  m_ui.setupUi(this);
  setJomVisible(HostOsInfo::isWindowsHost());
  m_ui.stopBeforeBuildComboBox->addItem(tr("None"), int(StopBeforeBuild::None));
  m_ui.stopBeforeBuildComboBox->addItem(tr("All"), int(StopBeforeBuild::All));
  m_ui.stopBeforeBuildComboBox->addItem(tr("Same Project"), int(StopBeforeBuild::SameProject));
  m_ui.stopBeforeBuildComboBox->addItem(tr("Same Build Directory"), int(StopBeforeBuild::SameBuildDir));
  m_ui.stopBeforeBuildComboBox->addItem(tr("Same Application"), int(StopBeforeBuild::SameApp));
  m_ui.buildBeforeDeployComboBox->addItem(tr("Do Not Build Anything"), int(BuildBeforeRunMode::Off));
  m_ui.buildBeforeDeployComboBox->addItem(tr("Build the Whole Project"), int(BuildBeforeRunMode::WholeProject));
  m_ui.buildBeforeDeployComboBox->addItem(tr("Build Only the Application to Be Run"), int(BuildBeforeRunMode::AppOnly));
  m_ui.directoryButtonGroup->setId(m_ui.currentDirectoryRadioButton, UseCurrentDirectory);
  m_ui.directoryButtonGroup->setId(m_ui.directoryRadioButton, UseProjectDirectory);

  connect(m_ui.directoryButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &ProjectExplorerSettingsWidget::slotDirectoryButtonGroupChanged);
}

auto ProjectExplorerSettingsWidget::setJomVisible(bool v) -> void
{
  m_ui.jomCheckbox->setVisible(v);
  m_ui.jomLabel->setVisible(v);
}

auto ProjectExplorerSettingsWidget::settings() const -> ProjectExplorerSettings
{
  m_settings.buildBeforeDeploy = static_cast<BuildBeforeRunMode>(m_ui.buildBeforeDeployComboBox->currentData().toInt());
  m_settings.deployBeforeRun = m_ui.deployProjectBeforeRunCheckBox->isChecked();
  m_settings.saveBeforeBuild = m_ui.saveAllFilesCheckBox->isChecked();
  m_settings.useJom = m_ui.jomCheckbox->isChecked();
  m_settings.addLibraryPathsToRunEnv = m_ui.addLibraryPathsToRunEnvCheckBox->isChecked();
  m_settings.prompToStopRunControl = m_ui.promptToStopRunControlCheckBox->isChecked();
  m_settings.automaticallyCreateRunConfigurations = m_ui.automaticallyCreateRunConfiguration->isChecked();
  m_settings.stopBeforeBuild = static_cast<StopBeforeBuild>(m_ui.stopBeforeBuildComboBox->currentData().toInt());
  m_settings.terminalMode = static_cast<TerminalMode>(m_ui.terminalModeComboBox->currentIndex());
  m_settings.closeSourceFilesWithProject = m_ui.closeSourceFilesCheckBox->isChecked();
  m_settings.clearIssuesOnRebuild = m_ui.clearIssuesCheckBox->isChecked();
  m_settings.abortBuildAllOnError = m_ui.abortBuildAllOnErrorCheckBox->isChecked();
  m_settings.lowBuildPriority = m_ui.lowBuildPriorityCheckBox->isChecked();
  return m_settings;
}

auto ProjectExplorerSettingsWidget::setSettings(const ProjectExplorerSettings &pes) -> void
{
  m_settings = pes;
  m_ui.buildBeforeDeployComboBox->setCurrentIndex(m_ui.buildBeforeDeployComboBox->findData(int(m_settings.buildBeforeDeploy)));
  m_ui.deployProjectBeforeRunCheckBox->setChecked(m_settings.deployBeforeRun);
  m_ui.saveAllFilesCheckBox->setChecked(m_settings.saveBeforeBuild);
  m_ui.jomCheckbox->setChecked(m_settings.useJom);
  m_ui.addLibraryPathsToRunEnvCheckBox->setChecked(m_settings.addLibraryPathsToRunEnv);
  m_ui.promptToStopRunControlCheckBox->setChecked(m_settings.prompToStopRunControl);
  m_ui.automaticallyCreateRunConfiguration->setChecked(m_settings.automaticallyCreateRunConfigurations);
  m_ui.stopBeforeBuildComboBox->setCurrentIndex(m_ui.stopBeforeBuildComboBox->findData(int(m_settings.stopBeforeBuild)));
  m_ui.terminalModeComboBox->setCurrentIndex(static_cast<int>(m_settings.terminalMode));
  m_ui.closeSourceFilesCheckBox->setChecked(m_settings.closeSourceFilesWithProject);
  m_ui.clearIssuesCheckBox->setChecked(m_settings.clearIssuesOnRebuild);
  m_ui.abortBuildAllOnErrorCheckBox->setChecked(m_settings.abortBuildAllOnError);
  m_ui.lowBuildPriorityCheckBox->setChecked(m_settings.lowBuildPriority);
}

auto ProjectExplorerSettingsWidget::projectsDirectory() const -> FilePath
{
  return m_ui.projectsDirectoryPathChooser->filePath();
}

auto ProjectExplorerSettingsWidget::setProjectsDirectory(const FilePath &pd) -> void
{
  m_ui.projectsDirectoryPathChooser->setFilePath(pd);
}

auto ProjectExplorerSettingsWidget::useProjectsDirectory() -> bool
{
  return m_ui.directoryButtonGroup->checkedId() == UseProjectDirectory;
}

auto ProjectExplorerSettingsWidget::setUseProjectsDirectory(bool b) -> void
{
  if (useProjectsDirectory() != b) {
    (b ? m_ui.directoryRadioButton : m_ui.currentDirectoryRadioButton)->setChecked(true);
    slotDirectoryButtonGroupChanged();
  }
}

auto ProjectExplorerSettingsWidget::slotDirectoryButtonGroupChanged() -> void
{
  auto enable = useProjectsDirectory();
  m_ui.projectsDirectoryPathChooser->setEnabled(enable);
}

// ------------------ ProjectExplorerSettingsPage
ProjectExplorerSettingsPage::ProjectExplorerSettingsPage()
{
  setId(Constants::BUILD_AND_RUN_SETTINGS_PAGE_ID);
  setDisplayName(ProjectExplorerSettingsWidget::tr("General"));
  setCategory(Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", "Build & Run"));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_buildrun.png");
}

auto ProjectExplorerSettingsPage::widget() -> QWidget*
{
  if (!m_widget) {
    m_widget = new ProjectExplorerSettingsWidget;
    m_widget->setSettings(ProjectExplorerPlugin::projectExplorerSettings());
    m_widget->setProjectsDirectory(DocumentManager::projectsDirectory());
    m_widget->setUseProjectsDirectory(DocumentManager::useProjectsDirectory());
  }
  return m_widget;
}

auto ProjectExplorerSettingsPage::apply() -> void
{
  if (m_widget) {
    ProjectExplorerPlugin::setProjectExplorerSettings(m_widget->settings());
    DocumentManager::setProjectsDirectory(m_widget->projectsDirectory());
    DocumentManager::setUseProjectsDirectory(m_widget->useProjectsDirectory());
  }
}

auto ProjectExplorerSettingsPage::finish() -> void
{
  delete m_widget;
}

} // namespace Internal
} // namespace ProjectExplorer
