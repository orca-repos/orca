// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>
#include <utils/id.hpp>

namespace Android {
namespace Internal {

#ifdef Q_OS_WIN32
#define ANDROID_BAT_SUFFIX ".bat"
#else
#define ANDROID_BAT_SUFFIX ""
#endif

} // namespace Internal

namespace Constants {

constexpr char ANDROID_SETTINGS_ID[] = "BB.Android Configurations";
constexpr char ANDROID_TOOLCHAIN_TYPEID[] = "Qt4ProjectManager.ToolChain.Android";
constexpr char ANDROID_QT_TYPE[] = "Qt4ProjectManager.QtVersion.Android";
constexpr char ANDROID_AM_START_ARGS[] = "Android.AmStartArgs";
constexpr char ANDROID_PRESTARTSHELLCMDLIST[] = "Android.PreStartShellCmdList";
constexpr char ANDROID_POSTFINISHSHELLCMDLIST[] = "Android.PostFinishShellCmdList";
constexpr char ANDROID_DEVICE_TYPE[] = "Android.Device.Type";
constexpr char ANDROID_DEVICE_ID[] = "Android Device";
constexpr char ANDROID_MANIFEST_MIME_TYPE[] = "application/vnd.google.android.android_manifest";
constexpr char ANDROID_MANIFEST_EDITOR_ID[] = "Android.AndroidManifestEditor.Id";
constexpr char ANDROID_MANIFEST_EDITOR_CONTEXT[] = "Android.AndroidManifestEditor.Id";
constexpr char ANDROID_KIT_NDK[] = "Android.NDK";
constexpr char ANDROID_KIT_SDK[] = "Android.SDK";
constexpr char ANDROID_BUILD_DIRECTORY[] = "android-build";
constexpr char JAVA_EDITOR_ID[] = "java.editor";
constexpr char JLS_SETTINGS_ID[] = "Java::JLSSettingsID";
constexpr char JAVA_MIMETYPE[] = "text/x-java";
constexpr char ANDROID_ARCHITECTURE[] = "Android.Architecture";
constexpr char ANDROID_PACKAGE_SOURCE_DIR[] = "ANDROID_PACKAGE_SOURCE_DIR";
constexpr char ANDROID_EXTRA_LIBS[] = "ANDROID_EXTRA_LIBS";
constexpr char ANDROID_ABI[] = "ANDROID_ABI";
constexpr char ANDROID_TARGET_ARCH[] = "ANDROID_TARGET_ARCH";
constexpr char ANDROID_ABIS[] = "ANDROID_ABIS";
constexpr char ANDROID_APPLICATION_ARGUMENTS[] = "ANDROID_APPLICATION_ARGUMENTS";
constexpr char ANDROID_DEPLOYMENT_SETTINGS_FILE[] = "ANDROID_DEPLOYMENT_SETTINGS_FILE";
constexpr char ANDROID_SO_LIBS_PATHS[] = "ANDROID_SO_LIBS_PATHS";
constexpr char ANDROID_PACKAGE_INSTALL_STEP_ID[] = "Qt4ProjectManager.AndroidPackageInstallationStep";
constexpr char ANDROID_BUILD_APK_ID[] = "QmakeProjectManager.AndroidBuildApkStep";
constexpr char ANDROID_DEPLOY_QT_ID[] = "Qt4ProjectManager.AndroidDeployQtStep";
constexpr char AndroidPackageSourceDir[] = "AndroidPackageSourceDir"; // QString
constexpr char AndroidDeploySettingsFile[] = "AndroidDeploySettingsFile"; // QString
constexpr char AndroidExtraLibs[] = "AndroidExtraLibs";  // QStringList
constexpr char AndroidAbi[] = "AndroidAbi"; // QString
constexpr char AndroidAbis[] = "AndroidAbis"; // QStringList
constexpr char AndroidMkSpecAbis[] = "AndroidMkSpecAbis"; // QStringList
constexpr char AndroidSoLibPath[] = "AndroidSoLibPath"; // QStringList
constexpr char AndroidTargets[] = "AndroidTargets"; // QStringList
constexpr char AndroidApplicationArgs[] = "AndroidApplicationArgs"; // QString

// For qbs support
constexpr char AndroidApk[] = "Android.APK"; // QStringList
constexpr char AndroidManifest[] = "Android.Manifest"; // QStringList
constexpr char AndroidNdkPlatform[] = "AndroidNdkPlatform"; //QString
constexpr char NdkLocation[] = "NdkLocation"; // FileName
constexpr char SdkLocation[] = "SdkLocation"; // FileName

// Android Device
const Utils::Id AndroidSerialNumber = "AndroidSerialNumber";
const Utils::Id AndroidAvdName = "AndroidAvdName";
const Utils::Id AndroidCpuAbi = "AndroidCpuAbi";
const Utils::Id AndroidAvdTarget = "AndroidAvdTarget";
const Utils::Id AndroidAvdDevice = "AndroidAvdDevice";
const Utils::Id AndroidAvdSkin = "AndroidAvdSkin";
const Utils::Id AndroidAvdSdcard = "AndroidAvdSdcard";
const Utils::Id AndroidSdk = "AndroidSdk";

} // namespace Constants;
} // namespace Android
