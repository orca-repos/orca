if (NOT IS_ABSOLUTE ${IDE_LOGO_PATH})
  set(IDE_LOGO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${IDE_LOGO_PATH}")
endif()
configure_file(core_logo.qrc.cmakein core_logo_cmake.qrc)

add_qtc_plugin(Core
  DEPENDS Qt6::PrintSupport Qt6::Qml Qt6::Sql Qt6::Gui Qt6::GuiPrivate
  PUBLIC_DEPENDS Aggregation ExtensionSystem Utils app_version
  SOURCES
    actionmanager/actioncontainer.cpp actionmanager/actioncontainer.hpp actionmanager/actioncontainer_p.hpp
    actionmanager/actionmanager.cpp actionmanager/actionmanager.hpp actionmanager/actionmanager_p.hpp
    actionmanager/command.cpp actionmanager/command.hpp actionmanager/command_p.hpp
    actionmanager/commandbutton.cpp actionmanager/commandbutton.hpp
    actionmanager/commandmappings.cpp actionmanager/commandmappings.hpp
    actionmanager/commandsfile.cpp actionmanager/commandsfile.hpp
    basefilewizard.cpp basefilewizard.hpp
    basefilewizardfactory.cpp basefilewizardfactory.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/core_logo_cmake.qrc
    core.qrc
    core_global.hpp
    coreconstants.hpp
    coreicons.cpp coreicons.hpp
    corejsextensions.cpp corejsextensions.hpp
    coreplugin.cpp coreplugin.hpp
    designmode.cpp designmode.hpp
    dialogs/addtovcsdialog.cpp dialogs/addtovcsdialog.hpp dialogs/addtovcsdialog.ui
    dialogs/codecselector.cpp dialogs/codecselector.hpp
    dialogs/externaltoolconfig.cpp dialogs/externaltoolconfig.hpp dialogs/externaltoolconfig.ui
    dialogs/filepropertiesdialog.cpp dialogs/filepropertiesdialog.hpp dialogs/filepropertiesdialog.ui
    dialogs/ioptionspage.cpp dialogs/ioptionspage.hpp
    dialogs/newdialog.cpp dialogs/newdialog.hpp dialogs/newdialog.ui
    dialogs/newdialogwidget.cpp dialogs/newdialogwidget.hpp
    dialogs/openwithdialog.cpp dialogs/openwithdialog.hpp dialogs/openwithdialog.ui
    dialogs/promptoverwritedialog.cpp dialogs/promptoverwritedialog.hpp
    dialogs/readonlyfilesdialog.cpp dialogs/readonlyfilesdialog.hpp dialogs/readonlyfilesdialog.ui
    dialogs/restartdialog.cpp dialogs/restartdialog.hpp
    dialogs/saveitemsdialog.cpp dialogs/saveitemsdialog.hpp dialogs/saveitemsdialog.ui
    dialogs/settingsdialog.cpp dialogs/settingsdialog.hpp
    dialogs/shortcutsettings.cpp dialogs/shortcutsettings.hpp
    diffservice.cpp diffservice.hpp
    documentmanager.cpp documentmanager.hpp
    editmode.cpp editmode.hpp
    editormanager/documentmodel.cpp editormanager/documentmodel.hpp editormanager/documentmodel_p.hpp
    editormanager/editorarea.cpp editormanager/editorarea.hpp
    editormanager/editormanager.cpp editormanager/editormanager.hpp editormanager/editormanager_p.hpp
    editormanager/editorview.cpp editormanager/editorview.hpp
    editormanager/editorwindow.cpp editormanager/editorwindow.hpp
    editormanager/ieditor.cpp editormanager/ieditor.hpp
    editormanager/ieditorfactory.cpp editormanager/ieditorfactory.hpp editormanager/ieditorfactory_p.hpp
    editormanager/iexternaleditor.cpp editormanager/iexternaleditor.hpp
    editormanager/openeditorsview.cpp editormanager/openeditorsview.hpp
    editormanager/openeditorswindow.cpp editormanager/openeditorswindow.hpp
    editormanager/systemeditor.cpp editormanager/systemeditor.hpp
    editortoolbar.cpp editortoolbar.hpp
    externaltool.cpp externaltool.hpp
    externaltoolmanager.cpp externaltoolmanager.hpp
    fancyactionbar.cpp fancyactionbar.hpp
    fancyactionbar.qrc
    fancytabwidget.cpp fancytabwidget.hpp
    featureprovider.cpp featureprovider.hpp
    fileiconprovider.cpp fileiconprovider.hpp
    fileutils.cpp fileutils.hpp
    find/basetextfind.cpp find/basetextfind.hpp
    find/currentdocumentfind.cpp find/currentdocumentfind.hpp
    find/find.qrc
    find/finddialog.ui
    find/findplugin.cpp find/findplugin.hpp
    find/findtoolbar.cpp find/findtoolbar.hpp
    find/findtoolwindow.cpp find/findtoolwindow.hpp
    find/findwidget.ui
    find/highlightscrollbarcontroller.cpp find/highlightscrollbarcontroller.hpp
    find/ifindfilter.cpp find/ifindfilter.hpp
    find/ifindsupport.cpp find/ifindsupport.hpp
    find/itemviewfind.cpp find/itemviewfind.hpp
    find/optionspopup.cpp find/optionspopup.hpp
    find/searchresultcolor.hpp
    find/searchresultitem.hpp
    find/searchresulttreeitemdelegate.cpp find/searchresulttreeitemdelegate.hpp
    find/searchresulttreeitemroles.hpp
    find/searchresulttreeitems.cpp find/searchresulttreeitems.hpp
    find/searchresulttreemodel.cpp find/searchresulttreemodel.hpp
    find/searchresulttreeview.cpp find/searchresulttreeview.hpp
    find/searchresultwidget.cpp find/searchresultwidget.hpp
    find/searchresultwindow.cpp find/searchresultwindow.hpp
    find/textfindconstants.hpp
    findplaceholder.cpp findplaceholder.hpp
    foldernavigationwidget.cpp
    foldernavigationwidget.hpp
    generalsettings.cpp generalsettings.hpp generalsettings.ui
    generatedfile.cpp generatedfile.hpp
    helpitem.cpp helpitem.hpp
    helpmanager.cpp helpmanager.hpp helpmanager_implementation.hpp
    icontext.cpp icontext.hpp
    icore.cpp icore.hpp
    idocument.cpp idocument.hpp
    idocumentfactory.cpp idocumentfactory.hpp
    ifilewizardextension.hpp
    imode.cpp imode.hpp
    inavigationwidgetfactory.cpp inavigationwidgetfactory.hpp
    ioutputpane.cpp ioutputpane.hpp
    iversioncontrol.cpp iversioncontrol.hpp
    iwelcomepage.cpp iwelcomepage.hpp
    iwizardfactory.cpp iwizardfactory.hpp
    jsexpander.cpp jsexpander.hpp
    locator/basefilefilter.cpp locator/basefilefilter.hpp
    locator/commandlocator.cpp locator/commandlocator.hpp
    locator/directoryfilter.cpp locator/directoryfilter.hpp locator/directoryfilter.ui
    locator/executefilter.cpp locator/executefilter.hpp
    locator/externaltoolsfilter.cpp locator/externaltoolsfilter.hpp
    locator/filesystemfilter.cpp locator/filesystemfilter.hpp locator/filesystemfilter.ui
    locator/ilocatorfilter.cpp locator/ilocatorfilter.hpp
    locator/javascriptfilter.cpp locator/javascriptfilter.hpp
    locator/locator.cpp locator/locator.hpp
    locator/locatorconstants.hpp
    locator/locatorfiltersfilter.cpp locator/locatorfiltersfilter.hpp
    locator/locatormanager.cpp locator/locatormanager.hpp
    locator/locatorsearchutils.cpp locator/locatorsearchutils.hpp
    locator/locatorsettingspage.cpp locator/locatorsettingspage.hpp locator/locatorsettingspage.ui
    locator/locatorwidget.cpp locator/locatorwidget.hpp
    locator/opendocumentsfilter.cpp locator/opendocumentsfilter.hpp
    locator/spotlightlocatorfilter.hpp locator/spotlightlocatorfilter.cpp
    locator/urllocatorfilter.cpp locator/urllocatorfilter.hpp locator/urllocatorfilter.ui
    loggingviewer.cpp loggingviewer.hpp
    loggingmanager.cpp loggingmanager.hpp
    mainwindow.cpp mainwindow.hpp
    manhattanstyle.cpp manhattanstyle.hpp
    menubarfilter.cpp menubarfilter.hpp
    messagebox.cpp messagebox.hpp
    messagemanager.cpp messagemanager.hpp
    messageoutputwindow.cpp messageoutputwindow.hpp
    mimetypemagicdialog.cpp mimetypemagicdialog.hpp mimetypemagicdialog.ui
    mimetypesettings.cpp mimetypesettings.hpp
    mimetypesettingspage.ui
    minisplitter.cpp minisplitter.hpp
    modemanager.cpp modemanager.hpp
    navigationsubwidget.cpp navigationsubwidget.hpp
    navigationwidget.cpp navigationwidget.hpp
    opendocumentstreeview.cpp opendocumentstreeview.hpp
    outputpane.cpp outputpane.hpp
    outputpanemanager.cpp outputpanemanager.hpp
    outputwindow.cpp outputwindow.hpp
    patchtool.cpp patchtool.hpp
    plugindialog.cpp plugindialog.hpp
    plugininstallwizard.cpp plugininstallwizard.hpp
    progressmanager/futureprogress.cpp progressmanager/futureprogress.hpp
    progressmanager/progressbar.cpp progressmanager/progressbar.hpp
    progressmanager/progressmanager.cpp progressmanager/progressmanager.hpp progressmanager/progressmanager_p.hpp
    progressmanager/progressview.cpp progressmanager/progressview.hpp
    rightpane.cpp rightpane.hpp
    settingsdatabase.cpp settingsdatabase.hpp
    shellcommand.cpp shellcommand.hpp
    sidebar.cpp sidebar.hpp
    sidebarwidget.cpp sidebarwidget.hpp
    statusbarmanager.cpp statusbarmanager.hpp
    styleanimator.cpp styleanimator.hpp
    systemsettings.cpp systemsettings.hpp systemsettings.ui
    textdocument.cpp textdocument.hpp
    themechooser.cpp themechooser.hpp
    vcsmanager.cpp vcsmanager.hpp
    versiondialog.cpp versiondialog.hpp
    welcomepagehelper.cpp welcomepagehelper.hpp
    windowsupport.cpp windowsupport.hpp
  EXPLICIT_MOC dialogs/filepropertiesdialog.hpp
)

extend_qtc_plugin(Core
  CONDITION ORCA_BUILD_WITH_PLUGINS_TESTS
  SOURCES
    locator/locator_test.cpp
    locator/locatorfiltertest.cpp locator/locatorfiltertest.hpp
    testdatadir.cpp testdatadir.hpp
)

extend_qtc_plugin(Core
  CONDITION WIN32
  SOURCES progressmanager/progressmanager_win.cpp
)

extend_qtc_plugin(Core
  CONDITION APPLE
  DEPENDS ${FWAppKit}
  SOURCES
    progressmanager/progressmanager_mac.mm
)

extend_qtc_plugin(Core
  CONDITION (NOT WIN32) AND (NOT APPLE)
  SOURCES progressmanager/progressmanager_x11.cpp
)

extend_qtc_plugin(Core
  CONDITION BUILD_WITH_CRASHPAD
  DEFINES ENABLE_CRASHPAD
)

if ((NOT WIN32) AND (NOT APPLE))
  # install logo
  foreach(size 16 24 32 48 64 128 256 512)
    install(
      FILES ${IDE_LOGO_PATH}/images/logo/${size}/Orca-orca.png
      DESTINATION share/icons/hicolor/${size}x${size}/apps
    )
  endforeach()
endif()

set(FONTS_BASE "${Orca_SOURCE_DIR}/share/3rdparty/studiofonts/")

qt_add_resources(Core
  CoreWelcomeScreenFonts_rcc
  BASE ${FONTS_BASE}
  PREFIX "studiofonts"
  FILES "${FONTS_BASE}/TitilliumWeb-Regular.ttf"
)
