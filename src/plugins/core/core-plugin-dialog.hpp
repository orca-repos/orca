// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace ExtensionSystem {
class PluginSpec;
class PluginView;
}

namespace Orca::Plugin::Core {

class PluginDialog final : public QDialog {
  Q_OBJECT

public:
  explicit PluginDialog(QWidget *parent);

private:
  auto updateRestartRequired() const -> void;
  auto updateButtons() const -> void;
  auto openDetails(ExtensionSystem::PluginSpec *spec) -> void;
  auto openErrorDetails() -> void;
  auto closeDialog() -> void;
  auto showInstallWizard() const -> void;

  ExtensionSystem::PluginView *m_view;
  QPushButton *m_details_button;
  QPushButton *m_error_details_button;
  QPushButton *m_install_button;
  QPushButton *m_close_button;
  QLabel *m_restart_required;
};

} // namespace Orca::Plugin::Core
