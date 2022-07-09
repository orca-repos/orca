// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "completionsettingspage.hpp"

#include "texteditorsettings.hpp"
#include "texteditorconstants.hpp"
#include "ui_completionsettingspage.h"

#include <core/icore.hpp>

#include <cppeditor/cpptoolssettings.hpp>

using namespace CppEditor;

namespace TextEditor {
namespace Internal {

class CompletionSettingsPageWidget final : public Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::Internal::CompletionSettingsPage)

public:
  explicit CompletionSettingsPageWidget(CompletionSettingsPage *owner);

private:
  auto apply() -> void final;
  auto caseSensitivity() const -> CaseSensitivity;
  auto completionTrigger() const -> CompletionTrigger;
  auto settingsFromUi(CompletionSettings &completion, CommentsSettings &comment) const -> void;

  CompletionSettingsPage *m_owner = nullptr;
  Ui::CompletionSettingsPage m_ui;
};

CompletionSettingsPageWidget::CompletionSettingsPageWidget(CompletionSettingsPage *owner) : m_owner(owner)
{
  m_ui.setupUi(this);

  connect(m_ui.completionTrigger, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
    const auto enableTimeoutWidgets = completionTrigger() == AutomaticCompletion;
    m_ui.automaticProposalTimeoutLabel->setEnabled(enableTimeoutWidgets);
    m_ui.automaticProposalTimeoutSpinBox->setEnabled(enableTimeoutWidgets);
  });

  auto caseSensitivityIndex = 0;
  switch (m_owner->m_completionSettings.m_caseSensitivity) {
  case CaseSensitive:
    caseSensitivityIndex = 0;
    break;
  case CaseInsensitive:
    caseSensitivityIndex = 1;
    break;
  case FirstLetterCaseSensitive:
    caseSensitivityIndex = 2;
    break;
  }

  auto completionTriggerIndex = 0;
  switch (m_owner->m_completionSettings.m_completionTrigger) {
  case ManualCompletion:
    completionTriggerIndex = 0;
    break;
  case TriggeredCompletion:
    completionTriggerIndex = 1;
    break;
  case AutomaticCompletion:
    completionTriggerIndex = 2;
    break;
  }

  m_ui.caseSensitivity->setCurrentIndex(caseSensitivityIndex);
  m_ui.completionTrigger->setCurrentIndex(completionTriggerIndex);
  m_ui.automaticProposalTimeoutSpinBox->setValue(m_owner->m_completionSettings.m_automaticProposalTimeoutInMs);
  m_ui.thresholdSpinBox->setValue(m_owner->m_completionSettings.m_characterThreshold);
  m_ui.insertBrackets->setChecked(m_owner->m_completionSettings.m_autoInsertBrackets);
  m_ui.surroundBrackets->setChecked(m_owner->m_completionSettings.m_surroundingAutoBrackets);
  m_ui.insertQuotes->setChecked(m_owner->m_completionSettings.m_autoInsertQuotes);
  m_ui.surroundQuotes->setChecked(m_owner->m_completionSettings.m_surroundingAutoQuotes);
  m_ui.partiallyComplete->setChecked(m_owner->m_completionSettings.m_partiallyComplete);
  m_ui.spaceAfterFunctionName->setChecked(m_owner->m_completionSettings.m_spaceAfterFunctionName);
  m_ui.autoSplitStrings->setChecked(m_owner->m_completionSettings.m_autoSplitStrings);
  m_ui.animateAutoComplete->setChecked(m_owner->m_completionSettings.m_animateAutoComplete);
  m_ui.overwriteClosingChars->setChecked(m_owner->m_completionSettings.m_overwriteClosingChars);
  m_ui.highlightAutoComplete->setChecked(m_owner->m_completionSettings.m_highlightAutoComplete);
  m_ui.skipAutoComplete->setChecked(m_owner->m_completionSettings.m_skipAutoCompletedText);
  m_ui.removeAutoComplete->setChecked(m_owner->m_completionSettings.m_autoRemove);

  m_ui.enableDoxygenCheckBox->setChecked(m_owner->m_commentsSettings.m_enableDoxygen);
  m_ui.generateBriefCheckBox->setChecked(m_owner->m_commentsSettings.m_generateBrief);
  m_ui.leadingAsterisksCheckBox->setChecked(m_owner->m_commentsSettings.m_leadingAsterisks);

  m_ui.generateBriefCheckBox->setEnabled(m_ui.enableDoxygenCheckBox->isChecked());
  m_ui.skipAutoComplete->setEnabled(m_ui.highlightAutoComplete->isChecked());
  m_ui.removeAutoComplete->setEnabled(m_ui.highlightAutoComplete->isChecked());
}

