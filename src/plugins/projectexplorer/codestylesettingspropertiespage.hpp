// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_codestylesettingspropertiespage.h"

namespace ProjectExplorer {
class EditorConfiguration;
class Project;

namespace Internal {

class CodeStyleSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit CodeStyleSettingsWidget(Project *project);

private:
  Ui::CodeStyleSettingsPropertiesPage m_ui;
  Project *m_project;
};

} // namespace Internal
} // namespace ProjectExplorer
