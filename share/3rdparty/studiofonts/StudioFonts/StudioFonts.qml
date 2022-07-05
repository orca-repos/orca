// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

pragma Singleton

import QtQuick 2.6

QtObject {
    property FontLoader fontLoader_regular: FontLoader {
        id: fontLoader_regular
        source: "../TitilliumWeb-Regular.ttf"
    }

    readonly property alias titilliumWeb_regular: fontLoader_regular.name
	
    property FontLoader fontLoader_light: FontLoader {
        id: fontLoader_light
        source: "../TitilliumWeb-Light.ttf"
    }

    readonly property alias titilliumWeb_light: fontLoader_light.name
}
