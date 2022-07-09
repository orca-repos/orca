// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace QmakeProjectManager {
namespace Constants {

// Menus
constexpr char M_CONTEXT[] = "ProFileEditor.ContextMenu";

// Kinds
constexpr char PROFILE_EDITOR_ID[] = "Qt4.proFileEditor";
constexpr char PROFILE_EDITOR_DISPLAY_NAME[] = QT_TRANSLATE_NOOP("OpenWith::Editors", ".pro File Editor");
constexpr char PROFILE_MIMETYPE[] = "application/vnd.qt.qmakeprofile";
constexpr char PROINCLUDEFILE_MIMETYPE [] = "application/vnd.qt.qmakeproincludefile";
constexpr char PROFEATUREFILE_MIMETYPE [] = "application/vnd.qt.qmakeprofeaturefile";
constexpr char PROCONFIGURATIONFILE_MIMETYPE [] = "application/vnd.qt.qmakeproconfigurationfile";
constexpr char PROCACHEFILE_MIMETYPE [] = "application/vnd.qt.qmakeprocachefile";
constexpr char PROSTASHFILE_MIMETYPE [] = "application/vnd.qt.qmakeprostashfile";

// Actions
constexpr char RUNQMAKE[] = "Qt4Builder.RunQMake";
constexpr char RUNQMAKECONTEXTMENU[] = "Qt4Builder.RunQMakeContextMenu";
constexpr char BUILDSUBDIR[] = "Qt4Builder.BuildSubDir";
constexpr char REBUILDSUBDIR[] = "Qt4Builder.RebuildSubDir";
constexpr char CLEANSUBDIR[] = "Qt4Builder.CleanSubDir";
constexpr char BUILDFILE[] = "Qt4Builder.BuildFile";
constexpr char BUILDSUBDIRCONTEXTMENU[] = "Qt4Builder.BuildSubDirContextMenu";
constexpr char REBUILDSUBDIRCONTEXTMENU[] = "Qt4Builder.RebuildSubDirContextMenu";
constexpr char CLEANSUBDIRCONTEXTMENU[] = "Qt4Builder.CleanSubDirContextMenu";
constexpr char BUILDFILECONTEXTMENU[] = "Qt4Builder.BuildFileContextMenu";
constexpr char ADDLIBRARY[] = "Qt4.AddLibrary";

// Tasks
constexpr char PROFILE_EVALUATE[] = "Qt4ProjectManager.ProFileEvaluate";

// Project
constexpr char QMAKEPROJECT_ID[] = "Qt4ProjectManager.Qt4Project";

constexpr char QMAKE_BC_ID[] = "Qt4ProjectManager.Qt4BuildConfiguration";
constexpr char MAKESTEP_BS_ID[] = "Qt4ProjectManager.MakeStep";
constexpr char QMAKE_BS_ID[] = "QtProjectManager.QMakeBuildStep";

// Kit
constexpr char KIT_INFORMATION_ID[] = "QtPM4.mkSpecInformation";

} // namespace Constants
} // namespace QmakeProjectManager
