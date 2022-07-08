// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.hpp"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QListWidgetItem;
QT_END_NAMESPACE

namespace ExtensionSystem {

namespace Internal {
namespace Ui {
class PluginErrorOverview;
}
} // namespace Internal

class EXTENSIONSYSTEM_EXPORT PluginErrorOverview : public QDialog {
  Q_OBJECT

public:
  explicit PluginErrorOverview(QWidget *parent = nullptr);
  ~PluginErrorOverview() override;

private:
  auto showDetails(QListWidgetItem *item) -> void;

  Internal::Ui::PluginErrorOverview *m_ui;
};

} // ExtensionSystem