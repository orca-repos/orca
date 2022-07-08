add_qtc_plugin(ProjectExplorer
  DEPENDS QtcSsh Qt6::Qml
  PLUGIN_DEPENDS Core TextEditor
  PLUGIN_TEST_DEPENDS GenericProjectManager
  SOURCES
    abi.cpp abi.hpp
    abiwidget.cpp abiwidget.hpp
    abstractprocessstep.cpp abstractprocessstep.hpp
    addrunconfigdialog.cpp addrunconfigdialog.hpp
    allprojectsfilter.cpp allprojectsfilter.hpp
    allprojectsfind.cpp allprojectsfind.hpp
    applicationlauncher.cpp applicationlauncher.hpp
    appoutputpane.cpp appoutputpane.hpp
    baseprojectwizarddialog.cpp baseprojectwizarddialog.hpp
    buildaspects.cpp buildaspects.hpp
    buildpropertiessettings.cpp buildpropertiessettings.hpp
    buildconfiguration.cpp buildconfiguration.hpp
    buildinfo.cpp buildinfo.hpp
    buildmanager.cpp buildmanager.hpp
    buildprogress.cpp buildprogress.hpp
    buildsettingspropertiespage.cpp buildsettingspropertiespage.hpp
    buildstep.cpp buildstep.hpp
    buildsteplist.cpp buildsteplist.hpp
    buildstepspage.cpp buildstepspage.hpp
    buildsystem.cpp buildsystem.hpp
    buildtargetinfo.hpp
    buildtargettype.hpp
    clangparser.cpp clangparser.hpp
    codestylesettingspropertiespage.cpp codestylesettingspropertiespage.hpp codestylesettingspropertiespage.ui
    compileoutputwindow.cpp compileoutputwindow.hpp
    configtaskhandler.cpp configtaskhandler.hpp
    copytaskhandler.cpp copytaskhandler.hpp
    currentprojectfilter.cpp currentprojectfilter.hpp
    currentprojectfind.cpp currentprojectfind.hpp
    customexecutablerunconfiguration.cpp customexecutablerunconfiguration.hpp
    customparser.cpp customparser.hpp
    customparserconfigdialog.cpp customparserconfigdialog.hpp customparserconfigdialog.ui
    customparserssettingspage.cpp customparserssettingspage.hpp
    customtoolchain.cpp customtoolchain.hpp
    customwizard/customwizard.cpp customwizard/customwizard.hpp
    customwizard/customwizardpage.cpp customwizard/customwizardpage.hpp
    customwizard/customwizardparameters.cpp customwizard/customwizardparameters.hpp
    customwizard/customwizardscriptgenerator.cpp customwizard/customwizardscriptgenerator.hpp
    dependenciespanel.cpp dependenciespanel.hpp
    deployablefile.cpp deployablefile.hpp
    deployconfiguration.cpp deployconfiguration.hpp
    deploymentdata.cpp deploymentdata.hpp
    deploymentdataview.cpp deploymentdataview.hpp
    desktoprunconfiguration.cpp desktoprunconfiguration.hpp
    devicesupport/desktopdevice.cpp devicesupport/desktopdevice.hpp
    devicesupport/desktopdevicefactory.cpp devicesupport/desktopdevicefactory.hpp
    devicesupport/desktopdeviceprocess.cpp devicesupport/desktopdeviceprocess.hpp
    devicesupport/desktopprocesssignaloperation.cpp devicesupport/desktopprocesssignaloperation.hpp
    devicesupport/devicecheckbuildstep.cpp devicesupport/devicecheckbuildstep.hpp
    devicesupport/devicefactoryselectiondialog.cpp devicesupport/devicefactoryselectiondialog.hpp devicesupport/devicefactoryselectiondialog.ui
    devicesupport/devicemanager.cpp devicesupport/devicemanager.hpp
    devicesupport/devicemanagermodel.cpp devicesupport/devicemanagermodel.hpp
    devicesupport/deviceprocess.cpp devicesupport/deviceprocess.hpp
    devicesupport/deviceprocessesdialog.cpp devicesupport/deviceprocessesdialog.hpp
    devicesupport/deviceprocesslist.cpp devicesupport/deviceprocesslist.hpp
    devicesupport/devicesettingspage.cpp devicesupport/devicesettingspage.hpp
    devicesupport/devicesettingswidget.cpp devicesupport/devicesettingswidget.hpp devicesupport/devicesettingswidget.ui
    devicesupport/devicetestdialog.cpp devicesupport/devicetestdialog.hpp devicesupport/devicetestdialog.ui
    devicesupport/deviceusedportsgatherer.cpp devicesupport/deviceusedportsgatherer.hpp
    devicesupport/idevice.cpp devicesupport/idevice.hpp
    devicesupport/idevicefactory.cpp devicesupport/idevicefactory.hpp
    devicesupport/idevicewidget.hpp
    devicesupport/localprocesslist.cpp devicesupport/localprocesslist.hpp
    devicesupport/sshdeviceprocess.cpp devicesupport/sshdeviceprocess.hpp
    devicesupport/sshdeviceprocess.cpp devicesupport/sshdeviceprocess.hpp
    devicesupport/sshdeviceprocesslist.cpp devicesupport/sshdeviceprocesslist.hpp
    devicesupport/sshsettingspage.cpp devicesupport/sshsettingspage.hpp
    editorconfiguration.cpp editorconfiguration.hpp
    editorsettingspropertiespage.cpp editorsettingspropertiespage.hpp editorsettingspropertiespage.ui
    environmentaspect.cpp environmentaspect.hpp
    environmentaspectwidget.cpp environmentaspectwidget.hpp
    environmentwidget.cpp environmentwidget.hpp
    expanddata.cpp expanddata.hpp
    extraabi.cpp extraabi.hpp
    extracompiler.cpp extracompiler.hpp
    fileinsessionfinder.cpp fileinsessionfinder.hpp
    filesinallprojectsfind.hpp
    filesinallprojectsfind.cpp
    filterkitaspectsdialog.cpp filterkitaspectsdialog.hpp
    gccparser.cpp gccparser.hpp
    gcctoolchain.cpp gcctoolchain.hpp
    gnumakeparser.cpp gnumakeparser.hpp
    headerpath.hpp
    importwidget.cpp importwidget.hpp
    ioutputparser.cpp ioutputparser.hpp
    ipotentialkit.hpp
    itaskhandler.hpp
    jsonwizard/jsonfieldpage.cpp jsonwizard/jsonfieldpage.hpp jsonwizard/jsonfieldpage_p.hpp
    jsonwizard/jsonfilepage.cpp jsonwizard/jsonfilepage.hpp
    jsonwizard/jsonkitspage.cpp jsonwizard/jsonkitspage.hpp
    jsonwizard/jsonprojectpage.cpp jsonwizard/jsonprojectpage.hpp
    jsonwizard/jsonsummarypage.cpp jsonwizard/jsonsummarypage.hpp
    jsonwizard/jsonwizard.cpp jsonwizard/jsonwizard.hpp
    jsonwizard/jsonwizardfactory.cpp jsonwizard/jsonwizardfactory.hpp
    jsonwizard/jsonwizardfilegenerator.cpp jsonwizard/jsonwizardfilegenerator.hpp
    jsonwizard/jsonwizardgeneratorfactory.cpp jsonwizard/jsonwizardgeneratorfactory.hpp
    jsonwizard/jsonwizardpagefactory.cpp jsonwizard/jsonwizardpagefactory.hpp
    jsonwizard/jsonwizardpagefactory_p.cpp
    jsonwizard/jsonwizardpagefactory_p.hpp
    jsonwizard/wizarddebug.hpp
    jsonwizard/jsonwizardscannergenerator.cpp jsonwizard/jsonwizardscannergenerator.hpp
    kit.cpp kit.hpp
    kitchooser.cpp kitchooser.hpp
    kitfeatureprovider.hpp
    kitinformation.cpp kitinformation.hpp
    kitmanager.cpp kitmanager.hpp
    kitmanagerconfigwidget.cpp kitmanagerconfigwidget.hpp
    kitmodel.cpp kitmodel.hpp
    kitoptionspage.cpp kitoptionspage.hpp
    ldparser.cpp ldparser.hpp
    lldparser.cpp lldparser.hpp
    linuxiccparser.cpp linuxiccparser.hpp
    localenvironmentaspect.cpp localenvironmentaspect.hpp
    makestep.cpp makestep.hpp
    miniprojecttargetselector.cpp miniprojecttargetselector.hpp
    msvcparser.cpp msvcparser.hpp
    msvctoolchain.cpp msvctoolchain.hpp
    namedwidget.cpp namedwidget.hpp
    osparser.cpp osparser.hpp
    panelswidget.cpp panelswidget.hpp
    parseissuesdialog.cpp parseissuesdialog.hpp
    processparameters.cpp processparameters.hpp
    processstep.cpp processstep.hpp
    project.cpp project.hpp
    projectconfiguration.cpp projectconfiguration.hpp
    projectconfigurationmodel.cpp projectconfigurationmodel.hpp
    projectexplorer.cpp projectexplorer.hpp
    projectexplorer.qrc
    projectexplorer_export.hpp
    projectexplorerconstants.cpp
    projectexplorerconstants.hpp
    projectexplorericons.cpp projectexplorericons.hpp
    projectexplorersettings.hpp
    projectexplorersettingspage.cpp projectexplorersettingspage.hpp projectexplorersettingspage.ui
    projectfilewizardextension.cpp projectfilewizardextension.hpp
    projectimporter.cpp projectimporter.hpp
    projectmacro.cpp projectmacro.hpp
    projectmanager.hpp
    projectmodels.cpp projectmodels.hpp
    projectnodes.cpp projectnodes.hpp
    projectpanelfactory.cpp projectpanelfactory.hpp
    projecttree.cpp projecttree.hpp
    projecttreewidget.cpp projecttreewidget.hpp
    projectwelcomepage.cpp projectwelcomepage.hpp
    projectwindow.cpp projectwindow.hpp
    projectwizardpage.cpp projectwizardpage.hpp projectwizardpage.ui
    rawprojectpart.cpp rawprojectpart.hpp
    removetaskhandler.cpp removetaskhandler.hpp
    runconfiguration.cpp runconfiguration.hpp
    runconfigurationaspects.cpp runconfigurationaspects.hpp
    runcontrol.cpp runcontrol.hpp
    runsettingspropertiespage.cpp runsettingspropertiespage.hpp
    selectablefilesmodel.cpp selectablefilesmodel.hpp
    session.cpp session.hpp
    sessiondialog.cpp sessiondialog.hpp sessiondialog.ui
    sessionmodel.cpp sessionmodel.hpp
    sessionview.cpp sessionview.hpp
    showineditortaskhandler.cpp showineditortaskhandler.hpp
    showoutputtaskhandler.cpp showoutputtaskhandler.hpp
    simpleprojectwizard.cpp simpleprojectwizard.hpp
    target.cpp target.hpp
    targetsettingspanel.cpp targetsettingspanel.hpp
    targetsetuppage.cpp targetsetuppage.hpp
    targetsetupwidget.cpp targetsetupwidget.hpp
    task.cpp task.hpp
    taskhub.cpp taskhub.hpp
    taskmodel.cpp taskmodel.hpp
    taskwindow.cpp taskwindow.hpp
    toolchain.cpp toolchain.hpp
    toolchaincache.hpp
    toolchainconfigwidget.cpp toolchainconfigwidget.hpp
    toolchainmanager.cpp toolchainmanager.hpp
    toolchainoptionspage.cpp toolchainoptionspage.hpp
    toolchainsettingsaccessor.cpp toolchainsettingsaccessor.hpp
    treescanner.cpp treescanner.hpp
    userfileaccessor.cpp userfileaccessor.hpp
    vcsannotatetaskhandler.cpp vcsannotatetaskhandler.hpp
    waitforstopdialog.cpp waitforstopdialog.hpp
    xcodebuildparser.cpp xcodebuildparser.hpp
)

