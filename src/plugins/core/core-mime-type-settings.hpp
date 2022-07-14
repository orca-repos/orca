// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-options-page-interface.hpp"

namespace Orca::Plugin::Core {

class MimeTypeSettingsPrivate;

class MimeTypeSettings final : public IOptionsPage {
  Q_OBJECT

public:
  MimeTypeSettings();
  ~MimeTypeSettings() override;

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;
  static auto restoreSettings() -> void;

private:
  MimeTypeSettingsPrivate *d;
};

} // namespace Orca::Plugin::Core
