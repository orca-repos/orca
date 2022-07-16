// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmakeconfigitem.hpp"
#include "cmaketool.hpp"

#include <utils/environment.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>

#include <QString>

namespace CMakeProjectManager {

class CMakeBuildConfiguration;

namespace Internal {

class BuildDirParameters {
public:
  BuildDirParameters();
  explicit BuildDirParameters(CMakeBuildConfiguration *bc);
  BuildDirParameters(const BuildDirParameters &other);

  auto operator=(const BuildDirParameters &other) -> BuildDirParameters&;

  auto isValid() const -> bool;
  auto cmakeTool() const -> CMakeTool*;

  QString projectName;
  Utils::FilePath sourceDirectory;
  Utils::FilePath buildDirectory;
  QString cmakeBuildType;
  Utils::Environment environment;
  Utils::Id cmakeToolId;
  QStringList initialCMakeArguments;
  QStringList configurationChangesArguments;
  QStringList additionalCMakeArguments;
};

} // namespace Internal
} // namespace CMakeProjectManager
