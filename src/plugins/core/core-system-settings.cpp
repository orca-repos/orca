// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-system-settings.hpp"
#include "ui_core-system-settings.h"

#include "core-constants.hpp"
#include "core-editor-manager-private.hpp"
#include "core-file-utils.hpp"
#include "core-interface.hpp"
#include "core-main-window.hpp"
#include "core-patch-tool.hpp"
#include "core-plugin.hpp"
#include "core-restart-dialog.hpp"
#include "core-vcs-manager.hpp"
#include "core-version-control-interface.hpp"

#include <app/app_version.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/environmentdialog.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/terminalcommand.hpp>
#include <utils/unixutils.hpp>

#include <QCoreApplication>
#include <QMessageBox>
#include <QSettings>

using namespace Utils;

namespace Orca::Plugin::Core {

#ifdef ENABLE_CRASHPAD
const char crashReportingEnabledKey[] = "CrashReportingEnabled";
const char showCrashButtonKey[] = "ShowCrashButton";

// TODO: move to somewhere in Utils
static QString formatSize(qint64 size)
{
  QStringList units{QObject::tr("Bytes"), QObject::tr("KB"), QObject::tr("MB"), QObject::tr("GB"), QObject::tr("TB")};
  double outputSize = size;
  int i;
  for (i = 0; i < units.size() - 1; ++i) {
    if (outputSize < 1024)
      break;
    outputSize /= 1024;
  }
  return i == 0
           ? QString("%0 %1").arg(outputSize).arg(units[i])             // Bytes
           : QString("%0 %1").arg(outputSize, 0, 'f', 2).arg(units[i]); // KB, MB, GB, TB
}
#endif // ENABLE_CRASHPAD

class SystemSettingsWidget final : public IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(Core::SystemSettingsWidget)

public:
  SystemSettingsWidget()
  {
    m_ui.setupUi(this);
    m_ui.terminalOpenArgs->setToolTip(tr("Command line arguments used for \"%1\".").arg(FileUtils::msgTerminalHereAction()));
    m_ui.reloadBehavior->setCurrentIndex(EditorManager::reloadSetting());
    if constexpr (HostOsInfo::isAnyUnixHost()) {
      for (const auto available_terminals = TerminalCommand::availableTerminalEmulators(); const auto &term : available_terminals)
        m_ui.terminalComboBox->addItem(term.command, QVariant::fromValue(term));
      updateTerminalUi(TerminalCommand::terminalEmulator());
      connect(m_ui.terminalComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        updateTerminalUi(m_ui.terminalComboBox->itemData(index).value<TerminalCommand>());
      });
    } else {
      m_ui.terminalLabel->hide();
      m_ui.terminalComboBox->hide();
      m_ui.terminalOpenArgs->hide();
      m_ui.terminalExecuteArgs->hide();
      m_ui.resetTerminalButton->hide();
    }

    if constexpr (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost()) {
      m_ui.externalFileBrowserEdit->setText(UnixUtils::fileBrowser(ICore::settings()));
    } else {
      m_ui.externalFileBrowserLabel->hide();
      m_ui.externalFileBrowserWidget->hide();
    }

