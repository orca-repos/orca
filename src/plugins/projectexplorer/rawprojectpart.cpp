// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "rawprojectpart.hpp"

#include "abi.hpp"
#include "buildconfiguration.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "projectexplorerconstants.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>

namespace ProjectExplorer {

RawProjectPartFlags::RawProjectPartFlags(const ToolChain *toolChain, const QStringList &commandLineFlags, const QString &includeFileBaseDir)
{
  // Keep the following cheap/non-blocking for the ui thread. Expensive
  // operations are encapsulated in ToolChainInfo as "runners".
  this->commandLineFlags = commandLineFlags;
  if (toolChain) {
    warningFlags = toolChain->warningFlags(commandLineFlags);
    languageExtensions = toolChain->languageExtensions(commandLineFlags);
    includedFiles = toolChain->includedFiles(commandLineFlags, includeFileBaseDir);
  }
}

auto RawProjectPart::setDisplayName(const QString &displayName) -> void
{
  this->displayName = displayName;
}

auto RawProjectPart::setFiles(const QStringList &files, const FileIsActive &fileIsActive, const GetMimeType &getMimeType) -> void
{
  this->files = files;
  this->fileIsActive = fileIsActive;
  this->getMimeType = getMimeType;
}

static auto trimTrailingSlashes(const QString &path) -> QString
{
  auto p = path;
  while (p.endsWith('/') && p.count() > 1) {
    p.chop(1);
  }
  return p;
}

auto RawProjectPart::frameworkDetectionHeuristic(const HeaderPath &header) -> HeaderPath
{
  const auto path = trimTrailingSlashes(header.path);

  if (path.endsWith(".framework"))
    return HeaderPath::makeFramework(path.left(path.lastIndexOf('/')));
  return header;
}

auto RawProjectPart::setProjectFileLocation(const QString &projectFile, int line, int column) -> void
{
  this->projectFile = projectFile;
  projectFileLine = line;
  projectFileColumn = column;
}

auto RawProjectPart::setConfigFileName(const QString &configFileName) -> void
{
  this->projectConfigFile = configFileName;
}

auto RawProjectPart::setBuildSystemTarget(const QString &target) -> void
{
  buildSystemTarget = target;
}

auto RawProjectPart::setCallGroupId(const QString &id) -> void
{
  callGroupId = id;
}

auto RawProjectPart::setQtVersion(Utils::QtMajorVersion qtVersion) -> void
{
  this->qtVersion = qtVersion;
}

auto RawProjectPart::setMacros(const Macros &macros) -> void
{
  this->projectMacros = macros;
}

auto RawProjectPart::setHeaderPaths(const HeaderPaths &headerPaths) -> void
{
  this->headerPaths = headerPaths;
}

auto RawProjectPart::setIncludePaths(const QStringList &includePaths) -> void
{
  this->headerPaths = Utils::transform<QVector>(includePaths, [](const QString &path) {
    return frameworkDetectionHeuristic(HeaderPath::makeUser(path));
  });
}

auto RawProjectPart::setPreCompiledHeaders(const QStringList &preCompiledHeaders) -> void
{
  this->precompiledHeaders = preCompiledHeaders;
}

auto RawProjectPart::setIncludedFiles(const QStringList &files) -> void
{
  includedFiles = files;
}

auto RawProjectPart::setSelectedForBuilding(bool yesno) -> void
{
  this->selectedForBuilding = yesno;
}

auto RawProjectPart::setFlagsForC(const RawProjectPartFlags &flags) -> void
{
  flagsForC = flags;
}

auto RawProjectPart::setFlagsForCxx(const RawProjectPartFlags &flags) -> void
{
  flagsForCxx = flags;
}

auto RawProjectPart::setBuildTargetType(BuildTargetType type) -> void
{
  buildTargetType = type;
}

KitInfo::KitInfo(Kit *kit) : kit(kit)
{
  // Toolchains
  if (kit) {
    cToolChain = ToolChainKitAspect::cToolChain(kit);
    cxxToolChain = ToolChainKitAspect::cxxToolChain(kit);
  }

  // Sysroot
  sysRootPath = SysRootKitAspect::sysRoot(kit).toString();
}

auto KitInfo::isValid() const -> bool
{
  return kit;
}

ToolChainInfo::ToolChainInfo(const ToolChain *toolChain, const QString &sysRootPath, const Utils::Environment &env)
{
  if (toolChain) {
    // Keep the following cheap/non-blocking for the ui thread...
    type = toolChain->typeId();
    isMsvc2015ToolChain = toolChain->targetAbi().osFlavor() == Abi::WindowsMsvc2015Flavor;
    wordWidth = toolChain->targetAbi().wordWidth();
    targetTriple = toolChain->effectiveCodeModelTargetTriple();
    targetTripleIsAuthoritative = !toolChain->explicitCodeModelTargetTriple().isEmpty();
    extraCodeModelFlags = toolChain->extraCodeModelFlags();
    installDir = toolChain->installDir();
    compilerFilePath = toolChain->compilerCommand();

    // ...and save the potentially expensive operations for later so that
    // they can be run from a worker thread.
    this->sysRootPath = sysRootPath;
    headerPathsRunner = toolChain->createBuiltInHeaderPathsRunner(env);
    macroInspectionRunner = toolChain->createMacroInspectionRunner();
  }
}

ProjectUpdateInfo::ProjectUpdateInfo(Project *project, const KitInfo &kitInfo, const Utils::Environment &env, const RawProjectParts &rawProjectParts, const RppGenerator &rppGenerator) : rawProjectParts(rawProjectParts), rppGenerator(rppGenerator), cToolChainInfo(ToolChainInfo(kitInfo.cToolChain, kitInfo.sysRootPath, env)), cxxToolChainInfo(ToolChainInfo(kitInfo.cxxToolChain, kitInfo.sysRootPath, env))
{
  if (project) {
    projectName = project->displayName();
    projectFilePath = project->projectFilePath();
    if (project->activeTarget() && project->activeTarget()->activeBuildConfiguration())
      buildRoot = project->activeTarget()->activeBuildConfiguration()->buildDirectory();
  }
}

} // namespace ProjectExplorer
