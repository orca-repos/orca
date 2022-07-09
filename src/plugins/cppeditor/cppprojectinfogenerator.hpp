// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectinfo.hpp"

#include <QFutureInterface>

namespace CppEditor::Internal {

class ProjectInfoGenerator {
public:
  ProjectInfoGenerator(const QFutureInterface<ProjectInfo::ConstPtr> &futureInterface, const ProjectExplorer::ProjectUpdateInfo &projectUpdateInfo);

  auto generate() -> ProjectInfo::ConstPtr;

private:
  auto createProjectParts(const ProjectExplorer::RawProjectPart &rawProjectPart, const Utils::FilePath &projectFilePath) -> const QVector<ProjectPart::ConstPtr>;
  auto createProjectPart(const Utils::FilePath &projectFilePath, const ProjectExplorer::RawProjectPart &rawProjectPart, const ProjectFiles &projectFiles, const QString &partName, Utils::Language language, Utils::LanguageExtensions languageExtensions) -> ProjectPart::ConstPtr;

  const QFutureInterface<ProjectInfo::ConstPtr> m_futureInterface;
  const ProjectExplorer::ProjectUpdateInfo &m_projectUpdateInfo;
  bool m_cToolchainMissing = false;
  bool m_cxxToolchainMissing = false;
};

} // namespace CppEditor::Internal
