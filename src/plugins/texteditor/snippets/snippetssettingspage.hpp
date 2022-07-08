// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <core/dialogs/ioptionspage.hpp>

namespace TextEditor {
namespace Internal {

class SnippetsSettingsPagePrivate;

class SnippetsSettingsPage final : public Core::IOptionsPage {
public:
  SnippetsSettingsPage();
  ~SnippetsSettingsPage() override;

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;

private:
  SnippetsSettingsPagePrivate *d;
};

} // Internal
} // TextEditor
