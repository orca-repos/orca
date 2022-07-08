// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "deploymentdata.hpp"
#include "kit.hpp"

#include <core/idocument.hpp>

#include <utils/environmentfwd.hpp>
#include <utils/fileutils.hpp>

#include <QObject>
#include <QFileSystemModel>

#include <functional>
#include <memory>

namespace Core {
class Context;
}

namespace Utils {
class Environment;
class MacroExpander;
}

namespace ProjectExplorer {
class BuildInfo;
class BuildSystem;
class ContainerNode;
class EditorConfiguration;
class FolderNode;
class Node;
class ProjectImporter;
class ProjectNode;
class ProjectPrivate;
class Target;

class PROJECTEXPLORER_EXPORT Project : public QObject {
  friend class SessionManager; // for setActiveTarget
  Q_OBJECT

public:
  // Roles to be implemented by all models that are exported via model()
  enum ModelRoles {
    // Absolute file path
    FilePathRole = QFileSystemModel::FilePathRole,
    isParsingRole
  };

  Project(const QString &mimeType, const Utils::FilePath &fileName);
  ~Project() override;

  auto displayName() const -> QString;
  auto id() const -> Utils::Id;
  auto markAsShuttingDown() -> void;
  auto isShuttingDown() const -> bool;
  auto mimeType() const -> QString;
  auto canBuildProducts() const -> bool;
  auto createBuildSystem(Target *target) const -> BuildSystem*;
  auto projectFilePath() const -> Utils::FilePath;
  auto projectDirectory() const -> Utils::FilePath;
  static auto projectDirectory(const Utils::FilePath &top) -> Utils::FilePath;
  // This does not affect nodes, only the root path.
  auto changeRootProjectDirectory() -> void;
  auto rootProjectDirectory() const -> Utils::FilePath;
  virtual auto rootProjectNode() const -> ProjectNode*;
  auto containerNode() const -> ContainerNode*;
  // EditorConfiguration:
  auto editorConfiguration() const -> EditorConfiguration*;
  // Target:
  auto addTargetForDefaultKit() -> Target*;
  auto addTargetForKit(Kit *kit) -> Target*;
  auto removeTarget(Target *target) -> bool;
  auto targets() const -> const QList<Target*>;
  // Note: activeTarget can be 0 (if no targets are defined).
  auto activeTarget() const -> Target*;
  auto target(Utils::Id id) const -> Target*;
  auto target(Kit *k) const -> Target*;
  virtual auto projectIssues(const Kit *k) const -> Tasks;
  static auto copySteps(Target *sourceTarget, Target *newTarget) -> bool;
  auto saveSettings() -> void;

  enum class RestoreResult {
    Ok,
    Error,
    UserAbort
  };

  auto restoreSettings(QString *errorMessage) -> RestoreResult;

  using NodeMatcher = std::function<bool(const Node *)>;
  static const NodeMatcher AllFiles;
  static const NodeMatcher SourceFiles;
  static const NodeMatcher GeneratedFiles;

  auto files(const NodeMatcher &matcher) const -> Utils::FilePaths;
  auto isKnownFile(const Utils::FilePath &filename) const -> bool;
  auto nodeForFilePath(const Utils::FilePath &filePath, const NodeMatcher &extraMatcher = {}) const -> const Node*;
  virtual auto toMap() const -> QVariantMap;
  auto projectContext() const -> Core::Context;
  auto projectLanguages() const -> Core::Context;
  auto namedSettings(const QString &name) const -> QVariant;
  auto setNamedSettings(const QString &name, const QVariant &value) -> void;
  auto setAdditionalEnvironment(const Utils::EnvironmentItems &envItems) -> void;
  auto additionalEnvironment() const -> Utils::EnvironmentItems;
  virtual auto needsConfiguration() const -> bool;
  auto needsBuildConfigurations() const -> bool;
  virtual auto configureAsExampleProject(Kit *kit) -> void;
  virtual auto projectImporter() const -> ProjectImporter*;
  virtual auto deploymentKnowledge() const -> DeploymentKnowledge { return DeploymentKnowledge::Bad; }
  auto hasMakeInstallEquivalent() const -> bool;
  virtual auto makeInstallCommand(const Target *target, const QString &installRoot) -> MakeInstallCommand;
  auto setup(const QList<BuildInfo> &infoList) -> void;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto findNodeForBuildKey(const QString &buildKey) const -> ProjectNode*;
  auto needsInitialExpansion() const -> bool;
  auto setNeedsInitialExpansion(bool needsInitialExpansion) -> void;
  auto setRootProjectNode(std::unique_ptr<ProjectNode> &&root) -> void;

