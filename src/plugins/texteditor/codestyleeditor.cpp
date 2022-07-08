// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codestyleeditor.hpp"

#include "textdocument.hpp"
#include "icodestylepreferencesfactory.hpp"
#include "icodestylepreferences.hpp"
#include "codestyleselectorwidget.hpp"
#include "displaysettings.hpp"
#include "tabsettings.hpp"
#include "indenter.hpp"

#include <texteditor/snippets/snippeteditor.hpp>
#include <texteditor/snippets/snippetprovider.hpp>

#include <QVBoxLayout>
#include <QTextBlock>
#include <QLabel>

using namespace TextEditor;

CodeStyleEditor::CodeStyleEditor(ICodeStylePreferencesFactory *factory, ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project, QWidget *parent) : CodeStyleEditorWidget(parent), m_factory(factory), m_codeStyle(codeStyle)
{
  m_layout = new QVBoxLayout(this);
  const auto selector = new CodeStyleSelectorWidget(factory, project, this);
  selector->setCodeStyle(codeStyle);
  m_preview = new SnippetEditorWidget(this);
  auto displaySettings = m_preview->displaySettings();
  displaySettings.m_visualizeWhitespace = true;
  m_preview->setDisplaySettings(displaySettings);
  const auto groupId = factory->snippetProviderGroupId();
  SnippetProvider::decorateEditor(m_preview, groupId);
  const auto label = new QLabel(tr("Edit preview contents to see how the current settings " "are applied to custom code snippets. Changes in the preview " "do not affect the current settings."), this);
  auto font = label->font();
  font.setItalic(true);
  label->setFont(font);
  label->setWordWrap(true);
  m_layout->addWidget(selector);
  m_layout->addWidget(m_preview);
  m_layout->addWidget(label);
  connect(codeStyle, &ICodeStylePreferences::currentTabSettingsChanged, this, &CodeStyleEditor::updatePreview);
  connect(codeStyle, &ICodeStylePreferences::currentValueChanged, this, &CodeStyleEditor::updatePreview);
  connect(codeStyle, &ICodeStylePreferences::currentPreferencesChanged, this, &CodeStyleEditor::updatePreview);
  m_preview->setCodeStyle(m_codeStyle);
  m_preview->setPlainText(factory->previewText());

  updatePreview();
}

auto CodeStyleEditor::updatePreview() -> void
{
  const auto doc = m_preview->document();

  m_preview->textDocument()->indenter()->invalidateCache();

  auto block = doc->firstBlock();
  auto tc = m_preview->textCursor();
  tc.beginEditBlock();
  while (block.isValid()) {
    m_preview->textDocument()->indenter()->indentBlock(block, QChar::Null, m_codeStyle->currentTabSettings());
    block = block.next();
  }
  tc.endEditBlock();
}
