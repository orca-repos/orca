// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/fileutils.hpp>
#include <utils/porting.hpp>

#include <QString>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT DeployableFile {
public:
  enum Type {
    TypeNormal,
    TypeExecutable
  };

  DeployableFile() = default;
  DeployableFile(const Utils::FilePath &localFilePath, const QString &remoteDir, Type type = TypeNormal);

  auto localFilePath() const -> Utils::FilePath { return m_localFilePath; }
  auto remoteDirectory() const -> QString { return m_remoteDir; }
  auto remoteFilePath() const -> QString;
  auto isValid() const -> bool;
  auto isExecutable() const -> bool;

  friend auto operator==(const DeployableFile &d1, const DeployableFile &d2) -> bool
  {
    return d1.localFilePath() == d2.localFilePath() && d1.remoteDirectory() == d2.remoteDirectory();
  }

  friend auto operator!=(const DeployableFile &d1, const DeployableFile &d2) -> bool
  {
    return !(d1 == d2);
  }

  friend PROJECTEXPLORER_EXPORT auto qHash(const DeployableFile &d) -> Utils::QHashValueType;

private:
  Utils::FilePath m_localFilePath;
  QString m_remoteDir;
  Type m_type = TypeNormal;
};

} // namespace ProjectExplorer
