// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sshsettingspage.hpp"

#include <core/core-interface.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <ssh/sshsettings.h>
#include <utils/hostosinfo.hpp>
#include <utils/pathchooser.hpp>

#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QSpinBox>

using namespace QSsh;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class SshSettingsWidget : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::SshSettingsWidget)

public:
  SshSettingsWidget();

  auto saveSettings() -> void;

private:
  auto apply() -> void final { saveSettings(); }
  auto setupConnectionSharingCheckBox() -> void;
  auto setupConnectionSharingSpinBox() -> void;
  auto setupSshPathChooser() -> void;
  auto setupSftpPathChooser() -> void;
  auto setupAskpassPathChooser() -> void;
  auto setupKeygenPathChooser() -> void;
  auto setupPathChooser(PathChooser &chooser, const FilePath &initialPath, bool &changedFlag) -> void;
  auto updateCheckboxEnabled() -> void;
  auto updateSpinboxEnabled() -> void;

  QCheckBox m_connectionSharingCheckBox;
  QSpinBox m_connectionSharingSpinBox;
  PathChooser m_sshChooser;
  PathChooser m_sftpChooser;
  PathChooser m_askpassChooser;
  PathChooser m_keygenChooser;
  bool m_sshPathChanged = false;
  bool m_sftpPathChanged = false;
  bool m_askpassPathChanged = false;
  bool m_keygenPathChanged = false;
};

SshSettingsPage::SshSettingsPage()
{
  setId(Constants::SSH_SETTINGS_PAGE_ID);
  setDisplayName(SshSettingsWidget::tr("SSH"));
  setCategory(Constants::DEVICE_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", "SSH"));
  setCategoryIconPath(":/projectexplorer/images/settingscategory_devices.png");
  setWidgetCreator([] { return new SshSettingsWidget; });
}

SshSettingsWidget::SshSettingsWidget()
{
  setupConnectionSharingCheckBox();
  setupConnectionSharingSpinBox();
  setupSshPathChooser();
  setupSftpPathChooser();
  setupAskpassPathChooser();
  setupKeygenPathChooser();
  auto *const layout = new QFormLayout(this);
  layout->addRow(tr("Enable connection sharing:"), &m_connectionSharingCheckBox);
  layout->addRow(tr("Connection sharing timeout:"), &m_connectionSharingSpinBox);
  layout->addRow(tr("Path to ssh executable:"), &m_sshChooser);
  layout->addRow(tr("Path to sftp executable:"), &m_sftpChooser);
  layout->addRow(tr("Path to ssh-askpass executable:"), &m_askpassChooser);
  layout->addRow(tr("Path to ssh-keygen executable:"), &m_keygenChooser);
  updateCheckboxEnabled();
  updateSpinboxEnabled();
}

auto SshSettingsWidget::saveSettings() -> void
{
  SshSettings::setConnectionSharingEnabled(m_connectionSharingCheckBox.isChecked());
  SshSettings::setConnectionSharingTimeout(m_connectionSharingSpinBox.value());
  if (m_sshPathChanged)
    SshSettings::setSshFilePath(m_sshChooser.filePath());
  if (m_sftpPathChanged)
    SshSettings::setSftpFilePath(m_sftpChooser.filePath());
  if (m_askpassPathChanged)
    SshSettings::setAskpassFilePath(m_askpassChooser.filePath());
  if (m_keygenPathChanged)
    SshSettings::setKeygenFilePath(m_keygenChooser.filePath());
  SshSettings::storeSettings(Orca::Plugin::Core::ICore::settings());
}

auto SshSettingsWidget::setupConnectionSharingCheckBox() -> void
{
  m_connectionSharingCheckBox.setChecked(SshSettings::connectionSharingEnabled());
  connect(&m_connectionSharingCheckBox, &QCheckBox::toggled, this, &SshSettingsWidget::updateSpinboxEnabled);
}

auto SshSettingsWidget::setupConnectionSharingSpinBox() -> void
{
  m_connectionSharingSpinBox.setMinimum(1);
  m_connectionSharingSpinBox.setValue(SshSettings::connectionSharingTimeout());
  m_connectionSharingSpinBox.setSuffix(tr(" minutes"));
}

auto SshSettingsWidget::setupSshPathChooser() -> void
{
  setupPathChooser(m_sshChooser, SshSettings::sshFilePath(), m_sshPathChanged);
}

auto SshSettingsWidget::setupSftpPathChooser() -> void
{
  setupPathChooser(m_sftpChooser, SshSettings::sftpFilePath(), m_sftpPathChanged);
}

auto SshSettingsWidget::setupAskpassPathChooser() -> void
{
  setupPathChooser(m_askpassChooser, SshSettings::askpassFilePath(), m_askpassPathChanged);
}

auto SshSettingsWidget::setupKeygenPathChooser() -> void
{
  setupPathChooser(m_keygenChooser, SshSettings::keygenFilePath(), m_keygenPathChanged);
}

auto SshSettingsWidget::setupPathChooser(PathChooser &chooser, const FilePath &initialPath, bool &changedFlag) -> void
{
  chooser.setExpectedKind(PathChooser::ExistingCommand);
  chooser.setFilePath(initialPath);
  connect(&chooser, &PathChooser::pathChanged, [&changedFlag] { changedFlag = true; });
}

auto SshSettingsWidget::updateCheckboxEnabled() -> void
{
  if (!HostOsInfo::isWindowsHost())
    return;
  m_connectionSharingCheckBox.setEnabled(false);
  static_cast<QFormLayout*>(layout())->labelForField(&m_connectionSharingCheckBox)->setEnabled(false);
}

auto SshSettingsWidget::updateSpinboxEnabled() -> void
{
  m_connectionSharingSpinBox.setEnabled(m_connectionSharingCheckBox.isChecked());
  static_cast<QFormLayout*>(layout())->labelForField(&m_connectionSharingSpinBox)->setEnabled(m_connectionSharingCheckBox.isChecked());
}

} // namespace Internal
} // namespace ProjectExplorer