    const auto patch_tool_tip = tr("Command used for reverting diff chunks.");
    m_ui.patchCommandLabel->setToolTip(patch_tool_tip);
    m_ui.patchChooser->setToolTip(patch_tool_tip);
    m_ui.patchChooser->setExpectedKind(PathChooser::ExistingCommand);
    m_ui.patchChooser->setHistoryCompleter(QLatin1String("General.PatchCommand.History"));
    m_ui.patchChooser->setFilePath(PatchTool::patchCommand());
    m_ui.autoSaveCheckBox->setChecked(EditorManagerPrivate::autoSaveEnabled());
    m_ui.autoSaveCheckBox->setToolTip(tr("Automatically creates temporary copies of " "modified files. If %1 is restarted after " "a crash or power failure, it asks whether to " "recover the auto-saved content.").arg(IDE_DISPLAY_NAME));
    m_ui.autoSaveRefactoringCheckBox->setChecked(EditorManager::autoSaveAfterRefactoring());
    m_ui.autoSaveRefactoringCheckBox->setToolTip(tr("Automatically saves all open files " "affected by a refactoring operation,\n provided they were unmodified before the " "refactoring."));
    m_ui.autoSaveInterval->setValue(EditorManagerPrivate::autoSaveInterval());
    m_ui.autoSuspendCheckBox->setChecked(EditorManagerPrivate::autoSuspendEnabled());
    m_ui.autoSuspendMinDocumentCount->setValue(EditorManagerPrivate::autoSuspendMinDocumentCount());
    m_ui.warnBeforeOpeningBigFiles->setChecked(EditorManagerPrivate::warnBeforeOpeningBigFilesEnabled());
    m_ui.bigFilesLimitSpinBox->setValue(EditorManagerPrivate::bigFileSizeLimit());
    m_ui.maxRecentFilesSpinBox->setMinimum(1);
    m_ui.maxRecentFilesSpinBox->setMaximum(99);
    m_ui.maxRecentFilesSpinBox->setValue(EditorManagerPrivate::maxRecentFiles());
    #ifdef ENABLE_CRASHPAD
    if (ICore::settings()->value(showCrashButtonKey).toBool()) {
      auto crashButton = new QPushButton("CRASH!!!");
      crashButton->show();
      connect(crashButton, &QPushButton::clicked, []() {
        // do a real crash
        volatile int *a = reinterpret_cast<volatile int*>(NULL);
        *a = 1;
      });
    }

    m_ui.enableCrashReportingCheckBox->setChecked(ICore::settings()->value(crashReportingEnabledKey).toBool());
    connect(m_ui.helpCrashReportingButton, &QAbstractButton::clicked, this, [this] {
      showHelpDialog(tr("Crash Reporting"), CorePlugin::msgCrashpadInformation());
    });
    connect(m_ui.enableCrashReportingCheckBox, QOverload<int>::of(&QCheckBox::stateChanged), this, [this] {
      const QString restartText = tr("The change will take effect after restart.");
      Core::RestartDialog restartDialog(Core::ICore::dialogParent(), restartText);
      restartDialog.exec();
      if (restartDialog.result() == QDialog::Accepted)
        apply();
    });

    updateClearCrashWidgets();
    connect(m_ui.clearCrashReportsButton, &QPushButton::clicked, this, [&] {
      QDir crashReportsDir = ICore::crashReportsPath().toDir();
      crashReportsDir.setFilter(QDir::Files);
      const QStringList crashFiles = crashReportsDir.entryList();
      for (QString file : crashFiles)
        crashReportsDir.remove(file);
      updateClearCrashWidgets();
    });
    #else
    m_ui.enableCrashReportingCheckBox->setVisible(false);
    m_ui.helpCrashReportingButton->setVisible(false);
    m_ui.clearCrashReportsButton->setVisible(false);
    m_ui.crashReportsSizeText->setVisible(false);
    #endif

    m_ui.askBeforeExitCheckBox->setChecked(dynamic_cast<MainWindow*>(ICore::mainWindow())->askConfirmationBeforeExit());

    if constexpr (HostOsInfo::isAnyUnixHost()) {
      connect(m_ui.resetTerminalButton, &QAbstractButton::clicked, this, &SystemSettingsWidget::resetTerminal);
      if constexpr (!HostOsInfo::isMacHost()) {
        connect(m_ui.resetFileBrowserButton, &QAbstractButton::clicked, this, &SystemSettingsWidget::resetFileBrowser);
        connect(m_ui.helpExternalFileBrowserButton, &QAbstractButton::clicked, this, &SystemSettingsWidget::showHelpForFileBrowser);
      }
    }

