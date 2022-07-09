// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppprojectpartchooser.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace Utils;

namespace CppEditor::Internal {

class ProjectPartPrioritizer {
public:
  struct PrioritizedProjectPart {
    PrioritizedProjectPart(const ProjectPart::ConstPtr &projectPart, int priority) : projectPart(projectPart), priority(priority) {}

    ProjectPart::ConstPtr projectPart;
    int priority = 0;
  };

  ProjectPartPrioritizer(const QList<ProjectPart::ConstPtr> &projectParts, const QString &preferredProjectPartId, const Utils::FilePath &activeProject, Language languagePreference, bool areProjectPartsFromDependencies) : m_preferredProjectPartId(preferredProjectPartId), m_activeProject(activeProject), m_languagePreference(languagePreference)
  {
    // Prioritize
    const auto prioritized = prioritize(projectParts);
    for (const auto &ppp : prioritized)
      m_info.projectParts << ppp.projectPart;

    // Best project part
    m_info.projectPart = m_info.projectParts.first();

    // Hints
    if (m_info.projectParts.size() > 1)
      m_info.hints |= ProjectPartInfo::IsAmbiguousMatch;
    if (prioritized.first().priority > 1000)
      m_info.hints |= ProjectPartInfo::IsPreferredMatch;
    if (areProjectPartsFromDependencies)
      m_info.hints |= ProjectPartInfo::IsFromDependenciesMatch;
    else
      m_info.hints |= ProjectPartInfo::IsFromProjectMatch;
  }

  auto info() const -> ProjectPartInfo
  {
    return m_info;
  }

private:
  auto prioritize(const QList<ProjectPart::ConstPtr> &projectParts) const -> QList<PrioritizedProjectPart>
  {
    // Prioritize
    auto prioritized = Utils::transform(projectParts, [&](const ProjectPart::ConstPtr &projectPart) {
      return PrioritizedProjectPart{projectPart, priority(*projectPart)};
    });

    // Sort according to priority
    const auto lessThan = [&](const PrioritizedProjectPart &p1, const PrioritizedProjectPart &p2) {
      return p1.priority > p2.priority;
    };
    std::stable_sort(prioritized.begin(), prioritized.end(), lessThan);

    return prioritized;
  }

  auto priority(const ProjectPart &projectPart) const -> int
  {
    auto thePriority = 0;

    if (!m_preferredProjectPartId.isEmpty() && projectPart.id() == m_preferredProjectPartId)
      thePriority += 1000;

    if (projectPart.belongsToProject(m_activeProject))
      thePriority += 100;

    if (projectPart.selectedForBuilding)
      thePriority += 10;

    if (isPreferredLanguage(projectPart))
      thePriority += 1;

    return thePriority;
  }

  auto isPreferredLanguage(const ProjectPart &projectPart) const -> bool
  {
    const auto isCProjectPart = projectPart.languageVersion <= LanguageVersion::LatestC;
    return (m_languagePreference == Language::C && isCProjectPart) || (m_languagePreference == Language::Cxx && !isCProjectPart);
  }

private:
  const QString m_preferredProjectPartId;
  const Utils::FilePath m_activeProject;
  Language m_languagePreference = Language::Cxx;

  // Results
  ProjectPartInfo m_info;
};

auto ProjectPartChooser::choose(const QString &filePath, const ProjectPartInfo &currentProjectPartInfo, const QString &preferredProjectPartId, const Utils::FilePath &activeProject, Language languagePreference, bool projectsUpdated) const -> ProjectPartInfo
{
  QTC_CHECK(m_projectPartsForFile);
  QTC_CHECK(m_projectPartsFromDependenciesForFile);
  QTC_CHECK(m_fallbackProjectPart);

  auto projectPart = currentProjectPartInfo.projectPart;
  auto projectParts = m_projectPartsForFile(filePath);
  auto areProjectPartsFromDependencies = false;

  if (projectParts.isEmpty()) {
    if (!projectsUpdated && projectPart && currentProjectPartInfo.hints & ProjectPartInfo::IsFallbackMatch)
      // Avoid re-calculating the expensive dependency table for non-project files.
      return ProjectPartInfo(projectPart, {projectPart}, ProjectPartInfo::IsFallbackMatch);

    // Fall-back step 1: Get some parts through the dependency table:
    projectParts = m_projectPartsFromDependenciesForFile(filePath);
    if (projectParts.isEmpty()) {
      // Fall-back step 2: Use fall-back part from the model manager:
      projectPart = m_fallbackProjectPart();
      return ProjectPartInfo(projectPart, {projectPart}, ProjectPartInfo::IsFallbackMatch);
    }
    areProjectPartsFromDependencies = true;
  }

  return ProjectPartPrioritizer(projectParts, preferredProjectPartId, activeProject, languagePreference, areProjectPartsFromDependencies).info();
}

auto ProjectPartChooser::setFallbackProjectPart(const FallBackProjectPart &getter) -> void
{
  m_fallbackProjectPart = getter;
}

auto ProjectPartChooser::setProjectPartsForFile(const ProjectPartsForFile &getter) -> void
{
  m_projectPartsForFile = getter;
}

auto ProjectPartChooser::setProjectPartsFromDependenciesForFile(const ProjectPartsFromDependenciesForFile &getter) -> void
{
  m_projectPartsFromDependenciesForFile = getter;
}

} // namespace CppEditor::Internal
