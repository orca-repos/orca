// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodestylesettingspage.hpp"

#include "cppcodestylepreferences.hpp"
#include "cppcodestylesnippets.hpp"
#include "cppeditorconstants.hpp"
#include "cpppointerdeclarationformatter.hpp"
#include "cppqtstyleindenter.hpp"
#include "cpptoolssettings.hpp"
#include <ui_cppcodestylesettingspage.h>

#include <core/icore.hpp>
#include <cppeditor/cppeditorconstants.hpp>
#include <texteditor/codestyleeditor.hpp>
#include <texteditor/fontsettings.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/displaysettings.hpp>
#include <texteditor/snippets/snippetprovider.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <utils/qtcassert.hpp>

#include <cplusplus/Overview.h>
#include <cplusplus/pp.h>

#include <extensionsystem/pluginmanager.hpp>

#include <QTextBlock>
#include <QTextStream>

using namespace TextEditor;

namespace CppEditor::Internal {

static auto applyRefactorings(QTextDocument *textDocument, TextEditorWidget *editor, const CppCodeStyleSettings &settings) -> void
{
  // Preprocess source
  Environment env;
  Preprocessor preprocess(nullptr, &env);
  const QByteArray preprocessedSource = preprocess.run(QLatin1String("<no-file>"), textDocument->toPlainText());

  Document::Ptr cppDocument = Document::create(QLatin1String("<no-file>"));
  cppDocument->setUtf8Source(preprocessedSource);
  cppDocument->parse(Document::ParseTranlationUnit);
  cppDocument->check();

  CppRefactoringFilePtr cppRefactoringFile = CppRefactoringChanges::file(editor, cppDocument);

  // Run the formatter
  Overview overview;
  overview.showReturnTypes = true;
  overview.starBindFlags = {};

  if (settings.bindStarToIdentifier)
    overview.starBindFlags |= Overview::BindToIdentifier;
  if (settings.bindStarToTypeName)
    overview.starBindFlags |= Overview::BindToTypeName;
  if (settings.bindStarToLeftSpecifier)
    overview.starBindFlags |= Overview::BindToLeftSpecifier;
  if (settings.bindStarToRightSpecifier)
    overview.starBindFlags |= Overview::BindToRightSpecifier;

  PointerDeclarationFormatter formatter(cppRefactoringFile, overview);
  Utils::ChangeSet change = formatter.format(cppDocument->translationUnit()->ast());

  // Apply change
  QTextCursor cursor(textDocument);
  change.apply(&cursor);
}

// ------------------ CppCodeStyleSettingsWidget

CppCodeStylePreferencesWidget::CppCodeStylePreferencesWidget(QWidget *parent) : QWidget(parent), m_ui(new Ui::CppCodeStyleSettingsPage)
{
  m_ui->setupUi(this);
  m_ui->categoryTab->setProperty("_q_custom_style_disabled", true);

  m_previews << m_ui->previewTextEditGeneral << m_ui->previewTextEditContent << m_ui->previewTextEditBraces << m_ui->previewTextEditSwitch << m_ui->previewTextEditPadding << m_ui->previewTextEditPointerReferences;
  for (auto i = 0; i < m_previews.size(); ++i)
    m_previews[i]->setPlainText(QLatin1String(Constants::DEFAULT_CODE_STYLE_SNIPPETS[i]));

  decorateEditors(TextEditorSettings::fontSettings());
  connect(TextEditorSettings::instance(), &TextEditorSettings::fontSettingsChanged, this, &CppCodeStylePreferencesWidget::decorateEditors);

  setVisualizeWhitespace(true);

  connect(m_ui->tabSettingsWidget, &TabSettingsWidget::settingsChanged, this, &CppCodeStylePreferencesWidget::slotTabSettingsChanged);
  connect(m_ui->indentBlockBraces, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentBlockBody, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentClassBraces, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentNamespaceBraces, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentEnumBraces, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentNamespaceBody, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentSwitchLabels, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentCaseStatements, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentCaseBlocks, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentCaseBreak, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentAccessSpecifiers, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentDeclarationsRelativeToAccessSpecifiers, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentFunctionBody, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->indentFunctionBraces, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->extraPaddingConditions, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->alignAssignments, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->bindStarToIdentifier, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->bindStarToTypeName, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->bindStarToLeftSpecifier, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);
  connect(m_ui->bindStarToRightSpecifier, &QCheckBox::toggled, this, &CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged);

  m_ui->categoryTab->setCurrentIndex(0);
}

CppCodeStylePreferencesWidget::~CppCodeStylePreferencesWidget()
{
  delete m_ui;
}

auto CppCodeStylePreferencesWidget::setCodeStyle(CppCodeStylePreferences *codeStylePreferences) -> void
{
  // code preferences
  m_preferences = codeStylePreferences;

  connect(m_preferences, &CppCodeStylePreferences::currentTabSettingsChanged, this, &CppCodeStylePreferencesWidget::setTabSettings);
  connect(m_preferences, &CppCodeStylePreferences::currentCodeStyleSettingsChanged, this, [this](const CppCodeStyleSettings &codeStyleSettings) {
    setCodeStyleSettings(codeStyleSettings);
  });

  connect(m_preferences, &ICodeStylePreferences::currentPreferencesChanged, this, [this](TextEditor::ICodeStylePreferences *currentPreferences) {
    slotCurrentPreferencesChanged(currentPreferences);
  });

  setTabSettings(m_preferences->tabSettings());
  setCodeStyleSettings(m_preferences->codeStyleSettings(), false);
  slotCurrentPreferencesChanged(m_preferences->currentPreferences(), false);

  updatePreview();
}

auto CppCodeStylePreferencesWidget::cppCodeStyleSettings() const -> CppCodeStyleSettings
{
  CppCodeStyleSettings set;

  set.indentBlockBraces = m_ui->indentBlockBraces->isChecked();
  set.indentBlockBody = m_ui->indentBlockBody->isChecked();
  set.indentClassBraces = m_ui->indentClassBraces->isChecked();
  set.indentEnumBraces = m_ui->indentEnumBraces->isChecked();
  set.indentNamespaceBraces = m_ui->indentNamespaceBraces->isChecked();
  set.indentNamespaceBody = m_ui->indentNamespaceBody->isChecked();
  set.indentAccessSpecifiers = m_ui->indentAccessSpecifiers->isChecked();
  set.indentDeclarationsRelativeToAccessSpecifiers = m_ui->indentDeclarationsRelativeToAccessSpecifiers->isChecked();
  set.indentFunctionBody = m_ui->indentFunctionBody->isChecked();
  set.indentFunctionBraces = m_ui->indentFunctionBraces->isChecked();
  set.indentSwitchLabels = m_ui->indentSwitchLabels->isChecked();
  set.indentStatementsRelativeToSwitchLabels = m_ui->indentCaseStatements->isChecked();
  set.indentBlocksRelativeToSwitchLabels = m_ui->indentCaseBlocks->isChecked();
  set.indentControlFlowRelativeToSwitchLabels = m_ui->indentCaseBreak->isChecked();
  set.bindStarToIdentifier = m_ui->bindStarToIdentifier->isChecked();
  set.bindStarToTypeName = m_ui->bindStarToTypeName->isChecked();
  set.bindStarToLeftSpecifier = m_ui->bindStarToLeftSpecifier->isChecked();
  set.bindStarToRightSpecifier = m_ui->bindStarToRightSpecifier->isChecked();
  set.extraPaddingForConditionsIfConfusingAlign = m_ui->extraPaddingConditions->isChecked();
  set.alignAssignments = m_ui->alignAssignments->isChecked();

  return set;
}

auto CppCodeStylePreferencesWidget::setTabSettings(const TabSettings &settings) -> void
{
  m_ui->tabSettingsWidget->setTabSettings(settings);
}

auto CppCodeStylePreferencesWidget::tabSettings() const -> TextEditor::TabSettings
{
  return m_ui->tabSettingsWidget->tabSettings();
}

auto CppCodeStylePreferencesWidget::setCodeStyleSettings(const CppCodeStyleSettings &s, bool preview) -> void
{
  const auto wasBlocked = m_blockUpdates;
  m_blockUpdates = true;
  m_ui->indentBlockBraces->setChecked(s.indentBlockBraces);
  m_ui->indentBlockBody->setChecked(s.indentBlockBody);
  m_ui->indentClassBraces->setChecked(s.indentClassBraces);
  m_ui->indentEnumBraces->setChecked(s.indentEnumBraces);
  m_ui->indentNamespaceBraces->setChecked(s.indentNamespaceBraces);
  m_ui->indentNamespaceBody->setChecked(s.indentNamespaceBody);
  m_ui->indentAccessSpecifiers->setChecked(s.indentAccessSpecifiers);
  m_ui->indentDeclarationsRelativeToAccessSpecifiers->setChecked(s.indentDeclarationsRelativeToAccessSpecifiers);
  m_ui->indentFunctionBody->setChecked(s.indentFunctionBody);
  m_ui->indentFunctionBraces->setChecked(s.indentFunctionBraces);
  m_ui->indentSwitchLabels->setChecked(s.indentSwitchLabels);
  m_ui->indentCaseStatements->setChecked(s.indentStatementsRelativeToSwitchLabels);
  m_ui->indentCaseBlocks->setChecked(s.indentBlocksRelativeToSwitchLabels);
  m_ui->indentCaseBreak->setChecked(s.indentControlFlowRelativeToSwitchLabels);
  m_ui->bindStarToIdentifier->setChecked(s.bindStarToIdentifier);
  m_ui->bindStarToTypeName->setChecked(s.bindStarToTypeName);
  m_ui->bindStarToLeftSpecifier->setChecked(s.bindStarToLeftSpecifier);
  m_ui->bindStarToRightSpecifier->setChecked(s.bindStarToRightSpecifier);
  m_ui->extraPaddingConditions->setChecked(s.extraPaddingForConditionsIfConfusingAlign);
  m_ui->alignAssignments->setChecked(s.alignAssignments);
  m_blockUpdates = wasBlocked;
  if (preview)
    updatePreview();
}

auto CppCodeStylePreferencesWidget::slotCurrentPreferencesChanged(ICodeStylePreferences *preferences, bool preview) -> void
{
  const auto enable = !preferences->isReadOnly() && !m_preferences->currentDelegate();
  m_ui->tabSettingsWidget->setEnabled(enable);
  m_ui->contentGroupBox->setEnabled(enable);
  m_ui->bracesGroupBox->setEnabled(enable);
  m_ui->switchGroupBox->setEnabled(enable);
  m_ui->alignmentGroupBox->setEnabled(enable);
  m_ui->pointerReferencesGroupBox->setEnabled(enable);
  if (preview)
    updatePreview();
}

auto CppCodeStylePreferencesWidget::slotCodeStyleSettingsChanged() -> void
{
  if (m_blockUpdates)
    return;

  if (m_preferences) {
    auto current = qobject_cast<CppCodeStylePreferences*>(m_preferences->currentPreferences());
    if (current)
      current->setCodeStyleSettings(cppCodeStyleSettings());
  }
  emit codeStyleSettingsChanged(cppCodeStyleSettings());
  updatePreview();
}

auto CppCodeStylePreferencesWidget::slotTabSettingsChanged(const TabSettings &settings) -> void
{
  if (m_blockUpdates)
    return;

  if (m_preferences) {
    auto current = qobject_cast<CppCodeStylePreferences*>(m_preferences->currentPreferences());
    if (current)
      current->setTabSettings(settings);
  }

  emit tabSettingsChanged(settings);
  updatePreview();
}

auto CppCodeStylePreferencesWidget::updatePreview() -> void
{
  auto cppCodeStylePreferences = m_preferences ? m_preferences : CppToolsSettings::instance()->cppCodeStyle();
  const auto ccss = cppCodeStylePreferences->currentCodeStyleSettings();
  const auto ts = cppCodeStylePreferences->currentTabSettings();
  QtStyleCodeFormatter formatter(ts, ccss);
  foreach(SnippetEditorWidget *preview, m_previews) {
    preview->textDocument()->setTabSettings(ts);
    preview->setCodeStyle(cppCodeStylePreferences);

    QTextDocument *doc = preview->document();
    formatter.invalidateCache(doc);

    QTextBlock block = doc->firstBlock();
    QTextCursor tc = preview->textCursor();
    tc.beginEditBlock();
    while (block.isValid()) {
      preview->textDocument()->indenter()->indentBlock(block, QChar::Null, ts);

      block = block.next();
    }
    applyRefactorings(doc, preview, ccss);
    tc.endEditBlock();
  }
}

auto CppCodeStylePreferencesWidget::decorateEditors(const FontSettings &fontSettings) -> void
{
  foreach(SnippetEditorWidget *editor, m_previews) {
    editor->textDocument()->setFontSettings(fontSettings);
    SnippetProvider::decorateEditor(editor, CppEditor::Constants::CPP_SNIPPETS_GROUP_ID);
  }
}

auto CppCodeStylePreferencesWidget::setVisualizeWhitespace(bool on) -> void
{
  foreach(SnippetEditorWidget *editor, m_previews) {
    DisplaySettings displaySettings = editor->displaySettings();
    displaySettings.m_visualizeWhitespace = on;
    editor->setDisplaySettings(displaySettings);
  }
}

auto CppCodeStylePreferencesWidget::addTab(CppCodeStyleWidget *page, QString tabName) -> void
{
  QTC_ASSERT(page, return);
  m_ui->categoryTab->addTab(page, tabName);

  connect(page, &CppEditor::CppCodeStyleWidget::codeStyleSettingsChanged, this, [this](const CppEditor::CppCodeStyleSettings &settings) {
    setCodeStyleSettings(settings, true);
  });

  connect(page, &CppEditor::CppCodeStyleWidget::tabSettingsChanged, this, &CppCodeStylePreferencesWidget::setTabSettings);

  connect(this, &CppCodeStylePreferencesWidget::codeStyleSettingsChanged, page, &CppCodeStyleWidget::setCodeStyleSettings);

  connect(this, &CppCodeStylePreferencesWidget::tabSettingsChanged, page, &CppCodeStyleWidget::setTabSettings);

  page->synchronize();
}

// ------------------ CppCodeStyleSettingsPage

CppCodeStyleSettingsPage::CppCodeStyleSettingsPage()
{
  setId(Constants::CPP_CODE_STYLE_SETTINGS_ID);
  setDisplayName(QCoreApplication::translate("CppEditor", Constants::CPP_CODE_STYLE_SETTINGS_NAME));
  setCategory(Constants::CPP_SETTINGS_CATEGORY);
}

auto CppCodeStyleSettingsPage::widget() -> QWidget*
{
  if (!m_widget) {
    auto originalCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
    m_pageCppCodeStylePreferences = new CppCodeStylePreferences();
    m_pageCppCodeStylePreferences->setDelegatingPool(originalCodeStylePreferences->delegatingPool());
    m_pageCppCodeStylePreferences->setCodeStyleSettings(originalCodeStylePreferences->codeStyleSettings());
    m_pageCppCodeStylePreferences->setCurrentDelegate(originalCodeStylePreferences->currentDelegate());
    // we set id so that it won't be possible to set delegate to the original prefs
    m_pageCppCodeStylePreferences->setId(originalCodeStylePreferences->id());
    m_widget = TextEditorSettings::codeStyleFactory(CppEditor::Constants::CPP_SETTINGS_ID)->createCodeStyleEditor(m_pageCppCodeStylePreferences);
  }
  return m_widget;
}

auto CppCodeStyleSettingsPage::apply() -> void
{
  if (m_widget) {
    QSettings *s = Core::ICore::settings();

    auto originalCppCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
    if (originalCppCodeStylePreferences->codeStyleSettings() != m_pageCppCodeStylePreferences->codeStyleSettings()) {
      originalCppCodeStylePreferences->setCodeStyleSettings(m_pageCppCodeStylePreferences->codeStyleSettings());
      originalCppCodeStylePreferences->toSettings(QLatin1String(CppEditor::Constants::CPP_SETTINGS_ID), s);
    }
    if (originalCppCodeStylePreferences->tabSettings() != m_pageCppCodeStylePreferences->tabSettings()) {
      originalCppCodeStylePreferences->setTabSettings(m_pageCppCodeStylePreferences->tabSettings());
      originalCppCodeStylePreferences->toSettings(QLatin1String(CppEditor::Constants::CPP_SETTINGS_ID), s);
    }
    if (originalCppCodeStylePreferences->currentDelegate() != m_pageCppCodeStylePreferences->currentDelegate()) {
      originalCppCodeStylePreferences->setCurrentDelegate(m_pageCppCodeStylePreferences->currentDelegate());
      originalCppCodeStylePreferences->toSettings(QLatin1String(CppEditor::Constants::CPP_SETTINGS_ID), s);
    }

    m_widget->apply();
  }
}

auto CppCodeStyleSettingsPage::finish() -> void
{
  delete m_widget;
}

} // namespace CppEditor::Internal