// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildstep.hpp"
#include "projectexplorer_export.hpp"

#include <QObject>
#include <QStringList>

namespace ProjectExplorer {

class RunConfiguration;

namespace Internal {
class CompileOutputSettings;
}

class Task;
class Project;

enum class BuildForRunConfigStatus {
  Building,
  NotBuilding,
  BuildFailed
};

enum class ConfigSelection {
  All,
  Active
};

class PROJECTEXPLORER_EXPORT BuildManager : public QObject {
  Q_OBJECT

public:
  explicit BuildManager(QObject *parent, QAction *cancelBuildAction);
  ~BuildManager() override;

  static auto instance() -> BuildManager*;
  static auto extensionsInitialized() -> void;
  static auto buildProjectWithoutDependencies(Project *project) -> void;
  static auto cleanProjectWithoutDependencies(Project *project) -> void;
  static auto rebuildProjectWithoutDependencies(Project *project) -> void;
  static auto buildProjectWithDependencies(Project *project, ConfigSelection configSelection = ConfigSelection::Active) -> void;
  static auto cleanProjectWithDependencies(Project *project, ConfigSelection configSelection) -> void;
  static auto rebuildProjectWithDependencies(Project *project, ConfigSelection configSelection) -> void;
  static auto buildProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void;
  static auto cleanProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void;
  static auto rebuildProjects(const QList<Project*> &projects, ConfigSelection configSelection) -> void;
  static auto deployProjects(const QList<Project*> &projects) -> void;
  static auto potentiallyBuildForRunConfig(RunConfiguration *rc) -> BuildForRunConfigStatus;
  static auto isBuilding() -> bool;
  static auto isDeploying() -> bool;
  static auto tasksAvailable() -> bool;
  static auto buildLists(QList<BuildStepList*> bsls, const QStringList &preambelMessage = QStringList()) -> bool;
  static auto buildList(BuildStepList *bsl) -> bool;
  static auto isBuilding(const Project *p) -> bool;
  static auto isBuilding(const Target *t) -> bool;
  static auto isBuilding(const ProjectConfiguration *p) -> bool;
  static auto isBuilding(BuildStep *step) -> bool;

  // Append any build step to the list of build steps (currently only used to add the QMakeStep)
  static auto appendStep(BuildStep *step, const QString &name) -> void;
  static auto getErrorTaskCount() -> int;
  static auto setCompileOutputSettings(const Internal::CompileOutputSettings &settings) -> void;
  static auto compileOutputSettings() -> const Internal::CompileOutputSettings&;
  static auto displayNameForStepId(Utils::Id stepId) -> QString;

public slots:
  static auto cancel() -> void;
  // Shows without focus
  static auto showTaskWindow() -> void;
  static auto toggleTaskWindow() -> void;
  static auto toggleOutputWindow() -> void;
  static auto aboutToRemoveProject(Project *p) -> void;

signals:
  auto buildStateChanged(Project *pro) -> void;
  auto buildQueueFinished(bool success) -> void;

private:
  static auto addToTaskWindow(const Task &task, int linkedOutputLines, int skipLines) -> void;
  static auto addToOutputWindow(const QString &string, BuildStep::OutputFormat format, BuildStep::OutputNewlineSetting newlineSettings = BuildStep::DoAppendNewline) -> void;
  static auto nextBuildQueue() -> void;
  static auto progressChanged(int percent, const QString &text) -> void;
  static auto emitCancelMessage() -> void;
  static auto showBuildResults() -> void;
  static auto updateTaskCount() -> void;
  static auto finish() -> void;
  static auto startBuildQueue() -> void;
  static auto nextStep() -> void;
  static auto clearBuildQueue() -> void;
  static auto buildQueueAppend(const QList<BuildStep*> &steps, QStringList names, const QStringList &preambleMessage = QStringList()) -> bool;
  static auto incrementActiveBuildSteps(BuildStep *bs) -> void;
  static auto decrementActiveBuildSteps(BuildStep *bs) -> void;
  static auto disconnectOutput(BuildStep *bs) -> void;
};

} // namespace ProjectExplorer
