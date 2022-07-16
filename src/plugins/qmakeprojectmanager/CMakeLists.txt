add_qtc_plugin(QmakeProjectManager
  DEPENDS QmlJS
  PLUGIN_DEPENDS Core CppEditor QtSupport ResourceEditor TextEditor
  SOURCES
    addlibrarywizard.cpp addlibrarywizard.hpp
    customwidgetwizard/classdefinition.cpp customwidgetwizard/classdefinition.hpp customwidgetwizard/classdefinition.ui
    customwidgetwizard/classlist.cpp customwidgetwizard/classlist.hpp
    customwidgetwizard/customwidgetpluginwizardpage.cpp customwidgetwizard/customwidgetpluginwizardpage.hpp customwidgetwizard/customwidgetpluginwizardpage.ui
    customwidgetwizard/customwidgetwidgetswizardpage.cpp customwidgetwizard/customwidgetwidgetswizardpage.hpp customwidgetwizard/customwidgetwidgetswizardpage.ui
    customwidgetwizard/customwidgetwizard.cpp customwidgetwizard/customwidgetwizard.hpp
    customwidgetwizard/customwidgetwizarddialog.cpp customwidgetwizard/customwidgetwizarddialog.hpp
    customwidgetwizard/filenamingparameters.hpp
    customwidgetwizard/plugingenerator.cpp customwidgetwizard/plugingenerator.hpp
    customwidgetwizard/pluginoptions.hpp
    externaleditors.cpp externaleditors.hpp
    librarydetailscontroller.cpp librarydetailscontroller.hpp
    librarydetailswidget.ui
    makefileparse.cpp makefileparse.hpp
    profilecompletionassist.cpp profilecompletionassist.hpp
    profileeditor.cpp profileeditor.hpp
    profilehighlighter.cpp profilehighlighter.hpp
    profilehoverhandler.cpp profilehoverhandler.hpp
    qmakebuildconfiguration.cpp qmakebuildconfiguration.hpp
    qmakebuildinfo.hpp
    qmakekitinformation.cpp qmakekitinformation.hpp
    qmakemakestep.cpp qmakemakestep.hpp
    qmakenodes.cpp qmakenodes.hpp
    qmakenodetreebuilder.cpp qmakenodetreebuilder.hpp
    qmakeparser.cpp qmakeparser.hpp
    qmakeparsernodes.cpp qmakeparsernodes.hpp
    qmakeproject.cpp qmakeproject.hpp
    qmakeprojectimporter.cpp qmakeprojectimporter.hpp
    qmakeprojectmanager.qrc
    qmakeprojectmanager_global.hpp
    qmakeprojectmanagerconstants.hpp
    qmakeprojectmanagerplugin.cpp qmakeprojectmanagerplugin.hpp
    qmakesettings.cpp qmakesettings.hpp
    qmakestep.cpp qmakestep.hpp
    wizards/qtprojectparameters.cpp wizards/qtprojectparameters.hpp
    wizards/qtwizard.cpp wizards/qtwizard.hpp
    wizards/subdirsprojectwizard.cpp wizards/subdirsprojectwizard.hpp
    wizards/subdirsprojectwizarddialog.cpp wizards/subdirsprojectwizarddialog.hpp
    wizards/wizards.qrc
)
