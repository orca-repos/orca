// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "builddirparameters.hpp"
#include "cmakebuildtarget.hpp"
#include "cmakeprojectnodes.hpp"
#include "fileapireader.hpp"

#include <projectexplorer/buildsystem.hpp>

#include <utils/fileutils.hpp>
#include <utils/futuresynchronizer.hpp>
#include <utils/temporarydirectory.hpp>

namespace CppEditor {
class CppProjectUpdater;
}

namespace ProjectExplorer {
class ExtraCompiler;
class FolderNode;
}

namespace CMakeProjectManager {

class CMakeBuildConfiguration;
class CMakeProject;

namespace Internal {

// --------------------------------------------------------------------
// CMakeBuildSystem:
// --------------------------------------------------------------------

class CMakeBuildSystem final : public ProjectExplorer::BuildSystem {
  Q_OBJECT

public:
  explicit CMakeBuildSystem(CMakeBuildConfiguration *bc);
  ~CMakeBuildSystem() final;

  auto triggerParsing() -> void final;
  auto supportsAction(ProjectExplorer::Node *context, ProjectExplorer::ProjectAction action, const ProjectExplorer::Node *node) const -> bool final;
  auto addFiles(ProjectExplorer::Node *context, const Utils::FilePaths &filePaths, Utils::FilePaths *) -> bool final;
  auto filesGeneratedFrom(const Utils::FilePath &sourceFile) const -> Utils::FilePaths final;
  auto name() const -> QString final { return QLatin1String("cmake"); }

  // Actions:
  auto runCMake() -> void;
  auto runCMakeAndScanProjectTree() -> void;
  auto runCMakeWithExtraArguments() -> void;
  auto stopCMakeRun() -> void;
  auto persistCMakeState() -> bool;
  auto clearCMakeCache() -> void;

  // Context menu actions:
  auto buildCMakeTarget(const QString &buildTarget) -> void;

  // Queries:
  auto appTargets() const -> const QList<ProjectExplorer::BuildTargetInfo>;
  auto buildTargetTitles() const -> QStringList;
  auto buildTargets() const -> const QList<CMakeBuildTarget>&;
  auto deploymentData() const -> ProjectExplorer::DeploymentData;
  auto cmakeBuildConfiguration() const -> CMakeBuildConfiguration*;
  auto testcasesInfo() const -> QList<ProjectExplorer::TestCaseInfo> const final;
  auto commandLineForTests(const QList<QString> &tests, const QStringList &options) const -> Utils::CommandLine final;

  // Generic CMake helper functions:
  static auto parseCMakeCacheDotTxt(const Utils::FilePath &cacheFile, QString *errorMessage) -> CMakeConfig;
  static auto filteredOutTarget(const CMakeBuildTarget &target) -> bool;
  auto isMultiConfig() const -> bool;
  auto usesAllCapsTargets() const -> bool;
  auto project() const -> CMakeProject*;

signals:
  auto configurationCleared() -> void;

private:
  // Actually ask for parsing:
  enum ReparseParameters {
    REPARSE_DEFAULT = 0,
    // Nothing special:-)
    REPARSE_FORCE_CMAKE_RUN = (1 << 0),
    // Force cmake to run, apply extraCMakeArguments if non-empty
    REPARSE_FORCE_INITIAL_CONFIGURATION = (1 << 1),
    // Force initial configuration arguments to cmake
    REPARSE_FORCE_EXTRA_CONFIGURATION = (1 << 2),
    // Force extra configuration arguments to cmake
    REPARSE_URGENT = (1 << 3),
    // Do not delay the parser run by 1s
  };

  auto reparseParametersString(int reparseFlags) -> QString;
  auto setParametersAndRequestParse(const BuildDirParameters &parameters, const int reparseParameters) -> void;
  auto mustApplyConfigurationChangesArguments(const BuildDirParameters &parameters) const -> bool;

  // State handling:
  // Parser states:
  auto handleParsingSuccess() -> void;
  auto handleParsingError() -> void;

  // Treescanner states:
  auto handleTreeScanningFinished() -> void;

  // Combining Treescanner and Parser states:
  auto combineScanAndParse(bool restoredFromBackup) -> void;
  auto checkAndReportError(QString &errorMessage) -> void;
  auto updateCMakeConfiguration(QString &errorMessage) -> void;
  auto updateProjectData() -> void;
  auto updateFallbackProjectData() -> void;
  auto findExtraCompilers() -> QList<ProjectExplorer::ExtraCompiler*>;
  auto updateQmlJSCodeModel(const QStringList &extraHeaderPaths, const QList<QByteArray> &moduleMappings) -> void;
  auto updateInitialCMakeExpandableVars() -> void;
  auto updateFileSystemNodes() -> void;
  auto handleParsingSucceeded(bool restoredFromBackup) -> void;
  auto handleParsingFailed(const QString &msg) -> void;
  auto wireUpConnections() -> void;
  auto buildDirectory(const BuildDirParameters &parameters) -> Utils::FilePath;
  auto stopParsingAndClearState() -> void;
  auto becameDirty() -> void;
  auto updateReparseParameters(const int parameters) -> void;
  auto takeReparseParameters() -> int;
  auto runCTest() -> void;

  ProjectExplorer::TreeScanner m_treeScanner;
  std::shared_ptr<ProjectExplorer::FolderNode> m_allFiles;
  QHash<QString, bool> m_mimeBinaryCache;
  bool m_waitingForParse = false;
  bool m_combinedScanAndParseResult = false;
  ParseGuard m_currentGuard;
  CppEditor::CppProjectUpdater *m_cppCodeModelUpdater = nullptr;
  QList<ProjectExplorer::ExtraCompiler*> m_extraCompilers;
  QList<CMakeBuildTarget> m_buildTargets;

  // Parsing state:
  BuildDirParameters m_parameters;
  int m_reparseParameters = REPARSE_DEFAULT;
  FileApiReader m_reader;
  mutable bool m_isHandlingError = false;

  // CTest integration
  Utils::FilePath m_ctestPath;
  QList<ProjectExplorer::TestCaseInfo> m_testNames;
  Utils::FutureSynchronizer m_futureSynchronizer;
};

} // namespace Internal
} // namespace CMakeProjectManager
