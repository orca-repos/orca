// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cpptoolsreuse.hpp"
#include "projectpart.hpp"

#include <functional>

namespace ProjectExplorer {
class Project;
}

namespace CppEditor::Internal {

class ProjectPartChooser {
public:
  using FallBackProjectPart = std::function<ProjectPart::ConstPtr()>;
  using ProjectPartsForFile = std::function<QList<ProjectPart::ConstPtr>(const QString &filePath)>;
  using ProjectPartsFromDependenciesForFile = std::function<QList<ProjectPart::ConstPtr>(const QString &filePath)>;

  auto setFallbackProjectPart(const FallBackProjectPart &getter) -> void;
  auto setProjectPartsForFile(const ProjectPartsForFile &getter) -> void;
  auto setProjectPartsFromDependenciesForFile(const ProjectPartsFromDependenciesForFile &getter) -> void;
  auto choose(const QString &filePath, const ProjectPartInfo &currentProjectPartInfo, const QString &preferredProjectPartId, const Utils::FilePath &activeProject, Utils::Language languagePreference, bool projectsUpdated) const -> ProjectPartInfo;

private:
  FallBackProjectPart m_fallbackProjectPart;
  ProjectPartsForFile m_projectPartsForFile;
  ProjectPartsFromDependenciesForFile m_projectPartsFromDependenciesForFile;
};

} // namespace CppEditor::Internal
