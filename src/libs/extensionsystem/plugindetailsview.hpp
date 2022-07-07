// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.hpp"

#include <QWidget>

namespace ExtensionSystem {

class PluginSpec;

namespace Internal {
namespace Ui { class PluginDetailsView; }
} // namespace Internal

class EXTENSIONSYSTEM_EXPORT PluginDetailsView : public QWidget {
  Q_OBJECT

public:
  PluginDetailsView(QWidget *parent = nullptr);
  ~PluginDetailsView() override;

  auto update(PluginSpec *spec) -> void;

private:
  Internal::Ui::PluginDetailsView *m_ui;
};

} // namespace ExtensionSystem
