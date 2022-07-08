// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "completionsettings.hpp"

#include <QSettings>

static constexpr char settingsGroup[]               = "CppTools/Completion";
static constexpr char caseSensitivityKey[]          = "CaseSensitivity";
static constexpr char completionTriggerKey[]        = "CompletionTrigger";
static constexpr char automaticProposalTimeoutKey[] = "AutomaticProposalTimeout";
static constexpr char characterThresholdKey[]       = "CharacterThreshold";
static constexpr char autoInsertBracesKey[]         = "AutoInsertBraces";
static constexpr char surroundingAutoBracketsKey[]  = "SurroundingAutoBrackets";
static constexpr char autoInsertQuotesKey[]         = "AutoInsertQuotes";
static constexpr char surroundingAutoQuotesKey[]    = "SurroundingAutoQuotes";
static constexpr char partiallyCompleteKey[]        = "PartiallyComplete";
static constexpr char spaceAfterFunctionNameKey[]   = "SpaceAfterFunctionName";
static constexpr char autoSplitStringsKey[]         = "AutoSplitStrings";
static constexpr char animateAutoCompleteKey[]      = "AnimateAutoComplete";
static constexpr char highlightAutoCompleteKey[]    = "HighlightAutoComplete";
static constexpr char skipAutoCompleteKey[]         = "SkipAutoComplete";
static constexpr char autoRemoveKey[]               = "AutoRemove";
static constexpr char overwriteClosingCharsKey[]    = "OverwriteClosingChars";

using namespace TextEditor;

auto CompletionSettings::toSettings(QSettings *s) const -> void
{
  s->beginGroup(settingsGroup);
  s->setValue(caseSensitivityKey, (int)m_caseSensitivity);
  s->setValue(completionTriggerKey, (int)m_completionTrigger);
  s->setValue(automaticProposalTimeoutKey, m_automaticProposalTimeoutInMs);
  s->setValue(characterThresholdKey, m_characterThreshold);
  s->setValue(autoInsertBracesKey, m_autoInsertBrackets);
  s->setValue(surroundingAutoBracketsKey, m_surroundingAutoBrackets);
  s->setValue(autoInsertQuotesKey, m_autoInsertQuotes);
  s->setValue(surroundingAutoQuotesKey, m_surroundingAutoQuotes);
  s->setValue(partiallyCompleteKey, m_partiallyComplete);
  s->setValue(spaceAfterFunctionNameKey, m_spaceAfterFunctionName);
  s->setValue(autoSplitStringsKey, m_autoSplitStrings);
  s->setValue(animateAutoCompleteKey, m_animateAutoComplete);
  s->setValue(highlightAutoCompleteKey, m_highlightAutoComplete);
  s->setValue(skipAutoCompleteKey, m_skipAutoCompletedText);
  s->setValue(autoRemoveKey, m_autoRemove);
  s->setValue(overwriteClosingCharsKey, m_overwriteClosingChars);
  s->endGroup();
}

auto CompletionSettings::fromSettings(QSettings *s) -> void
{
  *this = CompletionSettings(); // Assign defaults

  s->beginGroup(settingsGroup);
  m_caseSensitivity = (CaseSensitivity)s->value(caseSensitivityKey, m_caseSensitivity).toInt();
  m_completionTrigger = (CompletionTrigger)s->value(completionTriggerKey, m_completionTrigger).toInt();
  m_automaticProposalTimeoutInMs = s->value(automaticProposalTimeoutKey, m_automaticProposalTimeoutInMs).toInt();
  m_characterThreshold = s->value(characterThresholdKey, m_characterThreshold).toInt();
  m_autoInsertBrackets = s->value(autoInsertBracesKey, m_autoInsertBrackets).toBool();
  m_surroundingAutoBrackets = s->value(surroundingAutoBracketsKey, m_surroundingAutoBrackets).toBool();
  m_autoInsertQuotes = s->value(autoInsertQuotesKey, m_autoInsertQuotes).toBool();
  m_surroundingAutoQuotes = s->value(surroundingAutoQuotesKey, m_surroundingAutoQuotes).toBool();
  m_partiallyComplete = s->value(partiallyCompleteKey, m_partiallyComplete).toBool();
  m_spaceAfterFunctionName = s->value(spaceAfterFunctionNameKey, m_spaceAfterFunctionName).toBool();
  m_autoSplitStrings = s->value(autoSplitStringsKey, m_autoSplitStrings).toBool();
  m_animateAutoComplete = s->value(animateAutoCompleteKey, m_animateAutoComplete).toBool();
  m_highlightAutoComplete = s->value(highlightAutoCompleteKey, m_highlightAutoComplete).toBool();
  m_skipAutoCompletedText = s->value(skipAutoCompleteKey, m_skipAutoCompletedText).toBool();
  m_autoRemove = s->value(autoRemoveKey, m_autoRemove).toBool();
  m_overwriteClosingChars = s->value(overwriteClosingCharsKey, m_overwriteClosingChars).toBool();
  s->endGroup();
}

auto CompletionSettings::equals(const CompletionSettings &cs) const -> bool
{
  return m_caseSensitivity == cs.m_caseSensitivity && m_completionTrigger == cs.m_completionTrigger && m_automaticProposalTimeoutInMs == cs.m_automaticProposalTimeoutInMs && m_characterThreshold == cs.m_characterThreshold && m_autoInsertBrackets == cs.m_autoInsertBrackets && m_surroundingAutoBrackets == cs.m_surroundingAutoBrackets && m_autoInsertQuotes == cs.m_autoInsertQuotes && m_surroundingAutoQuotes == cs.m_surroundingAutoQuotes && m_partiallyComplete == cs.m_partiallyComplete && m_spaceAfterFunctionName == cs.m_spaceAfterFunctionName && m_autoSplitStrings == cs.m_autoSplitStrings && m_animateAutoComplete == cs.m_animateAutoComplete && m_highlightAutoComplete == cs.m_highlightAutoComplete && m_skipAutoCompletedText == cs.m_skipAutoCompletedText && m_autoRemove == cs.m_autoRemove && m_overwriteClosingChars == cs.m_overwriteClosingChars;
}
