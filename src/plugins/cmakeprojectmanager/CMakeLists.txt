add_qtc_plugin(CMakeProjectManager
  DEPENDS QmlJS
  PLUGIN_DEPENDS Core CppEditor ProjectExplorer TextEditor QtSupport
  SOURCES
    builddirparameters.cpp builddirparameters.hpp
    cmake_global.hpp
    cmakeautocompleter.cpp cmakeautocompleter.hpp
    cmakebuildconfiguration.cpp cmakebuildconfiguration.hpp
    cmakebuildsystem.cpp cmakebuildsystem.hpp
    cmakebuildstep.cpp cmakebuildstep.hpp
    cmakebuildtarget.hpp
    cmakeconfigitem.cpp cmakeconfigitem.hpp
    cmakeeditor.cpp cmakeeditor.hpp
    cmakefilecompletionassist.cpp cmakefilecompletionassist.hpp
    cmakeindenter.cpp cmakeindenter.hpp
    cmakekitinformation.cpp cmakekitinformation.hpp
    cmakelocatorfilter.cpp cmakelocatorfilter.hpp
    cmakeparser.cpp cmakeparser.hpp
    cmakeprocess.cpp cmakeprocess.hpp
    cmakeproject.cpp cmakeproject.hpp
    cmakeproject.qrc
    cmakeprojectconstants.hpp
    cmakeprojectimporter.cpp cmakeprojectimporter.hpp
    cmakeprojectmanager.cpp cmakeprojectmanager.hpp
    cmakeprojectnodes.cpp cmakeprojectnodes.hpp
    cmakeprojectplugin.cpp cmakeprojectplugin.hpp
    cmakesettingspage.cpp cmakesettingspage.hpp
    cmakespecificsettings.cpp cmakespecificsettings.hpp
    cmaketool.cpp cmaketool.hpp
    cmaketoolmanager.cpp cmaketoolmanager.hpp
    cmaketoolsettingsaccessor.cpp cmaketoolsettingsaccessor.hpp
    configmodel.cpp configmodel.hpp
    configmodelitemdelegate.cpp configmodelitemdelegate.hpp
    fileapidataextractor.cpp fileapidataextractor.hpp
    fileapiparser.cpp fileapiparser.hpp
    fileapireader.cpp fileapireader.hpp
    projecttreehelper.cpp projecttreehelper.hpp
)
