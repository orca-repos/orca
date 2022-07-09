// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <projectexplorer/projectimporter.hpp>

namespace QtSupport {

class QtVersion;

// Documentation inside.
class QTSUPPORT_EXPORT QtProjectImporter : public ProjectExplorer::ProjectImporter {
public:
  QtProjectImporter(const Utils::FilePath &path);

  class QtVersionData {
  public:
    QtVersion *qt = nullptr;
    bool isTemporary = true;
  };

protected:
  auto findOrCreateQtVersion(const Utils::FilePath &qmakePath) const -> QtVersionData;
  auto createTemporaryKit(const QtVersionData &versionData, const KitSetupFunction &setup) const -> ProjectExplorer::Kit*;

private:
  auto cleanupTemporaryQt(ProjectExplorer::Kit *k, const QVariantList &vl) -> void;
  auto persistTemporaryQt(ProjectExplorer::Kit *k, const QVariantList &vl) -> void;
};

} // namespace QmakeProjectManager
