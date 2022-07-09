// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <projectexplorer/project.hpp>

namespace CMakeProjectManager {

namespace Internal {
class CMakeProjectImporter;
class CMakeBuildSystem;
}

class CMAKE_EXPORT CMakeProject final : public ProjectExplorer::Project {
  Q_OBJECT

public:
  explicit CMakeProject(const Utils::FilePath &filename);
  ~CMakeProject() final;

  using IssueType = ProjectExplorer::Task::TaskType;

  auto projectIssues(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks final;
  auto projectImporter() const -> ProjectExplorer::ProjectImporter* final;
  auto addIssue(IssueType type, const QString &text) -> void;
  auto clearIssues() -> void;

protected:
  auto setupTarget(ProjectExplorer::Target *t) -> bool final;

private:
  auto deploymentKnowledge() const -> ProjectExplorer::DeploymentKnowledge override;
  auto makeInstallCommand(const ProjectExplorer::Target *target, const QString &installRoot) -> ProjectExplorer::MakeInstallCommand override;

  mutable Internal::CMakeProjectImporter *m_projectImporter = nullptr;

  friend class Internal::CMakeBuildSystem;

  ProjectExplorer::Tasks m_issues;
};

} // namespace CMakeProjectManager