    if constexpr (HostOsInfo::isMacHost()) {
      auto default_sensitivity = OsSpecificAspects::fileNameCaseSensitivity(HostOsInfo::hostOs());
      if (default_sensitivity == Qt::CaseSensitive) {
        m_ui.fileSystemCaseSensitivityChooser->addItem(tr("Case Sensitive (Default)"), Qt::CaseSensitive);
      } else {
        m_ui.fileSystemCaseSensitivityChooser->addItem(tr("Case Sensitive"), Qt::CaseSensitive);
      }
      if (default_sensitivity == Qt::CaseInsensitive) {
        m_ui.fileSystemCaseSensitivityChooser->addItem(tr("Case Insensitive (Default)"), Qt::CaseInsensitive);
      } else {
        m_ui.fileSystemCaseSensitivityChooser->addItem(tr("Case Insensitive"), Qt::CaseInsensitive);
      }
      if (const auto sensitivity = EditorManagerPrivate::readFileSystemSensitivity(ICore::settings()); sensitivity == Qt::CaseSensitive)
        m_ui.fileSystemCaseSensitivityChooser->setCurrentIndex(0);
      else
        m_ui.fileSystemCaseSensitivityChooser->setCurrentIndex(1);
    } else {
      m_ui.fileSystemCaseSensitivityLabel->hide();
      m_ui.fileSystemCaseSensitivityWidget->hide();
    }

    updatePath();

    m_ui.environmentChangesLabel->setElideMode(Qt::ElideRight);
    m_environment_changes = CorePlugin::environmentChanges();
    updateEnvironmentChangesLabel();

    connect(m_ui.environmentButton, &QPushButton::clicked, [this] {
      const auto changes = EnvironmentDialog::getEnvironmentItems(m_ui.environmentButton, m_environment_changes);
      if (!changes)
        return;
      m_environment_changes = *changes;
      updateEnvironmentChangesLabel();
      updatePath();
    });

    connect(VcsManager::instance(), &VcsManager::configurationChanged, this, &SystemSettingsWidget::updatePath);
  }

private:
  auto apply() -> void final;
  auto showHelpForFileBrowser() -> void;
  auto resetFileBrowser() const -> void;
  auto resetTerminal() const -> void;
  auto updateTerminalUi(const TerminalCommand &term) const -> void;
  auto updatePath() const -> void;
  auto updateEnvironmentChangesLabel() const -> void;
  auto updateClearCrashWidgets() -> void;
  auto showHelpDialog(const QString &title, const QString &help_text) -> void;

  Ui::SystemSettings m_ui{};
  QPointer<QMessageBox> m_dialog;
  EnvironmentItems m_environment_changes;
};

auto SystemSettingsWidget::apply() -> void
{
  auto settings = ICore::settings();
  EditorManager::setReloadSetting(static_cast<IDocument::ReloadSetting>(m_ui.reloadBehavior->currentIndex()));

  if constexpr (HostOsInfo::isAnyUnixHost()) {
    TerminalCommand::setTerminalEmulator({m_ui.terminalComboBox->lineEdit()->text(), m_ui.terminalOpenArgs->text(), m_ui.terminalExecuteArgs->text()});
    if constexpr (!HostOsInfo::isMacHost()) {
      UnixUtils::setFileBrowser(settings, m_ui.externalFileBrowserEdit->text());
    }
  }

  PatchTool::setPatchCommand(m_ui.patchChooser->filePath());
  EditorManagerPrivate::setAutoSaveEnabled(m_ui.autoSaveCheckBox->isChecked());
  EditorManagerPrivate::setAutoSaveInterval(m_ui.autoSaveInterval->value());
  EditorManagerPrivate::setAutoSaveAfterRefactoring(m_ui.autoSaveRefactoringCheckBox->isChecked());
  EditorManagerPrivate::setAutoSuspendEnabled(m_ui.autoSuspendCheckBox->isChecked());
  EditorManagerPrivate::setAutoSuspendMinDocumentCount(m_ui.autoSuspendMinDocumentCount->value());
  EditorManagerPrivate::setWarnBeforeOpeningBigFilesEnabled(m_ui.warnBeforeOpeningBigFiles->isChecked());
  EditorManagerPrivate::setBigFileSizeLimit(m_ui.bigFilesLimitSpinBox->value());
  EditorManagerPrivate::setMaxRecentFiles(m_ui.maxRecentFilesSpinBox->value());
  #ifdef ENABLE_CRASHPAD
  ICore::settings()->setValue(crashReportingEnabledKey, m_ui.enableCrashReportingCheckBox->isChecked());
  #endif

  dynamic_cast<MainWindow*>(ICore::mainWindow())->setAskConfirmationBeforeExit(m_ui.askBeforeExitCheckBox->isChecked());

  if constexpr (HostOsInfo::isMacHost()) {
    const auto sensitivity = EditorManagerPrivate::readFileSystemSensitivity(settings);
    if (const auto selected_sensitivity = static_cast<Qt::CaseSensitivity>(m_ui.fileSystemCaseSensitivityChooser->currentData().toInt()); selected_sensitivity != sensitivity) {
      EditorManagerPrivate::writeFileSystemSensitivity(settings, selected_sensitivity);
      RestartDialog dialog(ICore::dialogParent(), tr("The file system case sensitivity change will take effect after restart."));
      dialog.exec();
    }
  }

  CorePlugin::setEnvironmentChanges(m_environment_changes);
}

