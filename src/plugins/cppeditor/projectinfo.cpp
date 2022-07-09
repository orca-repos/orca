// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectinfo.hpp"

#include <projectexplorer/abi.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/rawprojectpart.hpp>
#include <projectexplorer/toolchain.hpp>

namespace CppEditor {

auto ProjectInfo::create(const ProjectExplorer::ProjectUpdateInfo &updateInfo, const QVector<ProjectPart::ConstPtr> &projectParts) -> ProjectInfo::ConstPtr
{
  return ConstPtr(new ProjectInfo(updateInfo, projectParts));
}

auto ProjectInfo::projectParts() const -> const QVector<ProjectPart::ConstPtr>
{
  return m_projectParts;
}

auto ProjectInfo::sourceFiles() const -> const QSet<QString>
{
  return m_sourceFiles;
}

auto ProjectInfo::operator ==(const ProjectInfo &other) const -> bool
{
  return m_projectName == other.m_projectName && m_projectFilePath == other.m_projectFilePath && m_buildRoot == other.m_buildRoot && m_projectParts == other.m_projectParts && m_headerPaths == other.m_headerPaths && m_sourceFiles == other.m_sourceFiles && m_defines == other.m_defines;
}

auto ProjectInfo::operator !=(const ProjectInfo &other) const -> bool
{
  return !operator ==(other);
}

auto ProjectInfo::definesChanged(const ProjectInfo &other) const -> bool
{
  return m_defines != other.m_defines;
}

auto ProjectInfo::configurationChanged(const ProjectInfo &other) const -> bool
{
  return definesChanged(other) || m_headerPaths != other.m_headerPaths;
}

auto ProjectInfo::configurationOrFilesChanged(const ProjectInfo &other) const -> bool
{
  return configurationChanged(other) || m_sourceFiles != other.m_sourceFiles;
}

static auto getSourceFiles(const QVector<ProjectPart::ConstPtr> &projectParts) -> QSet<QString>
{
  QSet<QString> sourceFiles;
  for (const auto &part : projectParts) {
    for (const auto &file : qAsConst(part->files))
      sourceFiles.insert(file.path);
  }
  return sourceFiles;
}

static auto getDefines(const QVector<ProjectPart::ConstPtr> &projectParts) -> ProjectExplorer::Macros
{
  ProjectExplorer::Macros defines;
  for (const auto &part : projectParts) {
    defines.append(part->toolChainMacros);
    defines.append(part->projectMacros);
  }
  return defines;
}

static auto getHeaderPaths(const QVector<ProjectPart::ConstPtr> &projectParts) -> ProjectExplorer::HeaderPaths
{
  QSet<ProjectExplorer::HeaderPath> uniqueHeaderPaths;
  for (const auto &part : projectParts) {
    for (const auto &headerPath : qAsConst(part->headerPaths))
      uniqueHeaderPaths.insert(headerPath);
  }
  return ProjectExplorer::HeaderPaths(uniqueHeaderPaths.cbegin(), uniqueHeaderPaths.cend());
}

ProjectInfo::ProjectInfo(const ProjectExplorer::ProjectUpdateInfo &updateInfo, const QVector<ProjectPart::ConstPtr> &projectParts) : m_projectParts(projectParts), m_projectName(updateInfo.projectName), m_projectFilePath(updateInfo.projectFilePath), m_buildRoot(updateInfo.buildRoot), m_headerPaths(getHeaderPaths(projectParts)), m_sourceFiles(getSourceFiles(projectParts)), m_defines(getDefines(projectParts)) {}

} // namespace CppEditor
