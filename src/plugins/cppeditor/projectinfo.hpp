// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "projectpart.hpp"

#include <projectexplorer/project.hpp>
#include <projectexplorer/rawprojectpart.hpp>
#include <projectexplorer/toolchain.hpp>
#include <utils/fileutils.hpp>

#include <QHash>
#include <QSet>
#include <QVector>

#include <memory>

namespace CppEditor {

class CPPEDITOR_EXPORT ProjectInfo {
public:
  using ConstPtr = std::shared_ptr<const ProjectInfo>;

  static auto create(const ProjectExplorer::ProjectUpdateInfo &updateInfo, const QVector<ProjectPart::ConstPtr> &projectParts) -> ConstPtr;
  auto projectParts() const -> const QVector<ProjectPart::ConstPtr>;
  auto sourceFiles() const -> const QSet<QString>;
  auto projectName() const -> QString { return m_projectName; }
  auto projectFilePath() const -> Utils::FilePath { return m_projectFilePath; }
  auto projectRoot() const -> Utils::FilePath { return m_projectFilePath.parentDir(); }
  auto buildRoot() const -> Utils::FilePath { return m_buildRoot; }

  // Comparisons
  auto operator ==(const ProjectInfo &other) const -> bool;
  auto operator !=(const ProjectInfo &other) const -> bool;
  auto definesChanged(const ProjectInfo &other) const -> bool;
  auto configurationChanged(const ProjectInfo &other) const -> bool;
  auto configurationOrFilesChanged(const ProjectInfo &other) const -> bool;

private:
  ProjectInfo(const ProjectExplorer::ProjectUpdateInfo &updateInfo, const QVector<ProjectPart::ConstPtr> &projectParts);

  const QVector<ProjectPart::ConstPtr> m_projectParts;
  const QString m_projectName;
  const Utils::FilePath m_projectFilePath;
  const Utils::FilePath m_buildRoot;
  const ProjectExplorer::HeaderPaths m_headerPaths;
  const QSet<QString> m_sourceFiles;
  const ProjectExplorer::Macros m_defines;
};

} // namespace CppEditor
