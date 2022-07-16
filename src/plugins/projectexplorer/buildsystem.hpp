// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "buildtargetinfo.hpp"
#include "project.hpp"
#include "treescanner.hpp"

#include <QObject>

namespace Utils {
class CommandLine;
}

namespace ProjectExplorer {

class BuildConfiguration;
class Node;

struct TestCaseInfo {
  QString name;
  int number = -1;
  Utils::FilePath path;
  int line = 0;
};

// --------------------------------------------------------------------
// BuildSystem:
// --------------------------------------------------------------------

// Check buildsystem.md for more information
class PROJECTEXPLORER_EXPORT BuildSystem : public QObject {
  Q_OBJECT

public:
  explicit BuildSystem(Target *target);
  explicit BuildSystem(BuildConfiguration *bc);
  ~BuildSystem() override;

  auto project() const -> Project*;
  auto target() const -> Target*;
  auto kit() const -> Kit*;
  auto buildConfiguration() const -> BuildConfiguration*;
  auto projectFilePath() const -> Utils::FilePath;
  auto projectDirectory() const -> Utils::FilePath;
  auto isWaitingForParse() const -> bool;
  auto requestParse() -> void;
  auto requestDelayedParse() -> void;
  auto requestParseWithCustomDelay(int delayInMs = 1000) -> void;
  auto cancelDelayedParseRequest() -> void;
  auto setParseDelay(int delayInMs) -> void;
  auto parseDelay() const -> int;
  auto isParsing() const -> bool;
  auto hasParsingData() const -> bool;
  auto activeParseEnvironment() const -> Utils::Environment;
  virtual auto addFiles(Node *context, const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded = nullptr) -> bool;
  virtual auto removeFiles(Node *context, const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved = nullptr) -> RemovedFilesFromProject;
  virtual auto deleteFiles(Node *context, const Utils::FilePaths &filePaths) -> bool;
  virtual auto canRenameFile(Node *context, const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  virtual auto renameFile(Node *context, const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  virtual auto addDependencies(Node *context, const QStringList &dependencies) -> bool;
  virtual auto supportsAction(Node *context, ProjectAction action, const Node *node) const -> bool;
  virtual auto name() const -> QString = 0;
  virtual auto filesGeneratedFrom(const Utils::FilePath &sourceFile) const -> Utils::FilePaths;
  virtual auto additionalData(Utils::Id id) const -> QVariant;
  auto setDeploymentData(const DeploymentData &deploymentData) -> void;
  auto deploymentData() const -> DeploymentData;
  auto setApplicationTargets(const QList<BuildTargetInfo> &appTargets) -> void;
  auto applicationTargets() const -> const QList<BuildTargetInfo>;
  auto buildTarget(const QString &buildKey) const -> BuildTargetInfo;
  auto setRootProjectNode(std::unique_ptr<ProjectNode> &&root) -> void;
  virtual auto testcasesInfo() const -> const QList<TestCaseInfo> { return {}; }
  virtual auto commandLineForTests(const QList<QString> &tests, const QStringList &options) const -> Utils::CommandLine;

  class PROJECTEXPLORER_EXPORT ParseGuard {
    friend class BuildSystem;

    explicit ParseGuard(BuildSystem *p);

    auto release() -> void;

  public:
    ParseGuard() = default;
    ~ParseGuard() { release(); }

    auto markAsSuccess() const -> void { m_success = true; }
    auto isSuccess() const -> bool { return m_success; }
    auto guardsProject() const -> bool { return m_buildSystem; }

    ParseGuard(const ParseGuard &other) = delete;
    auto operator=(const ParseGuard &other) -> ParseGuard& = delete;
    ParseGuard(ParseGuard &&other);
    auto operator=(ParseGuard &&other) -> ParseGuard&;

  private:
    BuildSystem *m_buildSystem = nullptr;
    mutable bool m_success = false;
  };

  auto emitBuildSystemUpdated() -> void;
  auto setExtraData(const QString &buildKey, Utils::Id dataKey, const QVariant &data) -> void;
  auto extraData(const QString &buildKey, Utils::Id dataKey) const -> QVariant;
  static auto startNewBuildSystemOutput(const QString &message) -> void;
  static auto appendBuildSystemOutput(const QString &message) -> void;

public:
  // FIXME: Make this private and the BuildSystem a friend
  auto guardParsingRun() -> ParseGuard { return ParseGuard(this); }
  auto disabledReason(const QString &buildKey) const -> QString;
  virtual auto triggerParsing() -> void = 0;

signals:
  auto parsingStarted() -> void;
  auto parsingFinished(bool success) -> void;
  auto deploymentDataChanged() -> void;
  auto applicationTargetsChanged() -> void;
  auto testInformationUpdated() -> void;

protected:
  // Helper methods to manage parsing state and signalling
  // Call in GUI thread before the actual parsing starts
  auto emitParsingStarted() -> void;
  // Call in GUI thread right after the actual parsing is done
  auto emitParsingFinished(bool success) -> void;

private:
  auto requestParseHelper(int delay) -> void; // request a (delayed!) parser run.

  class BuildSystemPrivate *d = nullptr;
};

} // namespace ProjectExplorer
