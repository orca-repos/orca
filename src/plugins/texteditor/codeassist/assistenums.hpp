// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace TextEditor {

enum AssistKind {
  Completion,
  QuickFix,
  FollowSymbol,
  FunctionHint
};

enum AssistReason {
  IdleEditor,
  ActivationCharacter,
  ExplicitlyInvoked
};

} // TextEditor
