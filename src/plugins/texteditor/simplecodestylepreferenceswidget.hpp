// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QWidget>

namespace TextEditor {

class TabSettings;
class TabSettingsWidget;
class ICodeStylePreferences;

namespace Ui {
class TabPreferencesWidget;
}

class TEXTEDITOR_EXPORT SimpleCodeStylePreferencesWidget : public QWidget {
  Q_OBJECT

public:
  explicit SimpleCodeStylePreferencesWidget(QWidget *parent = nullptr);

  auto setPreferences(ICodeStylePreferences *tabPreferences) -> void;
  auto tabSettingsWidget() const -> TabSettingsWidget*;

private:
  auto slotCurrentPreferencesChanged(ICodeStylePreferences *preferences) -> void;
  auto slotTabSettingsChanged(const TabSettings &settings) -> void;

  TabSettingsWidget *m_tabSettingsWidget;
  ICodeStylePreferences *m_preferences = nullptr;
};

} // namespace TextEditor
