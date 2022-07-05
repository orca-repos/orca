// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include <QWidget>

namespace ExtensionSystem {

class PluginSpec;
namespace Internal {
namespace Ui {
class PluginErrorView;
}
} // namespace Internal

class EXTENSIONSYSTEM_EXPORT PluginErrorView : public QWidget {
  Q_OBJECT

public:
  PluginErrorView(QWidget *parent = nullptr);
  ~PluginErrorView() override;

  auto update(PluginSpec *spec) -> void;

private:
  Internal::Ui::PluginErrorView *m_ui;
};

} // namespace ExtensionSystem
