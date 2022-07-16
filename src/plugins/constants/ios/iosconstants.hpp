// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>
#include <QLoggingCategory>

namespace Ios {
namespace Internal {

Q_DECLARE_LOGGING_CATEGORY(iosLog)
} // namespace Internal

namespace Constants {

constexpr char EXTRA_INFO_KEY[] = "extraInfo";
constexpr char IOS_SETTINGS_ID[] = "CC.Ios Configurations";
constexpr char IOSQT[] = "Qt4ProjectManager.QtVersion.Ios"; // this literal is replicated to avoid dependencies
constexpr char IOS_DEVICE_TYPE[] = "Ios.Device.Type";
constexpr char IOS_SIMULATOR_TYPE[] = "Ios.Simulator.Type";
constexpr char IOS_DEVICE_ID[] = "iOS Device ";
constexpr char IOS_SIMULATOR_DEVICE_ID[] = "iOS Simulator Device ";
constexpr char IOS_PRESET_BUILD_STEP_ID[] = "Ios.IosPresetBuildStep";
constexpr char IOS_DSYM_BUILD_STEP_ID[] = "Ios.IosDsymBuildStep";
constexpr char IOS_DEPLOY_STEP_ID[] = "Qt4ProjectManager.IosDeployStep";
constexpr char IosTarget[] = "IosTarget"; // QString
constexpr char IosBuildDir[] = "IosBuildDir"; // QString
constexpr char IosCmakeGenerator[] = "IosCmakeGenerator";

constexpr quint16 IOS_DEVICE_PORT_START = 30000;
constexpr quint16 IOS_DEVICE_PORT_END = 31000;
constexpr quint16 IOS_SIMULATOR_PORT_START = 30000;
constexpr quint16 IOS_SIMULATOR_PORT_END = 31000;

} // namespace Constants;
} // namespace Ios
