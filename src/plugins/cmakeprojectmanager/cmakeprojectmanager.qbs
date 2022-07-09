import qbs 1.0

QtcPlugin {
    name: "CMakeProjectManager"

    Depends { name: "Qt.widgets" }
    Depends { name: "Utils" }

    Depends { name: "Core" }
    Depends { name: "CppEditor" }
    Depends { name: "QmlJS" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "TextEditor" }
    Depends { name: "QtSupport" }
    Depends { name: "app_version_header" }

    files: [
        "builddirparameters.cpp",
        "builddirparameters.hpp",
        "cmake_global.hpp",
        "cmakebuildconfiguration.cpp",
        "cmakebuildconfiguration.hpp",
        "cmakebuildstep.cpp",
        "cmakebuildstep.hpp",
        "cmakebuildsystem.cpp",
        "cmakebuildsystem.hpp",
        "cmakebuildtarget.hpp",
        "cmakeconfigitem.cpp",
        "cmakeconfigitem.hpp",
        "cmakeeditor.cpp",
        "cmakeeditor.hpp",
        "cmakefilecompletionassist.cpp",
        "cmakefilecompletionassist.hpp",
        "cmakekitinformation.hpp",
        "cmakekitinformation.cpp",
        "cmakelocatorfilter.cpp",
        "cmakelocatorfilter.hpp",
        "cmakeparser.cpp",
        "cmakeparser.hpp",
        "cmakeprocess.cpp",
        "cmakeprocess.hpp",
        "cmakeproject.cpp",
        "cmakeproject.hpp",
        "cmakeproject.qrc",
        "cmakeprojectimporter.cpp",
        "cmakeprojectimporter.hpp",
        "cmakeprojectconstants.hpp",
        "cmakeprojectmanager.cpp",
        "cmakeprojectmanager.hpp",
        "cmakeprojectnodes.cpp",
        "cmakeprojectnodes.hpp",
        "cmakeprojectplugin.cpp",
        "cmakeprojectplugin.hpp",
        "cmaketool.cpp",
        "cmaketool.hpp",
        "cmaketoolmanager.cpp",
        "cmaketoolmanager.hpp",
        "cmaketoolsettingsaccessor.cpp",
        "cmaketoolsettingsaccessor.hpp",
        "cmakesettingspage.hpp",
        "cmakesettingspage.cpp",
        "cmakeindenter.hpp",
        "cmakeindenter.cpp",
        "cmakeautocompleter.hpp",
        "cmakeautocompleter.cpp",
        "cmakespecificsettings.hpp",
        "cmakespecificsettings.cpp",
        "configmodel.cpp",
        "configmodel.hpp",
        "configmodelitemdelegate.cpp",
        "configmodelitemdelegate.hpp",
        "fileapidataextractor.cpp",
        "fileapidataextractor.hpp",
        "fileapiparser.cpp",
        "fileapiparser.hpp",
        "fileapireader.cpp",
        "fileapireader.hpp",
        "projecttreehelper.cpp",
        "projecttreehelper.hpp"
    ]
}
