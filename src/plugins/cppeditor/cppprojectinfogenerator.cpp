// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppprojectinfogenerator.hpp"

#include "cppprojectfilecategorizer.hpp"

#include <projectexplorer/headerpath.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/taskhub.hpp>

#include <utils/qtcassert.hpp>

#include <QTimer>

#include <set>

using namespace ProjectExplorer;
using namespace Utils;

namespace CppEditor::Internal {

ProjectInfoGenerator::ProjectInfoGenerator(const QFutureInterface<ProjectInfo::ConstPtr> &futureInterface, const ProjectUpdateInfo &projectUpdateInfo) : m_futureInterface(futureInterface), m_projectUpdateInfo(projectUpdateInfo) {}

auto ProjectInfoGenerator::generate() -> ProjectInfo::ConstPtr
{
  QVector<ProjectPart::ConstPtr> projectParts;
  for (const auto &rpp : m_projectUpdateInfo.rawProjectParts) {
    if (m_futureInterface.isCanceled())
      return {};
    for (const auto &part : createProjectParts(rpp, m_projectUpdateInfo.projectFilePath)) {
      projectParts << part;
    }
  }
  const auto projectInfo = ProjectInfo::create(m_projectUpdateInfo, projectParts);

  static const auto showWarning = [](const QString &message) {
    QTimer::singleShot(0, TaskHub::instance(), [message] {
      TaskHub::addTask(BuildSystemTask(Task::Warning, message));
    });
  };
  if (m_cToolchainMissing) {
    showWarning(QCoreApplication::translate("CppEditor", "The project contains C source files, but the currently active kit " "has no C compiler. The code model will not be fully functional."));
  }
  if (m_cxxToolchainMissing) {
    showWarning(QCoreApplication::translate("CppEditor", "The project contains C++ source files, but the currently active kit " "has no C++ compiler. The code model will not be fully functional."));
  }
  return projectInfo;
}

auto ProjectInfoGenerator::createProjectParts(const RawProjectPart &rawProjectPart, const FilePath &projectFilePath) -> const QVector<ProjectPart::ConstPtr>
{
  QVector<ProjectPart::ConstPtr> result;
  ProjectFileCategorizer cat(rawProjectPart.displayName, rawProjectPart.files, rawProjectPart.fileIsActive);

  if (!cat.hasParts())
    return result;

  if (m_projectUpdateInfo.cxxToolChainInfo.isValid()) {
    if (cat.hasCxxSources()) {
      result << createProjectPart(projectFilePath, rawProjectPart, cat.cxxSources(), cat.partName("C++"), Language::Cxx, LanguageExtension::None);
    }
    if (cat.hasObjcxxSources()) {
      result << createProjectPart(projectFilePath, rawProjectPart, cat.objcxxSources(), cat.partName("Obj-C++"), Language::Cxx, LanguageExtension::ObjectiveC);
    }
  } else if (cat.hasCxxSources() || cat.hasObjcxxSources()) {
    m_cxxToolchainMissing = true;
  }

  if (m_projectUpdateInfo.cToolChainInfo.isValid()) {
    if (cat.hasCSources()) {
      result << createProjectPart(projectFilePath, rawProjectPart, cat.cSources(), cat.partName("C"), Language::C, LanguageExtension::None);
    }

    if (cat.hasObjcSources()) {
      result << createProjectPart(projectFilePath, rawProjectPart, cat.objcSources(), cat.partName("Obj-C"), Language::C, LanguageExtension::ObjectiveC);
    }
  } else if (cat.hasCSources() || cat.hasObjcSources()) {
    m_cToolchainMissing = true;
  }

  return result;
}

auto ProjectInfoGenerator::createProjectPart(const FilePath &projectFilePath, const RawProjectPart &rawProjectPart, const ProjectFiles &projectFiles, const QString &partName, Language language, LanguageExtensions languageExtensions) -> ProjectPart::ConstPtr
{
  RawProjectPartFlags flags;
  ToolChainInfo tcInfo;
  if (language == Language::C) {
    flags = rawProjectPart.flagsForC;
    tcInfo = m_projectUpdateInfo.cToolChainInfo;
  }
  // Use Cxx toolchain for C projects without C compiler in kit and for C++ code
  if (!tcInfo.isValid()) {
    flags = rawProjectPart.flagsForCxx;
    tcInfo = m_projectUpdateInfo.cxxToolChainInfo;
  }

  return ProjectPart::create(projectFilePath, rawProjectPart, partName, projectFiles, language, languageExtensions, flags, tcInfo);
}

} // namespace CppEditor::Internal
