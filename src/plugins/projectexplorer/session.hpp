// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/id.hpp>
#include <utils/persistentsettings.hpp>

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Core {
class IEditor;
}

namespace ProjectExplorer {

class Project;
class Target;
class BuildConfiguration;
class BuildSystem;
class DeployConfiguration;
class RunConfiguration;

enum class SetActive {
  Cascade,
  NoCascade
};

class PROJECTEXPLORER_EXPORT SessionManager : public QObject {
  Q_OBJECT

public:
  explicit SessionManager(QObject *parent = nullptr);
  ~SessionManager() override;

  static auto instance() -> SessionManager*;

  // higher level session management
  static auto activeSession() -> QString;
  static auto lastSession() -> QString;
  static auto startupSession() -> QString;
  static auto sessions() -> QStringList;
  static auto sessionDateTime(const QString &session) -> QDateTime;
  static auto createSession(const QString &session) -> bool;
  static auto confirmSessionDelete(const QStringList &sessions) -> bool;
  static auto deleteSession(const QString &session) -> bool;
  static auto deleteSessions(const QStringList &sessions) -> void;
  static auto cloneSession(const QString &original, const QString &clone) -> bool;
  static auto renameSession(const QString &original, const QString &newName) -> bool;
  static auto loadSession(const QString &session, bool initial = false) -> bool;
  static auto save() -> bool;
  static auto closeAllProjects() -> void;
  static auto addProject(Project *project) -> void;
  static auto removeProject(Project *project) -> void;
  static auto removeProjects(const QList<Project*> &remove) -> void;
  static auto setStartupProject(Project *startupProject) -> void;
  static auto dependencies(const Project *project) -> QList<Project*>;
  static auto hasDependency(const Project *project, const Project *depProject) -> bool;
  static auto canAddDependency(const Project *project, const Project *depProject) -> bool;
  static auto addDependency(Project *project, Project *depProject) -> bool;
  static auto removeDependency(Project *project, Project *depProject) -> void;
  static auto isProjectConfigurationCascading() -> bool;
  static auto setProjectConfigurationCascading(bool b) -> void;
  static auto setActiveTarget(Project *p, Target *t, SetActive cascade) -> void;
  static auto setActiveBuildConfiguration(Target *t, BuildConfiguration *bc, SetActive cascade) -> void;
  static auto setActiveDeployConfiguration(Target *t, DeployConfiguration *dc, SetActive cascade) -> void;
  static auto sessionNameToFileName(const QString &session) -> Utils::FilePath;
  static auto startupProject() -> Project*;
  static auto startupTarget() -> Target*;
  static auto startupBuildSystem() -> BuildSystem*;
  static auto startupRunConfiguration() -> RunConfiguration*;
  static auto projects() -> const QList<Project*>;
  static auto hasProjects() -> bool;
  static auto hasProject(Project *p) -> bool;
  static auto isDefaultVirgin() -> bool;
  static auto isDefaultSession(const QString &session) -> bool;

  // Let other plugins store persistent values within the session file
  static auto setValue(const QString &name, const QVariant &value) -> void;
  static auto value(const QString &name) -> QVariant;

  // NBS rewrite projectOrder (dependency management)
  static auto projectOrder(const Project *project = nullptr) -> QList<Project*>;
  static auto projectForFile(const Utils::FilePath &fileName) -> Project*;
  static auto projectWithProjectFilePath(const Utils::FilePath &filePath) -> Project*;
  static auto projectsForSessionName(const QString &session) -> QStringList;
  static auto reportProjectLoadingProgress() -> void;
  static auto loadingSession() -> bool;

signals:
  auto targetAdded(Target *target) -> void;
  auto targetRemoved(Target *target) -> void;
  auto projectAdded(Project *project) -> void;
  auto aboutToRemoveProject(Project *project) -> void;
  auto projectDisplayNameChanged(Project *project) -> void;
  auto projectRemoved(Project *project) -> void;
  auto startupProjectChanged(Project *project) -> void;
  auto aboutToUnloadSession(QString sessionName) -> void;
  auto aboutToLoadSession(QString sessionName) -> void;
  auto sessionLoaded(QString sessionName) -> void;
  auto aboutToSaveSession() -> void;
  auto dependencyChanged(Project *a, Project *b) -> void;
  auto sessionRenamed(const QString &oldName, const QString &newName) -> void;
  auto sessionRemoved(const QString &name) -> void;

  // for tests only
  auto projectFinishedParsing(Project *project) -> void;

private:
  static auto saveActiveMode(Utils::Id mode) -> void;
  static auto configureEditor(Core::IEditor *editor, const QString &fileName) -> void;
  static auto markSessionFileDirty() -> void;
  static auto configureEditors(Project *project) -> void;
};

} // namespace ProjectExplorer
