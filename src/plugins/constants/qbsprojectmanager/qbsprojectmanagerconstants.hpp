// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace QbsProjectManager {
namespace Constants {

// Contexts
constexpr char PROJECT_ID[] = "Qbs.QbsProject";

// MIME types:
constexpr char MIME_TYPE[] = "application/x-qt.qbs+qml";

// Actions:
constexpr char ACTION_REPARSE_QBS[] = "Qbs.Reparse";
constexpr char ACTION_REPARSE_QBS_CONTEXT[] = "Qbs.ReparseCtx";
constexpr char ACTION_BUILD_FILE_CONTEXT[] = "Qbs.BuildFileCtx";
constexpr char ACTION_BUILD_FILE[] = "Qbs.BuildFile";
constexpr char ACTION_BUILD_PRODUCT_CONTEXT[] = "Qbs.BuildProductCtx";
constexpr char ACTION_BUILD_PRODUCT[] = "Qbs.BuildProduct";
constexpr char ACTION_BUILD_SUBPROJECT_CONTEXT[] = "Qbs.BuildSubprojectCtx";
constexpr char ACTION_BUILD_SUBPROJECT[] = "Qbs.BuildSubproduct";
constexpr char ACTION_CLEAN_PRODUCT_CONTEXT[] = "Qbs.CleanProductCtx";
constexpr char ACTION_CLEAN_PRODUCT[] = "Qbs.CleanProduct";
constexpr char ACTION_CLEAN_SUBPROJECT_CONTEXT[] = "Qbs.CleanSubprojectCtx";
constexpr char ACTION_CLEAN_SUBPROJECT[] = "Qbs.CleanSubproject";
constexpr char ACTION_REBUILD_PRODUCT_CONTEXT[] = "Qbs.RebuildProductCtx";
constexpr char ACTION_REBUILD_PRODUCT[] = "Qbs.RebuildProduct";
constexpr char ACTION_REBUILD_SUBPROJECT_CONTEXT[] = "Qbs.RebuildSubprojectCtx";
constexpr char ACTION_REBUILD_SUBPROJECT[] = "Qbs.RebuildSubproject";

// Ids:
constexpr char QBS_BUILDSTEP_ID[] = "Qbs.BuildStep";
constexpr char QBS_CLEANSTEP_ID[] = "Qbs.CleanStep";
constexpr char QBS_INSTALLSTEP_ID[] = "Qbs.InstallStep";
constexpr char QBS_BC_ID[] = "Qbs.QbsBuildConfiguration";

// QBS strings:
constexpr char QBS_VARIANT_DEBUG[] = "debug";
constexpr char QBS_VARIANT_RELEASE[] = "release";
constexpr char QBS_CONFIG_VARIANT_KEY[] = "qbs.defaultBuildVariant";
constexpr char QBS_CONFIG_PROFILE_KEY[] = "qbs.profile";
constexpr char QBS_INSTALL_ROOT_KEY[] = "qbs.installRoot";
constexpr char QBS_CONFIG_DECLARATIVE_DEBUG_KEY[] = "modules.Qt.declarative.qmlDebugging";
constexpr char QBS_CONFIG_QUICK_DEBUG_KEY[] = "modules.Qt.quick.qmlDebugging";
constexpr char QBS_CONFIG_QUICK_COMPILER_KEY[] = "modules.Qt.quick.useCompiler";
constexpr char QBS_CONFIG_SEPARATE_DEBUG_INFO_KEY[] = "modules.cpp.separateDebugInformation";
constexpr char QBS_FORCE_PROBES_KEY[] = "qbspm.forceProbes";

// Toolchain related settings:
constexpr char QBS_TARGETPLATFORM[] = "qbs.targetPlatform";
constexpr char QBS_SYSROOT[] = "qbs.sysroot";
constexpr char QBS_ARCHITECTURES[] = "qbs.architectures";
constexpr char QBS_ARCHITECTURE[] = "qbs.architecture";
constexpr char QBS_TOOLCHAIN[] = "qbs.toolchain";
constexpr char CPP_TOOLCHAINPATH[] = "cpp.toolchainInstallPath";
constexpr char CPP_TOOLCHAINPREFIX[] = "cpp.toolchainPrefix";
constexpr char CPP_COMPILERNAME[] = "cpp.compilerName";
constexpr char CPP_CCOMPILERNAME[] = "cpp.cCompilerName";
constexpr char CPP_CXXCOMPILERNAME[] = "cpp.cxxCompilerName";
constexpr char CPP_PLATFORMCOMMONCOMPILERFLAGS[] = "cpp.platformCommonCompilerFlags";
constexpr char CPP_PLATFORMLINKERFLAGS[] = "cpp.platformLinkerFlags";
constexpr char CPP_VCVARSALLPATH[] = "cpp.vcvarsallPath";
constexpr char XCODE_DEVELOPERPATH[] = "xcode.developerPath";
constexpr char XCODE_SDK[] = "xcode.sdk";

// Settings page
constexpr char QBS_SETTINGS_CATEGORY[]  = "K.Qbs";
constexpr char QBS_SETTINGS_TR_CATEGORY[] = QT_TRANSLATE_NOOP("QbsProjectManager", "Qbs");
constexpr char QBS_SETTINGS_CATEGORY_ICON[]  = ":/projectexplorer/images/build.png";
constexpr char QBS_PROFILING_ENV[] = "QTC_QBS_PROFILING";

} // namespace Constants
} // namespace QbsProjectManager
