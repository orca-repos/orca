// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmakebuildtarget.hpp"
#include "cmakeprojectnodes.hpp"

#include <projectexplorer/rawprojectpart.hpp>

#include <utils/fileutils.hpp>
#include <utils/optional.hpp>

#include <QList>
#include <QSet>
#include <QString>

#include <memory>

namespace CMakeProjectManager {
namespace Internal {

class FileApiData;

class CMakeFileInfo {
public:
  auto operator==(const CMakeFileInfo &other) const -> bool { return path == other.path; }
  friend auto qHash(const CMakeFileInfo &info, uint seed = 0) { return info.path.hash(seed); }

  Utils::FilePath path;
  bool isCMake = false;
  bool isCMakeListsDotTxt = false;
  bool isExternal = false;
  bool isGenerated = false;
};

class FileApiQtcData {
public:
  QString errorMessage;
  CMakeConfig cache;
  QSet<CMakeFileInfo> cmakeFiles;
  QList<CMakeBuildTarget> buildTargets;
  ProjectExplorer::RawProjectParts projectParts;
  std::unique_ptr<CMakeProjectNode> rootProjectNode;
  QString ctestPath;
  bool isMultiConfig = false;
  bool usesAllCapsTargets = false;
};

auto extractData(FileApiData &data, const Utils::FilePath &sourceDirectory, const Utils::FilePath &buildDirectory) -> FileApiQtcData;

} // namespace Internal
} // namespace CMakeProjectManager
