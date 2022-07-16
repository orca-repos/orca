// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/fileutils.hpp>
#include <utils/porting.hpp>

#include <QList>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT BuildTargetInfo {
public:
  QString buildKey; // Used to identify this BuildTargetInfo object in its list.
  QString displayName;
  QString displayNameUniquifier;
  Utils::FilePath targetFilePath;
  Utils::FilePath projectFilePath;
  Utils::FilePath workingDirectory;
  bool isQtcRunnable = true;
  bool usesTerminal = false;
  Utils::QHashValueType runEnvModifierHash = 0; // Make sure to update this when runEnvModifier changes!
  std::function<void(Utils::Environment &, bool)> runEnvModifier;

  friend auto operator==(const BuildTargetInfo &ti1, const BuildTargetInfo &ti2) -> bool
  {
    return ti1.buildKey == ti2.buildKey && ti1.displayName == ti2.displayName && ti1.targetFilePath == ti2.targetFilePath && ti1.projectFilePath == ti2.projectFilePath && ti1.workingDirectory == ti2.workingDirectory && ti1.isQtcRunnable == ti2.isQtcRunnable && ti1.usesTerminal == ti2.usesTerminal && ti1.runEnvModifierHash == ti2.runEnvModifierHash;
  }

  friend auto operator!=(const BuildTargetInfo &ti1, const BuildTargetInfo &ti2) -> bool
  {
    return !(ti1 == ti2);
  }

  friend auto qHash(const BuildTargetInfo &ti)
  {
    return qHash(ti.displayName) ^ qHash(ti.buildKey);
  }
};

} // namespace ProjectExplorer