auto SystemSettingsWidget::resetTerminal() const -> void
{
  if constexpr (HostOsInfo::isAnyUnixHost())
    m_ui.terminalComboBox->setCurrentIndex(0);
}

auto SystemSettingsWidget::updateTerminalUi(const TerminalCommand &term) const -> void
{
  m_ui.terminalComboBox->lineEdit()->setText(term.command);
  m_ui.terminalOpenArgs->setText(term.openArgs);
  m_ui.terminalExecuteArgs->setText(term.executeArgs);
}

auto SystemSettingsWidget::resetFileBrowser() const -> void
{
  if constexpr (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost())
    m_ui.externalFileBrowserEdit->setText(UnixUtils::defaultFileBrowser());
}

auto SystemSettingsWidget::updatePath() const -> void
{
  EnvironmentChange change;
  change.addAppendToPath(VcsManager::additionalToolsPath());
  m_ui.patchChooser->setEnvironmentChange(change);
}

auto SystemSettingsWidget::updateEnvironmentChangesLabel() const -> void
{
  const auto short_summary = EnvironmentItem::toStringList(m_environment_changes).join("; ");
  m_ui.environmentChangesLabel->setText(short_summary.isEmpty() ? tr("No changes to apply.") : short_summary);
}

auto SystemSettingsWidget::showHelpDialog(const QString &title, const QString &help_text) -> void
{
  if (m_dialog) {
    if (m_dialog->windowTitle() != title)
      m_dialog->setText(help_text);
    if (m_dialog->text() != help_text)
      m_dialog->setText(help_text);
    m_dialog->show();
    ICore::raiseWindow(m_dialog);
    return;
  }

  const auto mb = new QMessageBox(QMessageBox::Information, title, help_text, QMessageBox::Close, this);
  mb->setWindowModality(Qt::NonModal);
  m_dialog = mb;
  mb->show();
}

#ifdef ENABLE_CRASHPAD
void SystemSettingsWidget::updateClearCrashWidgets()
{
  QDir crashReportsDir(ICore::crashReportsPath().toDir());
  crashReportsDir.setFilter(QDir::Files);
  qint64 size = 0;
  const QStringList crashFiles = crashReportsDir.entryList();
  for (QString file : crashFiles)
    size += QFileInfo(crashReportsDir, file).size();

  m_ui.clearCrashReportsButton->setEnabled(!crashFiles.isEmpty());
  m_ui.crashReportsSizeText->setText(formatSize(size));
}
#endif

auto SystemSettingsWidget::showHelpForFileBrowser() -> void
{
  if constexpr (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost())
    showHelpDialog(tr("Variables"), UnixUtils::fileBrowserHelpText());
}

SystemSettings::SystemSettings()
{
  setId(SETTINGS_ID_SYSTEM);
  setDisplayName(SystemSettingsWidget::tr("System"));
  setCategory(SETTINGS_CATEGORY_CORE);
  setWidgetCreator([] { return new SystemSettingsWidget; });
}

} // namespace Orca::Plugin::Core
