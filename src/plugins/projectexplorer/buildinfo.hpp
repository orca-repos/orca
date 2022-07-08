// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "buildconfiguration.hpp"

#include <utils/fileutils.hpp>
#include <utils/id.hpp>

namespace ProjectExplorer {

class BuildConfigurationFactory;

class PROJECTEXPLORER_EXPORT BuildInfo final {
public:
  BuildInfo() = default;

  QString displayName;
  QString typeName;
  Utils::FilePath buildDirectory;
  Utils::Id kitId;
  BuildConfiguration::BuildType buildType = BuildConfiguration::Unknown;
  QVariant extraInfo;
  const BuildConfigurationFactory *factory = nullptr;

  auto operator==(const BuildInfo &o) const -> bool
  {
    return factory == o.factory && displayName == o.displayName && typeName == o.typeName && buildDirectory == o.buildDirectory && kitId == o.kitId && buildType == o.buildType;
  }
};

} // namespace ProjectExplorer
