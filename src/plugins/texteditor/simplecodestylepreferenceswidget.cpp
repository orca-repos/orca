// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "simplecodestylepreferenceswidget.hpp"
#include "icodestylepreferences.hpp"
#include "tabsettings.hpp"
#include "tabsettingswidget.hpp"

#include <QVBoxLayout>

namespace TextEditor {

SimpleCodeStylePreferencesWidget::SimpleCodeStylePreferencesWidget(QWidget *parent) : QWidget(parent)
{
  m_tabSettingsWidget = new TabSettingsWidget(this);
  const auto layout = new QVBoxLayout(this);
  layout->addWidget(m_tabSettingsWidget);
  layout->setContentsMargins(QMargins());
  m_tabSettingsWidget->setEnabled(false);
}

auto SimpleCodeStylePreferencesWidget::setPreferences(ICodeStylePreferences *preferences) -> void
{
  if (m_preferences == preferences)
    return; // nothing changes

  // cleanup old
  if (m_preferences) {
    disconnect(m_preferences, &ICodeStylePreferences::currentTabSettingsChanged, m_tabSettingsWidget, &TabSettingsWidget::setTabSettings);
    disconnect(m_preferences, &ICodeStylePreferences::currentPreferencesChanged, this, &SimpleCodeStylePreferencesWidget::slotCurrentPreferencesChanged);
    disconnect(m_tabSettingsWidget, &TabSettingsWidget::settingsChanged, this, &SimpleCodeStylePreferencesWidget::slotTabSettingsChanged);
  }
  m_preferences = preferences;
  // fillup new
  if (m_preferences) {
    slotCurrentPreferencesChanged(m_preferences->currentPreferences());
    m_tabSettingsWidget->setTabSettings(m_preferences->currentTabSettings());

    connect(m_preferences, &ICodeStylePreferences::currentTabSettingsChanged, m_tabSettingsWidget, &TabSettingsWidget::setTabSettings);
    connect(m_preferences, &ICodeStylePreferences::currentPreferencesChanged, this, &SimpleCodeStylePreferencesWidget::slotCurrentPreferencesChanged);
    connect(m_tabSettingsWidget, &TabSettingsWidget::settingsChanged, this, &SimpleCodeStylePreferencesWidget::slotTabSettingsChanged);
  }
  m_tabSettingsWidget->setEnabled(m_preferences);
}

auto SimpleCodeStylePreferencesWidget::slotCurrentPreferencesChanged(ICodeStylePreferences *preferences) -> void
{
  m_tabSettingsWidget->setEnabled(!preferences->isReadOnly() && !m_preferences->currentDelegate());
}

auto SimpleCodeStylePreferencesWidget::slotTabSettingsChanged(const TabSettings &settings) -> void
{
  if (!m_preferences)
    return;

  const auto current = m_preferences->currentPreferences();
  if (!current)
    return;

  current->setTabSettings(settings);
}

auto SimpleCodeStylePreferencesWidget::tabSettingsWidget() const -> TabSettingsWidget*
{
  return m_tabSettingsWidget;
}

} // namespace TextEditor
