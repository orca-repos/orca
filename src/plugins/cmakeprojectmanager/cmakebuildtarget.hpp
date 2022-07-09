// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <projectexplorer/projectmacro.hpp>
#include <projectexplorer/projectnodes.hpp>

#include <utils/fileutils.hpp>

#include <QStringList>

namespace CMakeProjectManager {

enum TargetType {
  ExecutableType,
  StaticLibraryType,
  DynamicLibraryType,
  ObjectLibraryType,
  UtilityType
};

using Backtrace = QVector<ProjectExplorer::FolderNode::LocationInfo>;
using Backtraces = QVector<Backtrace>;

class CMAKE_EXPORT CMakeBuildTarget {
public:
  QString title;
  Utils::FilePath executable; // TODO: rename to output?
  TargetType targetType = UtilityType;
  bool linksToQtGui = false;
  bool qtcRunnable = true;
  Utils::FilePath workingDirectory;
  Utils::FilePath sourceDirectory;
  Utils::FilePath makeCommand;
  Utils::FilePaths libraryDirectories;

  Backtrace backtrace;
  Backtraces dependencyDefinitions;
  Backtraces sourceDefinitions;
  Backtraces defineDefinitions;
  Backtraces includeDefinitions;
  Backtraces installDefinitions;

  // code model
  QList<Utils::FilePath> includeFiles;
  QStringList compilerOptions;
  ProjectExplorer::Macros macros;
  QList<Utils::FilePath> files;
};

} // namespace CMakeProjectManager
