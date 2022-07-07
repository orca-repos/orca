add_qtc_library(Utils
  DEPENDS Qt6::Qml Qt6::Xml
  PUBLIC_DEPENDS
    Qt6::Concurrent Qt6::Core Qt6::Network Qt6::Gui Qt6::Widgets Qt6::Core5Compat
  SOURCES
    ../3rdparty/optional/optional.hpp
    ../3rdparty/variant/variant.hpp
    ../3rdparty/span/span.hpp
    QtConcurrentTools
    algorithm.hpp
    ansiescapecodehandler.cpp ansiescapecodehandler.hpp
    appmainwindow.cpp appmainwindow.hpp
    archive.cpp archive.hpp
    aspects.cpp aspects.hpp
    basetreeview.cpp basetreeview.hpp
    benchmarker.cpp benchmarker.hpp
    buildablehelperlibrary.cpp buildablehelperlibrary.hpp
    camelcasecursor.cpp camelcasecursor.hpp
    categorysortfiltermodel.cpp categorysortfiltermodel.hpp
    changeset.cpp changeset.hpp
    checkablemessagebox.cpp checkablemessagebox.hpp
    classnamevalidatinglineedit.cpp classnamevalidatinglineedit.hpp
    codegeneration.cpp codegeneration.hpp
    commandline.cpp commandline.hpp
    completinglineedit.cpp completinglineedit.hpp
    completingtextedit.cpp completingtextedit.hpp
    cpplanguage_details.hpp
    crumblepath.cpp crumblepath.hpp
    declarationmacros.hpp
    delegates.cpp delegates.hpp
    detailsbutton.cpp detailsbutton.hpp
    detailswidget.cpp detailswidget.hpp
    differ.cpp differ.hpp
    displayname.cpp displayname.hpp
    dropsupport.cpp dropsupport.hpp
    elfreader.cpp elfreader.hpp
    elidinglabel.cpp elidinglabel.hpp
    environment.cpp environment.hpp
    environmentdialog.cpp environmentdialog.hpp
    environmentfwd.hpp
    environmentmodel.cpp environmentmodel.hpp
    execmenu.cpp execmenu.hpp
    executeondestruction.hpp
    fadingindicator.cpp fadingindicator.hpp
    faketooltip.cpp faketooltip.hpp
    fancylineedit.cpp fancylineedit.hpp
    fancymainwindow.cpp fancymainwindow.hpp
    filecrumblabel.cpp filecrumblabel.hpp
    fileinprojectfinder.cpp fileinprojectfinder.hpp
    filenamevalidatinglineedit.cpp filenamevalidatinglineedit.hpp
    filepath.cpp filepath.hpp
    filesearch.cpp filesearch.hpp
    filesystemmodel.cpp filesystemmodel.hpp
    filesystemwatcher.cpp filesystemwatcher.hpp
    fileutils.cpp fileutils.hpp
    filewizardpage.cpp filewizardpage.hpp
    fixedsizeclicklabel.cpp fixedsizeclicklabel.hpp
    flowlayout.cpp flowlayout.hpp
    functiontraits.hpp
    futuresynchronizer.cpp futuresynchronizer.hpp
    fuzzymatcher.cpp fuzzymatcher.hpp
    genericconstants.hpp
    globalfilechangeblocker.cpp globalfilechangeblocker.hpp
    guard.cpp guard.hpp
    headerviewstretcher.cpp headerviewstretcher.hpp
    highlightingitemdelegate.cpp highlightingitemdelegate.hpp
    historycompleter.cpp historycompleter.hpp
    hostosinfo.cpp hostosinfo.hpp
    htmldocextractor.cpp htmldocextractor.hpp
    icon.cpp icon.hpp
    id.cpp id.hpp
    indexedcontainerproxyconstiterator.hpp
    infobar.cpp infobar.hpp
    infolabel.cpp infolabel.hpp
    itemviews.cpp itemviews.hpp
    json.cpp json.hpp
    jsontreeitem.cpp jsontreeitem.hpp
    launcherinterface.cpp launcherinterface.hpp
    launcherpackets.cpp launcherpackets.hpp
    launchersocket.cpp launchersocket.hpp
    layoutbuilder.cpp layoutbuilder.hpp
    linecolumn.cpp linecolumn.hpp
    link.cpp link.hpp
    listmodel.hpp
    listutils.hpp
    macroexpander.cpp macroexpander.hpp
    mapreduce.hpp
    mimetypes/mimedatabase.cpp mimetypes/mimedatabase.hpp mimetypes/mimedatabase_p.hpp
    mimetypes/mimeglobpattern.cpp mimetypes/mimeglobpattern_p.hpp
    mimetypes/mimemagicrule.cpp mimetypes/mimemagicrule_p.hpp
    mimetypes/mimemagicrulematcher.cpp mimetypes/mimemagicrulematcher_p.hpp
    mimetypes/mimeprovider.cpp mimetypes/mimeprovider_p.hpp
    mimetypes/mimetype.cpp mimetypes/mimetype.hpp mimetypes/mimetype_p.hpp
    mimetypes/mimetypeparser.cpp mimetypes/mimetypeparser_p.hpp
    multitextcursor.cpp multitextcursor.hpp
    namevaluedictionary.cpp namevaluedictionary.hpp
    namevaluedictionary.cpp namevaluedictionary.hpp
    namevalueitem.cpp namevalueitem.hpp
    namevalueitem.cpp namevalueitem.hpp
    namevaluemodel.cpp namevaluemodel.hpp
    namevaluemodel.cpp namevaluemodel.hpp
    namevaluesdialog.cpp namevaluesdialog.hpp
    namevaluesdialog.cpp namevaluesdialog.hpp
    namevaluevalidator.cpp namevaluevalidator.hpp
    namevaluevalidator.cpp namevaluevalidator.hpp
    navigationtreeview.cpp navigationtreeview.hpp
    networkaccessmanager.cpp networkaccessmanager.hpp
    optional.hpp
    osspecificaspects.hpp
    outputformat.hpp
    outputformatter.cpp outputformatter.hpp
    overlaywidget.cpp overlaywidget.hpp
    overridecursor.cpp overridecursor.hpp
    parameteraction.cpp parameteraction.hpp
    pathchooser.cpp pathchooser.hpp
    pathlisteditor.cpp pathlisteditor.hpp
    persistentsettings.cpp persistentsettings.hpp
    pointeralgorithm.hpp
    port.cpp port.hpp
    porting.hpp
    portlist.cpp portlist.hpp
    predicates.hpp
    processhandle.cpp processhandle.hpp
    processreaper.cpp processreaper.hpp
    processutils.cpp processutils.hpp
    progressindicator.cpp progressindicator.hpp
    projectintropage.cpp projectintropage.hpp projectintropage.ui
    proxyaction.cpp proxyaction.hpp
    proxycredentialsdialog.cpp proxycredentialsdialog.hpp proxycredentialsdialog.ui
    qrcparser.cpp qrcparser.hpp
    qtcassert.cpp qtcassert.hpp
    qtcolorbutton.cpp qtcolorbutton.hpp
    qtcprocess.cpp qtcprocess.hpp
    qtcsettings.cpp qtcsettings.hpp
    reloadpromptutils.cpp reloadpromptutils.hpp
    removefiledialog.cpp removefiledialog.hpp removefiledialog.ui
    runextensions.cpp runextensions.hpp
    savefile.cpp savefile.hpp
    scopedswap.hpp
    set_algorithm.hpp
    settingsaccessor.cpp settingsaccessor.hpp
    settingsselector.cpp settingsselector.hpp
    settingsutils.hpp
    shellcommand.cpp shellcommand.hpp
    shellcommandpage.cpp shellcommandpage.hpp
    singleton.cpp singleton.hpp
    sizedarray.hpp
    smallstring.hpp
    smallstringfwd.hpp
    smallstringio.hpp
    smallstringiterator.hpp
    smallstringlayout.hpp
    smallstringliteral.hpp
    smallstringmemory.hpp
    smallstringvector.hpp
    smallstringview.hpp
    span.hpp
    statuslabel.cpp statuslabel.hpp
    stringutils.cpp stringutils.hpp
    styledbar.cpp styledbar.hpp
    stylehelper.cpp stylehelper.hpp
    templateengine.cpp templateengine.hpp
    temporarydirectory.cpp temporarydirectory.hpp
    temporaryfile.cpp temporaryfile.hpp
    terminalcommand.cpp terminalcommand.hpp
    terminalprocess.cpp terminalprocess_p.hpp
    textfieldcheckbox.cpp textfieldcheckbox.hpp
    textfieldcombobox.cpp textfieldcombobox.hpp
    textfileformat.cpp textfileformat.hpp
    textutils.cpp textutils.hpp
    theme/theme.cpp theme/theme.hpp theme/theme_p.hpp
    threadutils.cpp threadutils.hpp
    tooltip/effects.hpp
    tooltip/tips.cpp tooltip/tips.hpp
    tooltip/tooltip.cpp tooltip/tooltip.hpp
    touchbar/touchbar.hpp
    treemodel.cpp treemodel.hpp
    treeviewcombobox.cpp treeviewcombobox.hpp
    uncommentselection.cpp uncommentselection.hpp
    unixutils.cpp unixutils.hpp
    url.cpp url.hpp
    utils.qrc
    utils_global.hpp
    utilsicons.cpp utilsicons.hpp
    variablechooser.cpp variablechooser.hpp
    variant.hpp
    winutils.cpp winutils.hpp
    wizard.cpp wizard.hpp
    wizardpage.cpp wizardpage.hpp
)

