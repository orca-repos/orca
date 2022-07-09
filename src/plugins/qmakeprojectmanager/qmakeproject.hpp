// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"

#include "qmakenodes.hpp"
#include "qmakeparsernodes.hpp"

#include <projectexplorer/deploymentdata.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/toolchain.hpp>

#include <QStringList>
#include <QFutureInterface>

QT_BEGIN_NAMESPACE
class QMakeGlobals;
class QMakeVfs;
QT_END_NAMESPACE

namespace CppEditor {
class CppProjectUpdater;
}

namespace ProjectExplorer {
class DeploymentData;
}

namespace QtSupport {
class ProFileReader;
}

namespace QmakeProjectManager {

class QmakeBuildConfiguration;

namespace Internal {
class CentralizedFolderWatcher;
}

class QMAKEPROJECTMANAGER_EXPORT QmakeProject final : public ProjectExplorer::Project {
  Q_OBJECT

public:
  explicit QmakeProject(const Utils::FilePath &proFile);
  ~QmakeProject() final;

  auto projectIssues(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks final;
  auto configureAsExampleProject(ProjectExplorer::Kit *kit) -> void final;
  auto projectImporter() const -> ProjectExplorer::ProjectImporter* final;

protected:
  auto fromMap(const QVariantMap &map, QString *errorMessage) -> RestoreResult final;

private:
  auto deploymentKnowledge() const -> ProjectExplorer::DeploymentKnowledge override;

  mutable ProjectExplorer::ProjectImporter *m_projectImporter = nullptr;
};

// FIXME: This export here is only there to appease the current version
// of the appman plugin. This _will_ go away, one way or the other.
class QMAKEPROJECTMANAGER_EXPORT QmakeBuildSystem : public ProjectExplorer::BuildSystem {
  Q_OBJECT

public:
  explicit QmakeBuildSystem(QmakeBuildConfiguration *bc);
  ~QmakeBuildSystem();

  auto supportsAction(ProjectExplorer::Node *context, ProjectExplorer::ProjectAction action, const ProjectExplorer::Node *node) const -> bool override;
  auto addFiles(ProjectExplorer::Node *context, const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded = nullptr) -> bool override;
  auto removeFiles(ProjectExplorer::Node *context, const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved = nullptr) -> ProjectExplorer::RemovedFilesFromProject override;
  auto deleteFiles(ProjectExplorer::Node *context, const Utils::FilePaths &filePaths) -> bool override;
  auto canRenameFile(ProjectExplorer::Node *context, const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool override;
  auto renameFile(ProjectExplorer::Node *context, const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool override;
  auto addDependencies(ProjectExplorer::Node *context, const QStringList &dependencies) -> bool override;
  auto name() const -> QString final { return QLatin1String("qmake"); }
  auto triggerParsing() -> void final;
  auto filesGeneratedFrom(const Utils::FilePath &file) const -> Utils::FilePaths final;
  auto additionalData(Utils::Id id) const -> QVariant final;
  auto asyncUpdate() -> void;
  auto buildFinished(bool success) -> void;
  auto activeTargetWasChanged(ProjectExplorer::Target *) -> void;
  auto executableFor(const QmakeProFile *file) -> Utils::FilePath;
  auto updateCppCodeModel() -> void;
  auto updateQmlJSCodeModel() -> void;

  static auto equalFileList(const QStringList &a, const QStringList &b) -> bool;

  auto updateBuildSystemData() -> void;
  auto collectData(const QmakeProFile *file, ProjectExplorer::DeploymentData &deploymentData) -> void;
  auto collectApplicationData(const QmakeProFile *file, ProjectExplorer::DeploymentData &deploymentData) -> void;
  auto collectLibraryData(const QmakeProFile *file, ProjectExplorer::DeploymentData &deploymentData) -> void;
  auto startAsyncTimer(QmakeProFile::AsyncUpdateDelay delay) -> void;
  auto warnOnToolChainMismatch(const QmakeProFile *pro) const -> void;
  auto testToolChain(ProjectExplorer::ToolChain *tc, const Utils::FilePath &path) const -> void;

  /// \internal
  auto createProFileReader(const QmakeProFile *qmakeProFile) -> QtSupport::ProFileReader*;
  /// \internal
  auto qmakeGlobals() -> QMakeGlobals*;
  /// \internal
  auto qmakeVfs() -> QMakeVfs*;
  /// \internal
  auto qmakeSysroot() -> QString;
  /// \internal
  auto destroyProFileReader(QtSupport::ProFileReader *reader) -> void;
  auto deregisterFromCacheManager() -> void;
  /// \internal
  auto scheduleAsyncUpdateFile(QmakeProFile *file, QmakeProFile::AsyncUpdateDelay delay = QmakeProFile::ParseLater) -> void;
  /// \internal
  auto incrementPendingEvaluateFutures() -> void;
  /// \internal
  auto decrementPendingEvaluateFutures() -> void;
  /// \internal
  auto wasEvaluateCanceled() -> bool;
  auto updateCodeModels() -> void;
  auto updateDocuments() -> void;
  auto watchFolders(const QStringList &l, QmakePriFile *file) -> void;
  auto unwatchFolders(const QStringList &l, QmakePriFile *file) -> void;

  static auto proFileParseError(const QString &errorMessage, const Utils::FilePath &filePath) -> void;

  enum AsyncUpdateState {
    Base,
    AsyncFullUpdatePending,
    AsyncPartialUpdatePending,
    AsyncUpdateInProgress,
    ShuttingDown
  };

  auto asyncUpdateState() const -> AsyncUpdateState;
  auto rootProFile() const -> QmakeProFile*;
  auto notifyChanged(const Utils::FilePath &name) -> void;

  enum Action {
    BUILD,
    REBUILD,
    CLEAN
  };

  auto buildHelper(Action action, bool isFileBuild, QmakeProFileNode *profile, ProjectExplorer::FileNode *buildableFile) -> void;
  auto buildDir(const Utils::FilePath &proFilePath) const -> Utils::FilePath;
  auto qmakeBuildConfiguration() const -> QmakeBuildConfiguration*;
  auto scheduleUpdateAllNowOrLater() -> void;

private:
  auto scheduleUpdateAll(QmakeProFile::AsyncUpdateDelay delay) -> void;
  auto scheduleUpdateAllLater() -> void { scheduleUpdateAll(QmakeProFile::ParseLater); }

  mutable QSet<const QPair<Utils::FilePath, Utils::FilePath>> m_toolChainWarnings;

  // Current configuration
  QString m_oldQtIncludePath;
  QString m_oldQtLibsPath;
  std::unique_ptr<QmakeProFile> m_rootProFile;
  QMakeVfs *m_qmakeVfs = nullptr;
  // cached data during project rescan
  std::unique_ptr<QMakeGlobals> m_qmakeGlobals;
  int m_qmakeGlobalsRefCnt = 0;
  bool m_invalidateQmakeVfsContents = false;
  QString m_qmakeSysroot;
  std::unique_ptr<QFutureInterface<void>> m_asyncUpdateFutureInterface;
  int m_pendingEvaluateFuturesCount = 0;
  AsyncUpdateState m_asyncUpdateState = Base;
  bool m_cancelEvaluate = false;
  QList<QmakeProFile*> m_partialEvaluate;
  CppEditor::CppProjectUpdater *m_cppCodeModelUpdater = nullptr;
  Internal::CentralizedFolderWatcher *m_centralizedFolderWatcher = nullptr;
  ProjectExplorer::BuildSystem::ParseGuard m_guard;
  bool m_firstParseNeeded = true;
};

} // namespace QmakeProjectManager
