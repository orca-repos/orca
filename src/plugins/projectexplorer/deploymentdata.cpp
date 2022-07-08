// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deploymentdata.hpp"

#include <utils/algorithm.hpp>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

using namespace Utils;

namespace ProjectExplorer {

auto DeploymentData::setLocalInstallRoot(const FilePath &installRoot) -> void
{
  m_localInstallRoot = installRoot;
}

auto DeploymentData::addFile(const DeployableFile &file) -> void
{
  m_files << file;
}

auto DeploymentData::addFile(const FilePath &localFilePath, const QString &remoteDirectory, DeployableFile::Type type) -> void
{
  addFile(DeployableFile(localFilePath, remoteDirectory, type));
}

auto DeploymentData::deployableForLocalFile(const FilePath &localFilePath) const -> DeployableFile
{
  const auto f = findOrDefault(m_files, equal(&DeployableFile::localFilePath, localFilePath));
  if (f.isValid())
    return f;
  const auto localFileName = localFilePath.fileName();
  return findOrDefault(m_files, [&localFileName](const DeployableFile &d) {
    return d.localFilePath().fileName() == localFileName;
  });
}

auto DeploymentData::operator==(const DeploymentData &other) const -> bool
{
  return toSet(m_files) == toSet(other.m_files) && m_localInstallRoot == other.m_localInstallRoot;
}

auto DeploymentData::addFilesFromDeploymentFile(const QString &deploymentFilePath, const QString &sourceDir) -> QString
{
  const auto sourcePrefix = sourceDir.endsWith('/') ? sourceDir : sourceDir + '/';
  QFile deploymentFile(deploymentFilePath);
  QTextStream deploymentStream;
  QString deploymentPrefix;

  if (!deploymentFile.open(QFile::ReadOnly | QFile::Text))
    return deploymentPrefix;
  deploymentStream.setDevice(&deploymentFile);
  deploymentPrefix = deploymentStream.readLine();
  if (!deploymentPrefix.endsWith('/'))
    deploymentPrefix.append('/');
  if (deploymentStream.device()) {
    while (!deploymentStream.atEnd()) {
      auto line = deploymentStream.readLine();
      if (!line.contains(':'))
        continue;
      const int splitPoint = line.lastIndexOf(':');
      auto sourceFile = line.left(splitPoint);
      if (QFileInfo(sourceFile).isRelative())
        sourceFile.prepend(sourcePrefix);
      auto targetFile = line.mid(splitPoint + 1);
      if (QFileInfo(targetFile).isRelative())
        targetFile.prepend(deploymentPrefix);
      addFile(FilePath::fromString(sourceFile), targetFile);
    }
  }
  return deploymentPrefix;
}

} // namespace ProjectExplorer
