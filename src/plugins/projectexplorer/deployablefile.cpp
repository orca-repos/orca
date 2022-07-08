// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "deployablefile.hpp"

#include <utils/fileutils.hpp>

#include <QHash>

using namespace Utils;

namespace ProjectExplorer {

DeployableFile::DeployableFile(const FilePath &localFilePath, const QString &remoteDir, Type type) : m_localFilePath(localFilePath), m_remoteDir(remoteDir), m_type(type) { }

auto DeployableFile::remoteFilePath() const -> QString
{
  return m_remoteDir.isEmpty() ? QString() : m_remoteDir + QLatin1Char('/') + m_localFilePath.fileName();
}

auto DeployableFile::isValid() const -> bool
{
  return !m_localFilePath.toString().isEmpty() && !m_remoteDir.isEmpty();
}

auto DeployableFile::isExecutable() const -> bool
{
  return m_type == TypeExecutable;
}

auto qHash(const DeployableFile &d) -> QHashValueType
{
  return qHash(qMakePair(d.localFilePath().toString(), d.remoteDirectory()));
}

} // namespace ProjectExplorer
