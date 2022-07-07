// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environment.hpp"
#include "filepath.hpp"

namespace Utils {

class ORCA_UTILS_EXPORT BuildableHelperLibrary {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::DebuggingHelperLibrary)

public:
  // returns the full path to the first qmake, qmake-qt4, qmake4 that has
  // at least version 2.0.0 and thus is a qt4 qmake
  static auto findSystemQt(const Environment &env) -> FilePath;
  static auto findQtsInEnvironment(const Environment &env, int maxCount = -1) -> FilePaths;
  static auto isQtChooser(const FilePath &filePath) -> bool;
  static auto qtChooserToQmakePath(const FilePath &path) -> FilePath;
  // return true if the qmake at qmakePath is a Qt (used by QtVersion)
  static auto qtVersionForQMake(const FilePath &qmakePath) -> QString;
  // returns something like qmake4, qmake, qmake-qt4 or whatever distributions have chosen (used by QtVersion)
  static auto possibleQMakeCommands() -> QStringList;
  static auto filterForQmakeFileDialog() -> QString;
};

} // Utils
