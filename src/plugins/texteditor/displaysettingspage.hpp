// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-options-page-interface.hpp>

namespace TextEditor {

class DisplaySettings;
class MarginSettings;
class DisplaySettingsPagePrivate;

class DisplaySettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  DisplaySettingsPage();
  ~DisplaySettingsPage() override;

  auto displaySettings() const -> const DisplaySettings&;
  auto marginSettings() const -> const MarginSettings&;

private:
  DisplaySettingsPagePrivate *d;
};

} // namespace TextEditor
