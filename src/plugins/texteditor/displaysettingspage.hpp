// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

namespace TextEditor {

class DisplaySettings;
class MarginSettings;
class DisplaySettingsPagePrivate;

class DisplaySettingsPage : public Core::IOptionsPage {
public:
  DisplaySettingsPage();
  ~DisplaySettingsPage() override;

  auto displaySettings() const -> const DisplaySettings&;
  auto marginSettings() const -> const MarginSettings&;

private:
  DisplaySettingsPagePrivate *d;
};

} // namespace TextEditor
