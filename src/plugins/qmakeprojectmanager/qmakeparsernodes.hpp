// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"
#include "proparser/prowriter.h"
#include "proparser/profileevaluator.h"

#include <core/core-document-interface.hpp>
#include <cppeditor/generatedcodemodelsupport.hpp>
#include <utils/porting.hpp>
#include <utils/textfileformat.hpp>

#include <QFutureWatcher>
#include <QHash>
#include <QLoggingCategory>
#include <QMap>
#include <QPair>
#include <QStringList>

#include <memory>

namespace ProjectExplorer {
class BuildConfiguration;
}

namespace Utils {
class FilePath;
class FileSystemWatcher;
} // namespace Utils;

namespace QtSupport {
class ProFileReader;
}

namespace QmakeProjectManager {
class QmakeBuildSystem;
class QmakeProFile;
class QmakeProject;

//  Type of projects
enum class ProjectType {
  Invalid = 0,
  ApplicationTemplate,
  StaticLibraryTemplate,
  SharedLibraryTemplate,
  ScriptTemplate,
  AuxTemplate,
  SubDirsTemplate
};

// Other variables of interest
enum class Variable {
  Defines = 1,
  IncludePath,
  CppFlags,
  CFlags,
  ExactSource,
  CumulativeSource,
  ExactResource,
  CumulativeResource,
  UiDir,
  HeaderExtension,
  CppExtension,
  MocDir,
  PkgConfig,
  PrecompiledHeader,
  LibDirectories,
  Config,
  Qt,
  QmlImportPath,
  QmlDesignerImportPath,
  Makefile,
  ObjectExt,
  ObjectsDir,
  Version,
  TargetExt,
  TargetVersionExt,
  StaticLibExtension,
  ShLibExtension,
  AndroidAbi,
  AndroidAbis,
  AndroidDeploySettingsFile,
  AndroidPackageSourceDir,
  AndroidExtraLibs,
  AndroidApplicationArgs,
  AppmanPackageDir,
  AppmanManifest,
  IsoIcons,
  QmakeProjectName,
  QmakeCc,
  QmakeCxx
};

auto qHash(Variable key, uint seed = 0) -> Utils::QHashValueType;

namespace Internal {
Q_DECLARE_LOGGING_CATEGORY(qmakeNodesLog)
class QmakeEvalInput;
class QmakeEvalResult;
using QmakeEvalResultPtr = std::shared_ptr<QmakeEvalResult>; // FIXME: Use unique_ptr once we require Qt 6
class QmakePriFileEvalResult;
} // namespace Internal;

class InstallsList;

enum class FileOrigin {
  ExactParse, CumulativeParse
};

auto qHash(FileOrigin fo) -> Utils::QHashValueType;

using SourceFile = QPair<Utils::FilePath, FileOrigin>;
using SourceFiles = QSet<SourceFile>;

// Implements ProjectNode for qmake .pri files
class QMAKEPROJECTMANAGER_EXPORT QmakePriFile {
public:
  QmakePriFile(QmakeBuildSystem *buildSystem, QmakeProFile *qmakeProFile, const Utils::FilePath &filePath);
  explicit QmakePriFile(const Utils::FilePath &filePath);
  virtual ~QmakePriFile();

  auto finishInitialization(QmakeBuildSystem *buildSystem, QmakeProFile *qmakeProFile) -> void;
  auto filePath() const -> Utils::FilePath;
  auto directoryPath() const -> Utils::FilePath;
  virtual auto displayName() const -> QString;
  auto parent() const -> QmakePriFile*;
  auto project() const -> QmakeProject*;
  auto children() const -> QVector<QmakePriFile*>;
  auto findPriFile(const Utils::FilePath &fileName) -> QmakePriFile*;
  auto findPriFile(const Utils::FilePath &fileName) const -> const QmakePriFile*;
  auto knowsFile(const Utils::FilePath &filePath) const -> bool;
  auto makeEmpty() -> void;

  // Files of the specified type declared in this file.
  auto files(const ProjectExplorer::FileType &type) const -> SourceFiles;

  // Files of the specified type declared in this file and in included .pri files.
  auto collectFiles(const ProjectExplorer::FileType &type) const -> const QSet<Utils::FilePath>;
  auto update(const Internal::QmakePriFileEvalResult &result) -> void;
  auto canAddSubProject(const Utils::FilePath &proFilePath) const -> bool;
  auto addSubProject(const Utils::FilePath &proFile) -> bool;
  auto removeSubProjects(const Utils::FilePath &proFilePath) -> bool;
  auto addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded = nullptr) -> bool;
  auto removeFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notRemoved = nullptr) -> bool;
  auto deleteFiles(const Utils::FilePaths &filePaths) -> bool;
  auto canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) -> bool;
  auto addDependencies(const QStringList &dependencies) -> bool;
  auto setProVariable(const QString &var, const QStringList &values, const QString &scope = QString(), int flags = QmakeProjectManager::Internal::ProWriter::ReplaceValues) -> bool;
  auto folderChanged(const QString &changedFolder, const QSet<Utils::FilePath> &newFiles) -> bool;
  auto deploysFolder(const QString &folder) const -> bool;
  auto proFile() const -> QmakeProFile*;
  auto subPriFilesExact() const -> QVector<QmakePriFile*>;

  // Set by parent
  auto includedInExactParse() const -> bool;
  static auto recursiveEnumerate(const QString &folder) -> QSet<Utils::FilePath>;
  auto scheduleUpdate() -> void;
  auto buildSystem() const -> QmakeBuildSystem*;

