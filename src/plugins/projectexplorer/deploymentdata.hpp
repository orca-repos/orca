// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "deployablefile.hpp"
#include "projectexplorer_export.hpp"

#include <utils/environment.hpp>

#include <QList>

namespace ProjectExplorer {

enum class DeploymentKnowledge {
  Perfect,
  Approximative,
  Bad
};

class PROJECTEXPLORER_EXPORT MakeInstallCommand {
public:
  Utils::FilePath command;
  QStringList arguments;
  Utils::Environment environment;
};

class PROJECTEXPLORER_EXPORT DeploymentData {
public:
  auto setFileList(const QList<DeployableFile> &files) -> void { m_files = files; }
  auto allFiles() const -> QList<DeployableFile> { return m_files; }
  auto setLocalInstallRoot(const Utils::FilePath &installRoot) -> void;
  auto localInstallRoot() const -> Utils::FilePath { return m_localInstallRoot; }
  auto addFile(const DeployableFile &file) -> void;
  auto addFile(const Utils::FilePath &localFilePath, const QString &remoteDirectory, DeployableFile::Type type = DeployableFile::TypeNormal) -> void;
  auto addFilesFromDeploymentFile(const QString &deploymentFilePath, const QString &sourceDir) -> QString;
  auto fileCount() const -> int { return m_files.count(); }
  auto fileAt(int index) const -> DeployableFile { return m_files.at(index); }
  auto deployableForLocalFile(const Utils::FilePath &localFilePath) const -> DeployableFile;
  auto operator==(const DeploymentData &other) const -> bool;

private:
  QList<DeployableFile> m_files;
  Utils::FilePath m_localInstallRoot;
};

inline auto operator!=(const DeploymentData &d1, const DeploymentData &d2) -> bool { return !(d1 == d2); }

} // namespace ProjectExplorer
