// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "commentssettings.hpp"
#include "completionsettings.hpp"

#include <core/core-options-page-interface.hpp>

namespace TextEditor {
namespace Internal {

class CompletionSettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  CompletionSettingsPage();

  auto completionSettings() -> const CompletionSettings&;
  auto commentsSettings() -> const CommentsSettings&;

private:
  friend class CompletionSettingsPageWidget;

  CommentsSettings m_commentsSettings;
  CompletionSettings m_completionSettings;
};

} // namespace Internal
} // namespace TextEditor
