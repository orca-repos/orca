// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "editorsettingspropertiespage.hpp"
#include "editorconfiguration.hpp"
#include "project.hpp"

#include <texteditor/behaviorsettings.hpp>
#include <texteditor/extraencodingsettings.hpp>
#include <texteditor/marginsettings.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/typingsettings.hpp>

#include <QTextCodec>

using namespace ProjectExplorer;
using namespace Internal;

EditorSettingsWidget::EditorSettingsWidget(Project *project) : QWidget(), m_project(project)
{
  m_ui.setupUi(this);

  const EditorConfiguration *config = m_project->editorConfiguration();
  settingsToUi(config);

  globalSettingsActivated(config->useGlobalSettings() ? 0 : 1);

  connect(m_ui.globalSelector, QOverload<int>::of(&QComboBox::activated), this, &EditorSettingsWidget::globalSettingsActivated);
  connect(m_ui.restoreButton, &QAbstractButton::clicked, this, &EditorSettingsWidget::restoreDefaultValues);

  connect(m_ui.showWrapColumn, &QAbstractButton::toggled, config, &EditorConfiguration::setShowWrapColumn);
  connect(m_ui.useIndenter, &QAbstractButton::toggled, config, &EditorConfiguration::setUseIndenter);
  connect(m_ui.wrapColumn, QOverload<int>::of(&QSpinBox::valueChanged), config, &EditorConfiguration::setWrapColumn);

  connect(m_ui.behaviorSettingsWidget, &TextEditor::BehaviorSettingsWidget::typingSettingsChanged, config, &EditorConfiguration::setTypingSettings);
  connect(m_ui.behaviorSettingsWidget, &TextEditor::BehaviorSettingsWidget::storageSettingsChanged, config, &EditorConfiguration::setStorageSettings);
  connect(m_ui.behaviorSettingsWidget, &TextEditor::BehaviorSettingsWidget::behaviorSettingsChanged, config, &EditorConfiguration::setBehaviorSettings);
  connect(m_ui.behaviorSettingsWidget, &TextEditor::BehaviorSettingsWidget::extraEncodingSettingsChanged, config, &EditorConfiguration::setExtraEncodingSettings);
  connect(m_ui.behaviorSettingsWidget, &TextEditor::BehaviorSettingsWidget::textCodecChanged, config, &EditorConfiguration::setTextCodec);
}

auto EditorSettingsWidget::settingsToUi(const EditorConfiguration *config) -> void
{
  m_ui.showWrapColumn->setChecked(config->marginSettings().m_showMargin);
  m_ui.useIndenter->setChecked(config->marginSettings().m_useIndenter);
  m_ui.wrapColumn->setValue(config->marginSettings().m_marginColumn);
  m_ui.behaviorSettingsWidget->setCodeStyle(config->codeStyle());
  m_ui.globalSelector->setCurrentIndex(config->useGlobalSettings() ? 0 : 1);
  m_ui.behaviorSettingsWidget->setAssignedCodec(config->textCodec());
  m_ui.behaviorSettingsWidget->setAssignedTypingSettings(config->typingSettings());
  m_ui.behaviorSettingsWidget->setAssignedStorageSettings(config->storageSettings());
  m_ui.behaviorSettingsWidget->setAssignedBehaviorSettings(config->behaviorSettings());
  m_ui.behaviorSettingsWidget->setAssignedExtraEncodingSettings(config->extraEncodingSettings());
}

auto EditorSettingsWidget::globalSettingsActivated(int index) -> void
{
  const auto useGlobal = !index;
  m_ui.displaySettings->setEnabled(!useGlobal);
  m_ui.behaviorSettingsWidget->setActive(!useGlobal);
  m_ui.restoreButton->setEnabled(!useGlobal);
  const auto config = m_project->editorConfiguration();
  config->setUseGlobalSettings(useGlobal);
}

auto EditorSettingsWidget::restoreDefaultValues() -> void
{
  const auto config = m_project->editorConfiguration();
  config->cloneGlobalSettings();
  settingsToUi(config);
}
