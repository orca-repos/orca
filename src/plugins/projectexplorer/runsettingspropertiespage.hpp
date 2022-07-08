// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QGridLayout;
class QLabel;
class QMenu;
class QPushButton;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Utils {
class InfoLabel;
}

namespace ProjectExplorer {

class DeployConfiguration;
class RunConfiguration;
class Target;

namespace Internal {

class BuildStepListWidget;

class RunSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit RunSettingsWidget(Target *target);

private:
  auto currentRunConfigurationChanged(int index) -> void;
  auto showAddRunConfigDialog() -> void;
  auto cloneRunConfiguration() -> void;
  auto removeRunConfiguration() -> void;
  auto activeRunConfigurationChanged() -> void;
  auto renameRunConfiguration() -> void;
  auto currentDeployConfigurationChanged(int index) -> void;
  auto aboutToShowDeployMenu() -> void;
  auto removeDeployConfiguration() -> void;
  auto activeDeployConfigurationChanged() -> void;
  auto renameDeployConfiguration() -> void;
  auto updateRemoveToolButton() -> void;
  auto uniqueDCName(const QString &name) -> QString;
  auto uniqueRCName(const QString &name) -> QString;
  auto updateDeployConfiguration(DeployConfiguration *) -> void;
  auto setConfigurationWidget(RunConfiguration *rc) -> void;
  auto addRunControlWidgets() -> void;
  auto addSubWidget(QWidget *subWidget, QLabel *label) -> void;
  auto removeSubWidgets() -> void;
  auto updateEnabledState() -> void;

  Target *m_target;
  QWidget *m_runConfigurationWidget = nullptr;
  RunConfiguration *m_runConfiguration = nullptr;
  QVBoxLayout *m_runLayout = nullptr;
  QWidget *m_deployConfigurationWidget = nullptr;
  QVBoxLayout *m_deployLayout = nullptr;
  BuildStepListWidget *m_deploySteps = nullptr;
  QMenu *m_addDeployMenu;
  bool m_ignoreChange = false;
  using RunConfigItem = QPair<QWidget*, QLabel*>;
  QList<RunConfigItem> m_subWidgets;
  QGridLayout *m_gridLayout;
  QComboBox *m_deployConfigurationCombo;
  QWidget *m_deployWidget;
  QComboBox *m_runConfigurationCombo;
  QPushButton *m_addDeployToolButton;
  QPushButton *m_removeDeployToolButton;
  QPushButton *m_addRunToolButton;
  QPushButton *m_removeRunToolButton;
  QPushButton *m_renameRunButton;
  QPushButton *m_cloneRunButton;
  QPushButton *m_renameDeployButton;
  Utils::InfoLabel *m_disabledText;
};

} // namespace Internal
} // namespace ProjectExplorer
