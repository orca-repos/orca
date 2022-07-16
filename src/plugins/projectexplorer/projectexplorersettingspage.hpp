// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_projectexplorersettingspage.h"

#include <core/core-options-page-interface.hpp>

#include <QPointer>

namespace ProjectExplorer {
namespace Internal {

class ProjectExplorerSettings;
class ProjectExplorerSettingsWidget;

class ProjectExplorerSettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  ProjectExplorerSettingsPage();

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;

private:
  QPointer<ProjectExplorerSettingsWidget> m_widget;
};

} // namespace Internal
} // namespace ProjectExplorer
