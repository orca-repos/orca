// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectpart.hpp"

#include <projectexplorer/project.hpp>

#include <utils/algorithm.hpp>

#include <QFile>
#include <QDir>
#include <QTextStream>

using namespace ProjectExplorer;

namespace CppEditor {

auto ProjectPart::id() const -> QString
{
  auto projectPartId = projectFileLocation();
  if (!displayName.isEmpty())
    projectPartId.append(QLatin1Char(' ') + displayName);
  return projectPartId;
}

auto ProjectPart::projectFileLocation() const -> QString
{
  auto location = QDir::fromNativeSeparators(projectFile);
  if (projectFileLine > 0)
    location += ":" + QString::number(projectFileLine);
  if (projectFileColumn > 0)
    location += ":" + QString::number(projectFileColumn);
  return location;
}

auto ProjectPart::belongsToProject(const ProjectExplorer::Project *project) const -> bool
{
  return belongsToProject(project ? project->projectFilePath() : Utils::FilePath());
}

auto ProjectPart::belongsToProject(const Utils::FilePath &project) const -> bool
{
  return topLevelProject == project;
}

auto ProjectPart::readProjectConfigFile(const QString &projectConfigFile) -> QByteArray
{
  QByteArray result;

  QFile f(projectConfigFile);
  if (f.open(QIODevice::ReadOnly)) {
    QTextStream is(&f);
    result = is.readAll().toUtf8();
    f.close();
  }

  return result;
}

// TODO: Why do we keep the file *and* the resulting macros? Why do we read the file
//       in several places?
static auto getProjectMacros(const RawProjectPart &rpp) -> Macros
{
  auto macros = rpp.projectMacros;
  if (!rpp.projectConfigFile.isEmpty())
    macros += Macro::toMacros(ProjectPart::readProjectConfigFile(rpp.projectConfigFile));
  return macros;
}

static auto getHeaderPaths(const RawProjectPart &rpp, const RawProjectPartFlags &flags, const ProjectExplorer::ToolChainInfo &tcInfo) -> HeaderPaths
{
  HeaderPaths headerPaths;

  // Prevent duplicate include paths.
  // TODO: Do this once when finalizing the raw project part?
  std::set<QString> seenPaths;
  for (const auto &p : qAsConst(rpp.headerPaths)) {
    const auto cleanPath = QDir::cleanPath(p.path);
    if (seenPaths.insert(cleanPath).second)
      headerPaths << HeaderPath(cleanPath, p.type);
  }

  if (tcInfo.headerPathsRunner) {
    const auto builtInHeaderPaths = tcInfo.headerPathsRunner(flags.commandLineFlags, tcInfo.sysRootPath, tcInfo.targetTriple);
    for (const auto &header : builtInHeaderPaths) {
      if (seenPaths.insert(header.path).second)
        headerPaths.push_back(HeaderPath{header.path, header.type});
    }
  }
  return headerPaths;
}

static auto getToolchainMacros(const RawProjectPartFlags &flags, const ToolChainInfo &tcInfo, Utils::Language language) -> ToolChain::MacroInspectionReport
{
  ToolChain::MacroInspectionReport report;
  if (tcInfo.macroInspectionRunner) {
    report = tcInfo.macroInspectionRunner(flags.commandLineFlags);
  } else if (language == Utils::Language::C) {
    // No compiler set in kit.
    report.languageVersion = Utils::LanguageVersion::LatestC;
  } else {
    report.languageVersion = Utils::LanguageVersion::LatestCxx;
  }
  return report;
}

static auto getIncludedFiles(const RawProjectPart &rpp, const RawProjectPartFlags &flags) -> QStringList
{
  return !rpp.includedFiles.isEmpty() ? rpp.includedFiles : flags.includedFiles;
}

ProjectPart::ProjectPart(const Utils::FilePath &topLevelProject, const RawProjectPart &rpp, const QString &displayName, const ProjectFiles &files, Utils::Language language, Utils::LanguageExtensions languageExtensions, const RawProjectPartFlags &flags, const ToolChainInfo &tcInfo) : topLevelProject(topLevelProject), displayName(displayName), projectFile(rpp.projectFile), projectConfigFile(rpp.projectConfigFile), projectFileLine(rpp.projectFileLine), projectFileColumn(rpp.projectFileColumn), callGroupId(rpp.callGroupId), language(language), languageExtensions(languageExtensions | flags.languageExtensions), qtVersion(rpp.qtVersion), files(files), includedFiles(getIncludedFiles(rpp, flags)), precompiledHeaders(rpp.precompiledHeaders), headerPaths(getHeaderPaths(rpp, flags, tcInfo)), projectMacros(getProjectMacros(rpp)), buildSystemTarget(rpp.buildSystemTarget), buildTargetType(rpp.buildTargetType), selectedForBuilding(rpp.selectedForBuilding), toolchainType(tcInfo.type), isMsvc2015Toolchain(tcInfo.isMsvc2015ToolChain), toolChainTargetTriple(tcInfo.targetTriple), targetTripleIsAuthoritative(tcInfo.targetTripleIsAuthoritative), toolChainWordWidth(tcInfo.wordWidth == 64 ? ProjectPart::WordWidth64Bit : ProjectPart::WordWidth32Bit), toolChainInstallDir(tcInfo.installDir), compilerFilePath(tcInfo.compilerFilePath), warningFlags(flags.warningFlags), extraCodeModelFlags(tcInfo.extraCodeModelFlags), compilerFlags(flags.commandLineFlags), m_macroReport(getToolchainMacros(flags, tcInfo, language)), languageFeatures(deriveLanguageFeatures()) {}

auto ProjectPart::deriveLanguageFeatures() const -> CPlusPlus::LanguageFeatures
{
  const auto hasCxx = languageVersion >= Utils::LanguageVersion::CXX98;
  const auto hasQt = hasCxx && qtVersion != Utils::QtMajorVersion::None;
  CPlusPlus::LanguageFeatures features;
  features.cxx11Enabled = languageVersion >= Utils::LanguageVersion::CXX11;
  features.cxx14Enabled = languageVersion >= Utils::LanguageVersion::CXX14;
  features.cxxEnabled = hasCxx;
  features.c99Enabled = languageVersion >= Utils::LanguageVersion::C99;
  features.objCEnabled = languageExtensions.testFlag(Utils::LanguageExtension::ObjectiveC);
  features.qtEnabled = hasQt;
  features.qtMocRunEnabled = hasQt;
  if (!hasQt) {
    features.qtKeywordsEnabled = false;
  } else {
    features.qtKeywordsEnabled = !Utils::contains(projectMacros, [](const ProjectExplorer::Macro &macro) { return macro.key == "QT_NO_KEYWORDS"; });
  }
  return features;
}

} // namespace CppEditor
