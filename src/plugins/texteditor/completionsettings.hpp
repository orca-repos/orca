// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

enum CaseSensitivity {
  CaseInsensitive,
  CaseSensitive,
  FirstLetterCaseSensitive
};

enum CompletionTrigger {
  ManualCompletion,
  // Display proposal only when explicitly invoked by the user.
  TriggeredCompletion,
  // When triggered by the user or upon contextual activation characters.
  AutomaticCompletion // The above plus an automatic trigger when the editor is "idle".
};

/**
 * Settings that describe how the code completion behaves.
 */
class TEXTEDITOR_EXPORT CompletionSettings {
public:
  auto toSettings(QSettings *s) const -> void;
  auto fromSettings(QSettings *s) -> void;
  auto equals(const CompletionSettings &bs) const -> bool;

  friend auto operator==(const CompletionSettings &t1, const CompletionSettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const CompletionSettings &t1, const CompletionSettings &t2) -> bool { return !t1.equals(t2); }

  CaseSensitivity m_caseSensitivity = CaseInsensitive;
  CompletionTrigger m_completionTrigger = AutomaticCompletion;

  int m_automaticProposalTimeoutInMs = 400;
  int m_characterThreshold = 3;
  bool m_autoInsertBrackets = true;
  bool m_surroundingAutoBrackets = true;
  bool m_autoInsertQuotes = true;
  bool m_surroundingAutoQuotes = true;
  bool m_partiallyComplete = true;
  bool m_spaceAfterFunctionName = false;
  bool m_autoSplitStrings = true;
  bool m_animateAutoComplete = true;
  bool m_highlightAutoComplete = true;
  bool m_skipAutoCompletedText = true;
  bool m_autoRemove = true;
  bool m_overwriteClosingChars = false;
};

} // namespace TextEditor
