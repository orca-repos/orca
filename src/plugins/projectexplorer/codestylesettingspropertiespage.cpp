// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codestylesettingspropertiespage.hpp"
#include "editorconfiguration.hpp"
#include "project.hpp"
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/codestyleeditor.hpp>

using namespace TextEditor;
using namespace ProjectExplorer;
using namespace Internal;

CodeStyleSettingsWidget::CodeStyleSettingsWidget(Project *project) : QWidget(), m_project(project)
{
  m_ui.setupUi(this);

  const EditorConfiguration *config = m_project->editorConfiguration();

  for (const auto factory : TextEditorSettings::codeStyleFactories()) {
    const auto languageId = factory->languageId();
    auto codeStylePreferences = config->codeStyle(languageId);

    auto preview = factory->createCodeStyleEditor(codeStylePreferences, project, m_ui.stackedWidget);
    if (preview && preview->layout())
      preview->layout()->setContentsMargins(QMargins());
    m_ui.stackedWidget->addWidget(preview);
    m_ui.languageComboBox->addItem(factory->displayName());
  }

  connect(m_ui.languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), m_ui.stackedWidget, &QStackedWidget::setCurrentIndex);
}