auto CompletionSettingsPageWidget::apply() -> void
{
  CompletionSettings completionSettings;
  CommentsSettings commentsSettings;

  settingsFromUi(completionSettings, commentsSettings);

  if (m_owner->m_completionSettings != completionSettings) {
    m_owner->m_completionSettings = completionSettings;
    m_owner->m_completionSettings.toSettings(Core::ICore::settings());
    emit TextEditorSettings::instance()->completionSettingsChanged(completionSettings);
  }

  if (m_owner->m_commentsSettings != commentsSettings) {
    m_owner->m_commentsSettings = commentsSettings;
    m_owner->m_commentsSettings.toSettings(Core::ICore::settings());
    emit TextEditorSettings::instance()->commentsSettingsChanged(commentsSettings);
  }
}

auto CompletionSettingsPageWidget::caseSensitivity() const -> CaseSensitivity
{
  switch (m_ui.caseSensitivity->currentIndex()) {
  case 0: // Full
    return CaseSensitive;
  case 1: // None
    return CaseInsensitive;
  default: // First letter
    return FirstLetterCaseSensitive;
  }
}

auto CompletionSettingsPageWidget::completionTrigger() const -> CompletionTrigger
{
  switch (m_ui.completionTrigger->currentIndex()) {
  case 0:
    return ManualCompletion;
  case 1:
    return TriggeredCompletion;
  default:
    return AutomaticCompletion;
  }
}

auto CompletionSettingsPageWidget::settingsFromUi(CompletionSettings &completion, CommentsSettings &comment) const -> void
{
  completion.m_caseSensitivity = caseSensitivity();
  completion.m_completionTrigger = completionTrigger();
  completion.m_automaticProposalTimeoutInMs = m_ui.automaticProposalTimeoutSpinBox->value();
  completion.m_characterThreshold = m_ui.thresholdSpinBox->value();
  completion.m_autoInsertBrackets = m_ui.insertBrackets->isChecked();
  completion.m_surroundingAutoBrackets = m_ui.surroundBrackets->isChecked();
  completion.m_autoInsertQuotes = m_ui.insertQuotes->isChecked();
  completion.m_surroundingAutoQuotes = m_ui.surroundQuotes->isChecked();
  completion.m_partiallyComplete = m_ui.partiallyComplete->isChecked();
  completion.m_spaceAfterFunctionName = m_ui.spaceAfterFunctionName->isChecked();
  completion.m_autoSplitStrings = m_ui.autoSplitStrings->isChecked();
  completion.m_animateAutoComplete = m_ui.animateAutoComplete->isChecked();
  completion.m_overwriteClosingChars = m_ui.overwriteClosingChars->isChecked();
  completion.m_highlightAutoComplete = m_ui.highlightAutoComplete->isChecked();
  completion.m_skipAutoCompletedText = m_ui.skipAutoComplete->isChecked();
  completion.m_autoRemove = m_ui.removeAutoComplete->isChecked();

  comment.m_enableDoxygen = m_ui.enableDoxygenCheckBox->isChecked();
  comment.m_generateBrief = m_ui.generateBriefCheckBox->isChecked();
  comment.m_leadingAsterisks = m_ui.leadingAsterisksCheckBox->isChecked();
}

auto CompletionSettingsPage::completionSettings() -> const CompletionSettings&
{
  return m_completionSettings;
}

auto CompletionSettingsPage::commentsSettings() -> const CommentsSettings&
{
  return m_commentsSettings;
}

CompletionSettingsPage::CompletionSettingsPage()
{
  setId("P.Completion");
  setDisplayName(CompletionSettingsPageWidget::tr("Completion"));
  setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
  setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
  setWidgetCreator([this] { return new CompletionSettingsPageWidget(this); });

  QSettings *s = Core::ICore::settings();
  m_completionSettings.fromSettings(s);
  m_commentsSettings.fromSettings(s);
}

} // Internal
} // TextEditor
