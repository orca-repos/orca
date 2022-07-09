// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppquickfixprojectsettingswidget.hpp"
#include "cppquickfixsettingswidget.hpp"
#include "ui_cppquickfixprojectsettingswidget.h"

#include <QFile>

using namespace CppEditor::Internal;

CppQuickFixProjectSettingsWidget::CppQuickFixProjectSettingsWidget(ProjectExplorer::Project *project, QWidget *parent) : QWidget(parent), ui(new Ui::CppQuickFixProjectSettingsWidget)
{
  m_projectSettings = CppQuickFixProjectsSettings::getSettings(project);
  ui->setupUi(this);
  m_settingsWidget = new CppEditor::Internal::CppQuickFixSettingsWidget(this);
  m_settingsWidget->loadSettings(m_projectSettings->getSettings());
  if (auto layout = m_settingsWidget->layout())
    layout->setContentsMargins(0, 0, 0, 0);
  ui->layout->addWidget(m_settingsWidget);
  connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CppQuickFixProjectSettingsWidget::currentItemChanged);
  connect(ui->pushButton_custom, &QAbstractButton::clicked, this, &CppQuickFixProjectSettingsWidget::buttonCustomClicked);
  connect(m_settingsWidget, &CppEditor::Internal::CppQuickFixSettingsWidget::settingsChanged, [this] {
    m_settingsWidget->saveSettings(m_projectSettings->getSettings());
    if (!useGlobalSettings())
      m_projectSettings->saveOwnSettings();
  });
  ui->comboBox->setCurrentIndex(m_projectSettings->isUsingGlobalSettings() ? 0 : 1);
}

CppQuickFixProjectSettingsWidget::~CppQuickFixProjectSettingsWidget()
{
  delete ui;
}

auto CppQuickFixProjectSettingsWidget::currentItemChanged() -> void
{
  if (useGlobalSettings()) {
    const auto &path = m_projectSettings->filePathOfSettingsFile();
    ui->pushButton_custom->setToolTip(tr("Custom settings are saved in a file. If you use the " "global settings, you can delete that file."));
    ui->pushButton_custom->setText(tr("Delete Custom Settings File"));
    ui->pushButton_custom->setVisible(!path.isEmpty() && path.exists());
    m_projectSettings->useGlobalSettings();
  } else /*Custom*/ {
    if (!m_projectSettings->useCustomSettings()) {
      ui->comboBox->setCurrentIndex(0);
      return;
    }
    ui->pushButton_custom->setToolTip(tr("Resets all settings to the global settings."));
    ui->pushButton_custom->setText(tr("Reset to Global"));
    ui->pushButton_custom->setVisible(true);
    // otherwise you change the comboBox and exit and have no custom settings:
    m_projectSettings->saveOwnSettings();
  }
  m_settingsWidget->loadSettings(m_projectSettings->getSettings());
}

auto CppQuickFixProjectSettingsWidget::buttonCustomClicked() -> void
{
  if (useGlobalSettings()) {
    // delete file
    QFile::remove(m_projectSettings->filePathOfSettingsFile().toString());
    ui->pushButton_custom->setVisible(false);
  } else /*Custom*/ {
    m_projectSettings->resetOwnSettingsToGlobal();
    m_projectSettings->saveOwnSettings();
    m_settingsWidget->loadSettings(m_projectSettings->getSettings());
  }
}

auto CppQuickFixProjectSettingsWidget::useGlobalSettings() -> bool
{
  return ui->comboBox->currentIndex() == 0;
}