extend_qtc_plugin(ProjectExplorer
  CONDITION PROJECT_USER_FILE_EXTENSION
  DEFINES "PROJECT_USER_FILE_EXTENSION=${PROJECT_USER_FILE_EXTENSION}"
)

if (TARGET libclang)
  set(CLANG_BINDIR "$<TARGET_FILE_DIR:libclang>")
endif()
extend_qtc_plugin(ProjectExplorer
  DEFINES "CLANG_BINDIR=\"${CLANG_BINDIR}\""
)

extend_qtc_plugin(ProjectExplorer
  CONDITION WIN32
  SOURCES windebuginterface.cpp windebuginterface.hpp
  DEFINES UNICODE _UNICODE
)

extend_qtc_plugin(ProjectExplorer
  CONDITION journald
  DEPENDS systemd
  SOURCES journaldwatcher.cpp journaldwatcher.hpp
  DEFINES WITH_JOURNALD
)

extend_qtc_plugin(ProjectExplorer
  CONDITION WITH_TESTS
  SOURCES
    jsonwizard/jsonwizard_test.cpp
    outputparser_test.cpp outputparser_test.hpp
)

file(GLOB_RECURSE test_resources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} testdata/*)
qtc_add_resources(ProjectExplorer "testdata"
  CONDITION WITH_TESTS
  PREFIX "/projectexplorer"
  BASE "."
  FILES ${test_resources}
)

qtc_plugin_enabled(_projectexplorer_enabled ProjectExplorer)
if (WITH_TESTS AND _projectexplorer_enabled)
  set_source_files_properties(jsonwizard/jsonwizard_test.cpp
    PROPERTIES HEADER_FILE_ONLY ON
  )
endif()