protected:
  auto setIncludedInExactParse(bool b) -> void;
  static auto varNames(ProjectExplorer::FileType type, QtSupport::ProFileReader *readerExact) -> QStringList;
  static auto varNamesForRemoving() -> QStringList;
  static auto varNameForAdding(const QString &mimeType) -> QString;
  static auto filterFilesProVariables(ProjectExplorer::FileType fileType, const QSet<Utils::FilePath> &files) -> QSet<Utils::FilePath>;
  static auto filterFilesRecursiveEnumerata(ProjectExplorer::FileType fileType, const QSet<Utils::FilePath> &files) -> QSet<Utils::FilePath>;

  enum ChangeType {
    AddToProFile,
    RemoveFromProFile
  };

  enum class Change {
    Save,
    TestOnly
  };

  auto renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath, Change mode) -> bool;
  auto changeFiles(const QString &mimeType, const Utils::FilePaths &filePaths, Utils::FilePaths *notChanged, ChangeType change, Change mode = Change::Save) -> void;
  auto addChild(QmakePriFile *pf) -> void;

private:
  auto setParent(QmakePriFile *p) -> void;
  auto prepareForChange() -> bool;
  static auto ensureWriteableProFile(const QString &file) -> bool;
  auto readProFile() -> QPair<ProFile*, QStringList>;
  static auto readProFileFromContents(const QString &contents) -> QPair<ProFile*, QStringList>;
  auto save(const QStringList &lines) -> void;
  auto saveModifiedEditors() -> bool;
  auto formResources(const Utils::FilePath &formFile) const -> Utils::FilePaths;
  static auto baseVPaths(QtSupport::ProFileReader *reader, const QString &projectDir, const QString &buildDir) -> QStringList;
  static auto fullVPaths(const QStringList &baseVPaths, QtSupport::ProFileReader *reader, const QString &qmakeVariable, const QString &projectDir) -> QStringList;
  static auto extractSources(QHash<int, Internal::QmakePriFileEvalResult*> proToResult, Internal::QmakePriFileEvalResult *fallback, QVector<ProFileEvaluator::SourceFile> sourceFiles, ProjectExplorer::FileType type, bool cumulative) -> void;
  static auto extractInstalls(QHash<int, Internal::QmakePriFileEvalResult*> proToResult, Internal::QmakePriFileEvalResult *fallback, const InstallsList &installList) -> void;
  static auto processValues(Internal::QmakePriFileEvalResult &result) -> void;
  auto watchFolders(const QSet<Utils::FilePath> &folders) -> void;

  auto continuationIndent() const -> QString;

  QPointer<QmakeBuildSystem> m_buildSystem;
  QmakeProFile *m_qmakeProFile = nullptr;
  QmakePriFile *m_parent = nullptr;
  QVector<QmakePriFile*> m_children;

  Utils::TextFileFormat m_textFormat;

  // Memory is cheap...
  QMap<ProjectExplorer::FileType, SourceFiles> m_files;
  QSet<Utils::FilePath> m_recursiveEnumerateFiles; // FIXME: Remove this?!
  QSet<QString> m_watchedFolders;
  const Utils::FilePath m_filePath;
  bool m_includedInExactParse = true;

  friend class QmakeProFile;
};

class QMAKEPROJECTMANAGER_EXPORT TargetInformation {
public:
  bool valid = false;
  QString target;
  Utils::FilePath destDir;
  Utils::FilePath buildDir;
  QString buildTarget;

  auto operator==(const TargetInformation &other) const -> bool
  {
    return target == other.target && valid == other.valid && destDir == other.destDir && buildDir == other.buildDir && buildTarget == other.buildTarget;
  }

  auto operator!=(const TargetInformation &other) const -> bool
  {
    return !(*this == other);
  }

  TargetInformation() = default;
  TargetInformation(const TargetInformation &other) = default;
};

class QMAKEPROJECTMANAGER_EXPORT InstallsItem {
public:
  InstallsItem() = default;
  InstallsItem(QString p, QVector<ProFileEvaluator::SourceFile> f, bool a, bool e) : path(p), files(f), active(a), executable(e) {}
  QString path;
  QVector<ProFileEvaluator::SourceFile> files;
  bool active = false;
  bool executable = false;
};

class QMAKEPROJECTMANAGER_EXPORT InstallsList {
public:
  auto clear() -> void
  {
    targetPath.clear();
    items.clear();
  }

  QString targetPath;
  QVector<InstallsItem> items;
};

