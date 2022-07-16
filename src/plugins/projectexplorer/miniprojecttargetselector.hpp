// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QDateTime>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Kit;
class Project;
class Target;
class BuildConfiguration;
class DeployConfiguration;
class RunConfiguration;

namespace Internal {

class GenericListWidget;
class ProjectListView;
class KitAreaWidget;

class MiniProjectTargetSelector : public QWidget {
  Q_OBJECT

public:
  explicit MiniProjectTargetSelector(QAction *projectAction, QWidget *parent);

  auto setVisible(bool visible) -> void override;
  auto keyPressEvent(QKeyEvent *ke) -> void override;
  auto keyReleaseEvent(QKeyEvent *ke) -> void override;
  auto event(QEvent *event) -> bool override;
  auto toggleVisible() -> void;
  auto nextOrShow() -> void;

private:
  friend class Target;

  auto projectAdded(Project *project) -> void;
  auto projectRemoved(Project *project) -> void;
  auto handleNewTarget(Target *target) -> void;
  auto handleRemovalOfTarget(Target *pc) -> void;
  auto changeStartupProject(Project *project) -> void;
  auto activeTargetChanged(Target *target) -> void;
  auto kitChanged(Kit *k) -> void;
  auto activeBuildConfigurationChanged(BuildConfiguration *bc) -> void;
  auto activeDeployConfigurationChanged(DeployConfiguration *dc) -> void;
  auto activeRunConfigurationChanged(RunConfiguration *rc) -> void;
  auto delayedHide() -> void;
  auto updateActionAndSummary() -> void;
  auto switchToProjectsMode() -> void;
  auto addedTarget(Target *target) -> void;
  auto removedTarget(Target *target) -> void;
  auto addedBuildConfiguration(BuildConfiguration *bc, bool update = true) -> void;
  auto removedBuildConfiguration(BuildConfiguration *bc, bool update = true) -> void;
  auto addedDeployConfiguration(DeployConfiguration *dc, bool update = true) -> void;
  auto removedDeployConfiguration(DeployConfiguration *dc, bool update = true) -> void;
  auto addedRunConfiguration(RunConfiguration *rc, bool update = true) -> void;
  auto removedRunConfiguration(RunConfiguration *rc, bool update = true) -> void;
  auto updateProjectListVisible() -> void;
  auto updateTargetListVisible() -> void;
  auto updateBuildListVisible() -> void;
  auto updateDeployListVisible() -> void;
  auto updateRunListVisible() -> void;
  auto updateSummary() -> void;
  auto paintEvent(QPaintEvent *) -> void override;
  auto mousePressEvent(QMouseEvent *) -> void override;
  auto doLayout(bool keepSize) -> void;
  auto listWidgetWidths(int minSize, int maxSize) -> QVector<int>;
  auto createTitleLabel(const QString &text) -> QWidget*;

  QAction *m_projectAction;

  enum TYPES {
    PROJECT = 0,
    TARGET = 1,
    BUILD = 2,
    DEPLOY = 3,
    RUN = 4,
    LAST = 5
  };

  ProjectListView *m_projectListWidget;
  KitAreaWidget *m_kitAreaWidget;
  QVector<GenericListWidget*> m_listWidgets;
  QVector<QWidget*> m_titleWidgets;
  QLabel *m_summaryLabel;
  Project *m_project = nullptr;
  Target *m_target = nullptr;
  BuildConfiguration *m_buildConfiguration = nullptr;
  DeployConfiguration *m_deployConfiguration = nullptr;
  RunConfiguration *m_runConfiguration = nullptr;
  bool m_hideOnRelease = false;
  QDateTime m_earliestHidetime;
};

} // namespace Internal
} // namespace ProjectExplorer
