// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

namespace CMakeProjectManager {
namespace Constants {

constexpr char CMAKE_MIMETYPE[] = "text/x-cmake";
constexpr char CMAKE_PROJECT_MIMETYPE[] = "text/x-cmake-project";
constexpr char CMAKE_EDITOR_ID[] = "CMakeProject.CMakeEditor";
constexpr char RUN_CMAKE[] = "CMakeProject.RunCMake";
constexpr char CLEAR_CMAKE_CACHE[] = "CMakeProject.ClearCache";
constexpr char RESCAN_PROJECT[] = "CMakeProject.RescanProject";
constexpr char RUN_CMAKE_CONTEXT_MENU[] = "CMakeProject.RunCMakeContextMenu";
constexpr char BUILD_FILE_CONTEXT_MENU[] = "CMakeProject.BuildFileContextMenu";
constexpr char BUILD_FILE[] = "CMakeProject.BuildFile";
constexpr char CMAKE_HOME_DIR[] = "CMakeProject.HomeDirectory";

// Project
constexpr char CMAKE_PROJECT_ID[] = "CMakeProjectManager.CMakeProject";
constexpr char CMAKE_BUILDCONFIGURATION_ID[] = "CMakeProjectManager.CMakeBuildConfiguration";

// Menu
constexpr char M_CONTEXT[] = "CMakeEditor.ContextMenu";

// Settings page
constexpr char CMAKE_SETTINGS_PAGE_ID[] = "Z.CMake";

// Snippets
constexpr char CMAKE_SNIPPETS_GROUP_ID[] = "CMake";

// Icons
constexpr char FILE_OVERLAY_CMAKE[] = ":/cmakeproject/images/fileoverlay_cmake.png";

// Actions
constexpr char BUILD_TARGET_CONTEXT_MENU[] = "CMake.BuildTargetContextMenu";

// Build Step
constexpr char CMAKE_BUILD_STEP_ID[] = "CMakeProjectManager.MakeStep";

// Features
constexpr char CMAKE_FEATURE_ID[] = "CMakeProjectManager.Wizard.FeatureCMake";

} // namespace Constants
} // namespace CMakeProjectManager
