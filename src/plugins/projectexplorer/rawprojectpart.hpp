// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildtargettype.hpp"
#include "headerpath.hpp"
#include "projectexplorer_export.hpp"
#include "projectmacro.hpp"

// this include style is forced for the cpp unit test mocks
#include <projectexplorer/toolchain.hpp>

#include <utils/cpplanguage_details.hpp>
#include <utils/environment.hpp>
#include <utils/fileutils.hpp>

#include <QPointer>

#include <functional>

namespace ProjectExplorer {

class Kit;
class Project;

class PROJECTEXPLORER_EXPORT RawProjectPartFlags {
public:
  RawProjectPartFlags() = default;
  RawProjectPartFlags(const ToolChain *toolChain, const QStringList &commandLineFlags, const QString &includeFileBaseDir);
  
  QStringList commandLineFlags;
  // The following are deduced from commandLineFlags.
  Utils::WarningFlags warningFlags = Utils::WarningFlags::Default;
  Utils::LanguageExtensions languageExtensions = Utils::LanguageExtension::None;
  QStringList includedFiles;
};

class PROJECTEXPLORER_EXPORT RawProjectPart {
public:
  auto setDisplayName(const QString &displayName) -> void;

  auto setProjectFileLocation(const QString &projectFile, int line = -1, int column = -1) -> void;
  auto setConfigFileName(const QString &configFileName) -> void;
  auto setCallGroupId(const QString &id) -> void;

  // FileIsActive and GetMimeType must be thread-safe.
  using FileIsActive = std::function<bool(const QString &filePath)>;
  using GetMimeType = std::function<QString(const QString &filePath)>;

  auto setFiles(const QStringList &files, const FileIsActive &fileIsActive = {}, const GetMimeType &getMimeType = {}) -> void;
  static auto frameworkDetectionHeuristic(const HeaderPath &header) -> HeaderPath;
  auto setHeaderPaths(const HeaderPaths &headerPaths) -> void;
  auto setIncludePaths(const QStringList &includePaths) -> void;
  auto setPreCompiledHeaders(const QStringList &preCompiledHeaders) -> void;
  auto setIncludedFiles(const QStringList &files) -> void;
  auto setBuildSystemTarget(const QString &target) -> void;
  auto setBuildTargetType(BuildTargetType type) -> void;
  auto setSelectedForBuilding(bool yesno) -> void;
  auto setFlagsForC(const RawProjectPartFlags &flags) -> void;
  auto setFlagsForCxx(const RawProjectPartFlags &flags) -> void;
  auto setMacros(const Macros &macros) -> void;
  auto setQtVersion(Utils::QtMajorVersion qtVersion) -> void;

  QString displayName;
  QString projectFile;
  int projectFileLine = -1;
  int projectFileColumn = -1;
  QString callGroupId;

  // Files
  QStringList files;
  FileIsActive fileIsActive;
  GetMimeType getMimeType;
  QStringList precompiledHeaders;
  QStringList includedFiles;
  HeaderPaths headerPaths;
  QString projectConfigFile; // Generic Project Manager only

  // Build system
  QString buildSystemTarget;
  BuildTargetType buildTargetType = BuildTargetType::Unknown;
  bool selectedForBuilding = true;

  // Flags
  RawProjectPartFlags flagsForC;
  RawProjectPartFlags flagsForCxx;

  // Misc
  Macros projectMacros;
  Utils::QtMajorVersion qtVersion = Utils::QtMajorVersion::Unknown;
};

using RawProjectParts = QVector<RawProjectPart>;

class PROJECTEXPLORER_EXPORT KitInfo {
public:
  explicit KitInfo(Kit *kit);

  auto isValid() const -> bool;

  Kit *kit = nullptr;
  ToolChain *cToolChain = nullptr;
  ToolChain *cxxToolChain = nullptr;

  Utils::QtMajorVersion projectPartQtVersion = Utils::QtMajorVersion::None;

  QString sysRootPath;
};

class PROJECTEXPLORER_EXPORT ToolChainInfo {
public:
  ToolChainInfo() = default;
  ToolChainInfo(const ToolChain *toolChain, const QString &sysRootPath, const Utils::Environment &env);

  auto isValid() const -> bool { return type.isValid(); }

public:
  Utils::Id type;
  bool isMsvc2015ToolChain = false;
  bool targetTripleIsAuthoritative = false;
  unsigned wordWidth = 0;
  QString targetTriple;
  Utils::FilePath compilerFilePath;
  Utils::FilePath installDir;
  QStringList extraCodeModelFlags;
  QString sysRootPath; // For headerPathsRunner.
  ToolChain::BuiltInHeaderPathsRunner headerPathsRunner;
  ToolChain::MacroInspectionRunner macroInspectionRunner;
};

class PROJECTEXPLORER_EXPORT ProjectUpdateInfo {
public:
  using RppGenerator = std::function<RawProjectParts()>;

  ProjectUpdateInfo() = default;
  ProjectUpdateInfo(Project *project, const KitInfo &kitInfo, const Utils::Environment &env, const RawProjectParts &rawProjectParts, const RppGenerator &rppGenerator = {});

  QString projectName;
  Utils::FilePath projectFilePath;
  Utils::FilePath buildRoot;
  RawProjectParts rawProjectParts;
  RppGenerator rppGenerator;
  ToolChainInfo cToolChainInfo;
  ToolChainInfo cxxToolChainInfo;
};

} // namespace ProjectExplorer
