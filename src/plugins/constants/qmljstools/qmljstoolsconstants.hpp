// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace QmlJSTools {
namespace Constants {

constexpr char QML_MIMETYPE[] = "text/x-qml"; // separate def also in projectexplorerconstants.h
constexpr char QBS_MIMETYPE[] = "application/x-qt.qbs+qml";
constexpr char QMLPROJECT_MIMETYPE[] = "application/x-qmlproject";
constexpr char QMLTYPES_MIMETYPE[] = "application/x-qt.meta-info+qml";
constexpr char QMLUI_MIMETYPE[] = "application/x-qt.ui+qml";
constexpr char JS_MIMETYPE[] = "application/javascript";
constexpr char JSON_MIMETYPE[] = "application/json";
constexpr char QML_JS_CODE_STYLE_SETTINGS_ID[] = "A.Code Style";
constexpr char QML_JS_CODE_STYLE_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("QmlJSTools", "Code Style");
constexpr char QML_JS_SETTINGS_ID[] = "QmlJS";
constexpr char QML_JS_SETTINGS_NAME[] = QT_TRANSLATE_NOOP("QmlJSTools", "Qt Quick");
constexpr char M_TOOLS_QMLJS[] = "QmlJSTools.Tools.Menu";
constexpr char RESET_CODEMODEL[] = "QmlJSTools.ResetCodeModel";

} // namespace Constants
} // namespace QmlJSTools