extend_qtc_library(Utils CONDITION WIN32
  SOURCES
    touchbar/touchbar.cpp
  DEPENDS
    user32 iphlpapi ws2_32 shell32 ole32
  DEFINES
    _UNICODE UNICODE
  PUBLIC_DEFINES
    _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS
)

extend_qtc_library(Utils CONDITION APPLE
  SOURCES
    fileutils_mac.mm fileutils_mac.hpp
    processhandle_mac.mm
    theme/theme_mac.mm theme/theme_mac.hpp
    touchbar/touchbar_appdelegate_mac.mm touchbar/touchbar_appdelegate_mac_p.hpp
    touchbar/touchbar_mac.mm touchbar/touchbar_mac_p.hpp
  DEPENDS
    ${FWFoundation} ${FWAppKit}
)

extend_qtc_library(Utils CONDITION UNIX AND NOT APPLE
  SOURCES
    touchbar/touchbar.cpp
)

if (WIN32)
  add_qtc_executable(orca_process_stub
    SOURCES process_stub_win.c
    DEPENDS shell32
    DEFINES _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS
  )

  add_qtc_executable(orca_ctrlc_stub
    DEPENDS user32 shell32
    DEFINES _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS
    SOURCES
      process_ctrlc_stub.cpp
  )
else()
  add_qtc_executable(orca_process_stub SOURCES process_stub_unix.c)
endif()