  // Set project files that will be watched and by default trigger the same callback
  // as the main project file.
  using DocGenerator = std::function<std::unique_ptr<Core::IDocument>(const Utils::FilePath &)>;
  using DocUpdater = std::function<void(Core::IDocument *)>;

  auto setExtraProjectFiles(const QSet<Utils::FilePath> &projectDocumentPaths, const DocGenerator &docGenerator = {}, const DocUpdater &docUpdater = {}) -> void;
  auto updateExtraProjectFiles(const QSet<Utils::FilePath> &projectDocumentPaths, const DocUpdater &docUpdater) -> void;
  auto updateExtraProjectFiles(const DocUpdater &docUpdater) -> void;
  auto setDisplayName(const QString &name) -> void;
  auto setProjectLanguage(Utils::Id id, bool enabled) -> void;
  auto setExtraData(const QString &key, const QVariant &data) -> void;
  auto extraData(const QString &key) const -> QVariant;
  auto availableQmlPreviewTranslations(QString *errorMessage) -> QStringList;
  auto modifiedDocuments() const -> QList<Core::IDocument*>;
  auto isModified() const -> bool;
  virtual auto isEditModePreferred() const -> bool;

signals:
  auto projectFileIsDirty(const Utils::FilePath &path) -> void;
  auto displayNameChanged() -> void;
  auto fileListChanged() -> void;
  auto environmentChanged() -> void;

  // Note: activeTarget can be 0 (if no targets are defined).
  auto activeTargetChanged(Target *target) -> void;
  auto aboutToRemoveTarget(Target *target) -> void;
  auto removedTarget(Target *target) -> void;
  auto addedTarget(Target *target) -> void;
  auto settingsLoaded() -> void;
  auto aboutToSaveSettings() -> void;
  auto projectLanguagesUpdated() -> void;
  auto anyParsingStarted(Target *target) -> void;
  auto anyParsingFinished(Target *target, bool success) -> void;
  auto rootProjectDirectoryChanged() -> void;

  #ifdef WITH_TESTS
    void indexingFinished(Utils::Id indexer);
  #endif

protected:
  virtual auto fromMap(const QVariantMap &map, QString *errorMessage) -> RestoreResult;
  auto createTargetFromMap(const QVariantMap &map, int index) -> void;
  virtual auto setupTarget(Target *t) -> bool;
  auto setCanBuildProducts() -> void;
  auto setId(Utils::Id id) -> void;
  auto setProjectLanguages(Core::Context language) -> void;
  auto setHasMakeInstallEquivalent(bool enabled) -> void;
  auto setNeedsBuildConfigurations(bool value) -> void;
  auto setNeedsDeployConfigurations(bool value) -> void;
  static auto createProjectTask(Task::TaskType type, const QString &description) -> Task;
  auto setBuildSystemCreator(const std::function<BuildSystem *(Target *)> &creator) -> void;

private:
  auto addTarget(std::unique_ptr<Target> &&target) -> void;
  auto addProjectLanguage(Utils::Id id) -> void;
  auto removeProjectLanguage(Utils::Id id) -> void;
  auto handleSubTreeChanged(FolderNode *node) -> void;
  auto setActiveTarget(Target *target) -> void;

  friend class ContainerNode;
  ProjectPrivate *d;
};

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Project *)
