// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qtsupport/qtprojectimporter.hpp>

namespace CMakeProjectManager {

class CMakeTool;

namespace Internal {

class CMakeProjectImporter : public QtSupport::QtProjectImporter {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeProjectImporter)

public:
  CMakeProjectImporter(const Utils::FilePath &path);

  auto importCandidates() -> QStringList final;

private:
  auto examineDirectory(const Utils::FilePath &importPath, QString *warningMessage) const -> QList<void*> final;
  auto matchKit(void *directoryData, const ProjectExplorer::Kit *k) const -> bool final;
  auto createKit(void *directoryData) const -> ProjectExplorer::Kit* final;
  auto buildInfoList(void *directoryData) const -> const QList<ProjectExplorer::BuildInfo> final;

  struct CMakeToolData {
    bool isTemporary = false;
    CMakeTool *cmakeTool = nullptr;
  };

  auto findOrCreateCMakeTool(const Utils::FilePath &cmakeToolPath) const -> CMakeToolData;
  auto deleteDirectoryData(void *directoryData) const -> void final;
  auto cleanupTemporaryCMake(ProjectExplorer::Kit *k, const QVariantList &vl) -> void;
  auto persistTemporaryCMake(ProjectExplorer::Kit *k, const QVariantList &vl) -> void;
};

} // namespace Internal
} // namespace CMakeProjectManager
