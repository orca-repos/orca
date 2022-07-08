// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

namespace TextEditor {

class HighlighterSettings;

class HighlighterSettingsPage final : public Core::IOptionsPage {
public:
  HighlighterSettingsPage();
  ~HighlighterSettingsPage() override;

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;
  auto highlighterSettings() const -> const HighlighterSettings&;

private:
  auto settingsFromUI() -> void;
  auto settingsToUI() -> void;
  auto settingsChanged() const -> bool;

  class HighlighterSettingsPagePrivate;
  HighlighterSettingsPagePrivate *d;
};

} // namespace TextEditor
