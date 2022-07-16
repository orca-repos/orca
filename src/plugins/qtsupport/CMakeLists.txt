add_qtc_plugin(QtSupport
  DEPENDS Qt6::Xml
  PUBLIC_DEPENDS ProParser
  PLUGIN_DEPENDS Core ProjectExplorer ResourceEditor
  SOURCES
    baseqtversion.cpp baseqtversion.hpp
    codegenerator.cpp codegenerator.hpp
    codegensettings.cpp codegensettings.hpp
    codegensettingspage.cpp codegensettingspage.hpp
    codegensettingspagewidget.ui
    exampleslistmodel.cpp exampleslistmodel.hpp
    gettingstartedwelcomepage.cpp gettingstartedwelcomepage.hpp
    profilereader.cpp profilereader.hpp
    qscxmlcgenerator.cpp qscxmlcgenerator.hpp
    qtbuildaspects.cpp qtbuildaspects.hpp
    qtconfigwidget.cpp qtconfigwidget.hpp
    qtcppkitinfo.cpp qtcppkitinfo.hpp
    qtkitinformation.cpp qtkitinformation.hpp
    qtoptionspage.cpp qtoptionspage.hpp
    qtoutputformatter.cpp qtoutputformatter.hpp
    qtparser.cpp qtparser.hpp
    qtprojectimporter.cpp qtprojectimporter.hpp
    qtsupport.qrc
    qtsupport_global.hpp
    qtsupportconstants.hpp
    qtsupportplugin.cpp qtsupportplugin.hpp
    qttestparser.cpp qttestparser.hpp
    qtversionfactory.hpp
    qtversioninfo.ui
    qtversionmanager.cpp qtversionmanager.hpp qtversionmanager.ui
    qtversions.cpp qtversions.hpp
    screenshotcropper.cpp screenshotcropper.hpp
    showbuildlog.ui
    translationwizardpage.cpp translationwizardpage.hpp
    uicgenerator.cpp uicgenerator.hpp
)
