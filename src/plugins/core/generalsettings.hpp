// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

namespace Core {
namespace Internal {

class GeneralSettings final : public IOptionsPage {
public:
  GeneralSettings();

  static auto showShortcutsInContextMenu() -> bool;
  auto setShowShortcutsInContextMenu(bool show) const -> void;

private:
  friend class GeneralSettingsWidget;
  bool m_default_show_shortcuts_in_context_menu;
};

} // namespace Internal
} // namespace Core
