// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakestep.hpp"

#include <qtsupport/qtprojectimporter.hpp>

namespace QmakeProjectManager {
namespace Internal {

// Documentation inside.
class QmakeProjectImporter : public QtSupport::QtProjectImporter {
public:
  QmakeProjectImporter(const Utils::FilePath &path);

  auto importCandidates() -> QStringList final;

private:
  auto examineDirectory(const Utils::FilePath &importPath, QString *warningMessage) const -> QList<void*> final;
  auto matchKit(void *directoryData, const ProjectExplorer::Kit *k) const -> bool final;
  auto createKit(void *directoryData) const -> ProjectExplorer::Kit* final;
  auto buildInfoList(void *directoryData) const -> const QList<ProjectExplorer::BuildInfo> final;
  auto deleteDirectoryData(void *directoryData) const -> void final;
  auto createTemporaryKit(const QtProjectImporter::QtVersionData &data, const QString &parsedSpec, const QMakeStepConfig::OsType &osType) const -> ProjectExplorer::Kit*;
};

} // namespace Internal
} // namespace QmakeProjectManager
