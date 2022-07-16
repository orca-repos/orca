// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/abstractprocessstep.hpp>
#include <utils/treemodel.hpp>

namespace Utils {
class CommandLine;
class StringAspect;
} // Utils

namespace CMakeProjectManager {
namespace Internal {

class CMakeBuildStep;

class CMakeTargetItem : public Utils::TreeItem {
public:
  CMakeTargetItem() = default;
  CMakeTargetItem(const QString &target, CMakeBuildStep *step, bool special);

private:
  auto data(int column, int role) const -> QVariant final;
  auto setData(int column, const QVariant &data, int role) -> bool final;
  auto flags(int column) const -> Qt::ItemFlags final;

  QString m_target;
  CMakeBuildStep *m_step = nullptr;
  bool m_special = false;
};

class CMakeBuildStep : public ProjectExplorer::AbstractProcessStep {
  Q_OBJECT

public:
  CMakeBuildStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

  auto buildTargets() const -> QStringList;
  auto setBuildTargets(const QStringList &target) -> void;
  auto buildsBuildTarget(const QString &target) const -> bool;
  auto setBuildsBuildTarget(const QString &target, bool on) -> void;
  auto toMap() const -> QVariantMap override;
  auto cleanTarget() const -> QString;
  auto allTarget() const -> QString;
  auto installTarget() const -> QString;
  static auto specialTargets(bool allCapsTargets) -> QStringList;
  auto activeRunConfigTarget() const -> QString;

signals:
  auto buildTargetsChanged() -> void;

private:
  auto cmakeCommand() const -> Utils::CommandLine;
  auto processFinished(int exitCode, QProcess::ExitStatus status) -> void override;
  auto fromMap(const QVariantMap &map) -> bool override;
  auto init() -> bool override;
  auto setupOutputFormatter(Utils::OutputFormatter *formatter) -> void override;
  auto doRun() -> void override;
  auto createConfigWidget() -> QWidget* override;
  auto defaultBuildTarget() const -> QString;
  auto runImpl() -> void;
  auto handleProjectWasParsed(bool success) -> void;
  auto handleBuildTargetsChanges(bool success) -> void;
  auto recreateBuildTargetsModel() -> void;
  auto updateBuildTargetsModel() -> void;

  QMetaObject::Connection m_runTrigger;
  friend class CMakeBuildStepConfigWidget;
  QStringList m_buildTargets; // Convention: Empty string member signifies "Current executable"
  Utils::StringAspect *m_cmakeArguments = nullptr;
  Utils::StringAspect *m_toolArguments = nullptr;
  bool m_waiting = false;
  QString m_allTarget = "all";
  QString m_installTarget = "install";
  Utils::TreeModel<Utils::TreeItem, CMakeTargetItem> m_buildTargetModel;
};

class CMakeBuildStepFactory : public ProjectExplorer::BuildStepFactory {
public:
  CMakeBuildStepFactory();
};

} // namespace Internal
} // namespace CMakeProjectManager
