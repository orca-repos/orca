// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "cppprojectfile.hpp"

#include <projectexplorer/buildtargettype.hpp>
#include <projectexplorer/headerpath.hpp>
#include <projectexplorer/projectmacro.hpp>
#include <projectexplorer/rawprojectpart.hpp>

#include <cplusplus/Token.h>

#include <utils/cpplanguage_details.hpp>
#include <utils/filepath.hpp>
#include <utils/id.hpp>

#include <QString>
#include <QSharedPointer>

namespace ProjectExplorer {
class Project;
}

namespace CppEditor {

class CPPEDITOR_EXPORT ProjectPart {
public:
  enum ToolChainWordWidth {
    WordWidth32Bit,
    WordWidth64Bit,
  };

  using ConstPtr = QSharedPointer<const ProjectPart>;
  
  static auto create(const Utils::FilePath &topLevelProject, const ProjectExplorer::RawProjectPart &rpp = {}, const QString &displayName = {}, const ProjectFiles &files = {}, Utils::Language language = Utils::Language::Cxx, Utils::LanguageExtensions languageExtensions = {}, const ProjectExplorer::RawProjectPartFlags &flags = {}, const ProjectExplorer::ToolChainInfo &tcInfo = {}) -> ConstPtr
  {
    return ConstPtr(new ProjectPart(topLevelProject, rpp, displayName, files, language, languageExtensions, flags, tcInfo));
  }

  auto id() const -> QString;
  auto projectFileLocation() const -> QString;
  auto hasProject() const -> bool { return !topLevelProject.isEmpty(); }
  auto belongsToProject(const ProjectExplorer::Project *project) const -> bool;
  auto belongsToProject(const Utils::FilePath &project) const -> bool;
  static auto readProjectConfigFile(const QString &projectConfigFile) -> QByteArray;
  
  const Utils::FilePath topLevelProject;
  const QString displayName;
  const QString projectFile;
  const QString projectConfigFile; // Generic Project Manager only
  const int projectFileLine = -1;
  const int projectFileColumn = -1;
  const QString callGroupId;

  // Versions, features and extensions
  const Utils::Language language = Utils::Language::Cxx;
  const Utils::LanguageVersion &languageVersion = m_macroReport.languageVersion;
  const Utils::LanguageExtensions languageExtensions = Utils::LanguageExtension::None;
  const Utils::QtMajorVersion qtVersion = Utils::QtMajorVersion::Unknown;

  // Files
  const ProjectFiles files;
  const QStringList includedFiles;
  const QStringList precompiledHeaders;
  const ProjectExplorer::HeaderPaths headerPaths;

  // Macros
  const ProjectExplorer::Macros projectMacros;
  const ProjectExplorer::Macros &toolChainMacros = m_macroReport.macros;

  // Build system
  const QString buildSystemTarget;
  const ProjectExplorer::BuildTargetType buildTargetType = ProjectExplorer::BuildTargetType::Unknown;
  const bool selectedForBuilding = true;

  // ToolChain
  const Utils::Id toolchainType;
  const bool isMsvc2015Toolchain = false;
  const QString toolChainTargetTriple;
  const bool targetTripleIsAuthoritative;
  const ToolChainWordWidth toolChainWordWidth = WordWidth32Bit;
  const Utils::FilePath toolChainInstallDir;
  const Utils::FilePath compilerFilePath;
  const Utils::WarningFlags warningFlags = Utils::WarningFlags::Default;

  // Misc
  const QStringList extraCodeModelFlags;
  const QStringList compilerFlags;

private:
  ProjectPart(const Utils::FilePath &topLevelProject, const ProjectExplorer::RawProjectPart &rpp, const QString &displayName, const ProjectFiles &files, Utils::Language language, Utils::LanguageExtensions languageExtensions, const ProjectExplorer::RawProjectPartFlags &flags, const ProjectExplorer::ToolChainInfo &tcInfo);

  auto deriveLanguageFeatures() const -> CPlusPlus::LanguageFeatures;

  const ProjectExplorer::ToolChain::MacroInspectionReport m_macroReport;

public:
  // Must come last due to initialization order.
  const CPlusPlus::LanguageFeatures languageFeatures;
};

} // namespace CppEditor
