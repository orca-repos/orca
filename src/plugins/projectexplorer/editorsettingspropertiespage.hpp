// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_editorsettingspropertiespage.h"

namespace ProjectExplorer {
class EditorConfiguration;
class Project;

namespace Internal {

class EditorSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit EditorSettingsWidget(Project *project);

private:
  auto globalSettingsActivated(int index) -> void;
  auto restoreDefaultValues() -> void;
  auto settingsToUi(const EditorConfiguration *config) -> void;

  Ui::EditorSettingsPropertiesPage m_ui;
  Project *m_project;
};

} // namespace Internal
} // namespace ProjectExplorer