// Implements ProjectNode for qmake .pro files
class QMAKEPROJECTMANAGER_EXPORT QmakeProFile : public QmakePriFile {
public:
  QmakeProFile(QmakeBuildSystem *buildSystem, const Utils::FilePath &filePath);
  explicit QmakeProFile(const Utils::FilePath &filePath);
  ~QmakeProFile() override;

  auto isParent(QmakeProFile *node) -> bool;
  auto displayName() const -> QString final;
  auto allProFiles() -> QList<QmakeProFile*>;
  auto findProFile(const Utils::FilePath &fileName) -> QmakeProFile*;
  auto findProFile(const Utils::FilePath &fileName) const -> const QmakeProFile*;
  auto projectType() const -> ProjectType;
  auto variableValue(const Variable var) const -> QStringList;
  auto singleVariableValue(const Variable var) const -> QString;

  auto isSubProjectDeployable(const Utils::FilePath &filePath) const -> bool
  {
    return !m_subProjectsNotToDeploy.contains(filePath);
  }

  auto sourceDir() const -> Utils::FilePath;
  auto generatedFiles(const Utils::FilePath &buildDirectory, const Utils::FilePath &sourceFile, const ProjectExplorer::FileType &sourceFileType) const -> Utils::FilePaths;
  auto extraCompilers() const -> QList<ProjectExplorer::ExtraCompiler*>;
  auto targetInformation() const -> TargetInformation;
  auto installsList() const -> InstallsList;
  auto featureRoots() const -> const QStringList { return m_featureRoots; }
  auto cxxDefines() const -> QByteArray;

  enum AsyncUpdateDelay {
    ParseNow,
    ParseLater
  };

  using QmakePriFile::scheduleUpdate;

  auto scheduleUpdate(AsyncUpdateDelay delay) -> void;
  auto validParse() const -> bool;
  auto parseInProgress() const -> bool;
  auto setParseInProgressRecursive(bool b) -> void;
  auto asyncUpdate() -> void;
  auto isFileFromWildcard(const QString &filePath) const -> bool;

private:
  auto cleanupFutureWatcher() -> void;
  auto setupFutureWatcher() -> void;
  auto setParseInProgress(bool b) -> void;
  auto setValidParseRecursive(bool b) -> void;
  auto setupReader() -> void;
  auto evalInput() const -> Internal::QmakeEvalInput;
  static auto evaluate(const Internal::QmakeEvalInput &input) -> Internal::QmakeEvalResultPtr;
  auto applyEvaluate(const Internal::QmakeEvalResultPtr &parseResult) -> void;
  auto asyncEvaluate(QFutureInterface<Internal::QmakeEvalResultPtr> &fi, Internal::QmakeEvalInput input) -> void;
  auto cleanupProFileReaders() -> void;
  auto updateGeneratedFiles(const Utils::FilePath &buildDir) -> void;
  static auto uiDirPath(QtSupport::ProFileReader *reader, const Utils::FilePath &buildDir) -> QString;
  static auto mocDirPath(QtSupport::ProFileReader *reader, const Utils::FilePath &buildDir) -> QString;
  static auto sysrootify(const QString &path, const QString &sysroot, const QString &baseDir, const QString &outputDir) -> QString;
  static auto includePaths(QtSupport::ProFileReader *reader, const Utils::FilePath &sysroot, const Utils::FilePath &buildDir, const QString &projectDir) -> QStringList;
  static auto libDirectories(QtSupport::ProFileReader *reader) -> QStringList;
  static auto subDirsPaths(QtSupport::ProFileReader *reader, const QString &projectDir, QStringList *subProjectsNotToDeploy, QStringList *errors) -> Utils::FilePaths;
  static auto targetInformation(QtSupport::ProFileReader *reader, QtSupport::ProFileReader *readerBuildPass, const Utils::FilePath &buildDir, const Utils::FilePath &projectFilePath) -> TargetInformation;
  static auto installsList(const QtSupport::ProFileReader *reader, const QString &projectFilePath, const QString &projectDir, const QString &buildDir) -> InstallsList;

  auto setupExtraCompiler(const Utils::FilePath &buildDir, const ProjectExplorer::FileType &fileType, ProjectExplorer::ExtraCompilerFactory *factory) -> void;

  bool m_validParse = false;
  bool m_parseInProgress = false;

  using VariablesHash = QHash<Variable, QStringList>;

  QString m_displayName;
  ProjectType m_projectType = ProjectType::Invalid;
  VariablesHash m_varValues;
  QList<ProjectExplorer::ExtraCompiler*> m_extraCompilers;
  TargetInformation m_qmakeTargetInformation;
  Utils::FilePaths m_subProjectsNotToDeploy;
  InstallsList m_installsList;
  QStringList m_featureRoots;
  std::unique_ptr<Utils::FileSystemWatcher> m_wildcardWatcher;
  QMap<QString, QStringList> m_wildcardDirectoryContents;

  // Async stuff
  QFutureWatcher<Internal::QmakeEvalResultPtr> *m_parseFutureWatcher = nullptr;
  QtSupport::ProFileReader *m_readerExact = nullptr;
  QtSupport::ProFileReader *m_readerCumulative = nullptr;
};

} // namespace QmakeProjectManager
