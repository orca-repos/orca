add_qtc_plugin(LanguageClient
  PUBLIC_DEPENDS LanguageServerProtocol Qt6::Core
  PLUGIN_DEPENDS ProjectExplorer Core TextEditor
  SOURCES
    client.cpp client.hpp
    diagnosticmanager.cpp diagnosticmanager.hpp
    documentsymbolcache.cpp documentsymbolcache.hpp
    dynamiccapabilities.cpp dynamiccapabilities.hpp
    languageclient.qrc
    languageclientcompletionassist.cpp languageclientcompletionassist.hpp
    languageclientfunctionhint.cpp languageclientfunctionhint.hpp
    languageclientformatter.cpp languageclientformatter.hpp
    languageclienthoverhandler.cpp languageclienthoverhandler.hpp
    languageclientinterface.cpp languageclientinterface.hpp
    languageclientmanager.cpp languageclientmanager.hpp
    languageclientoutline.cpp languageclientoutline.hpp
    languageclientplugin.cpp languageclientplugin.hpp
    languageclientquickfix.cpp languageclientquickfix.hpp
    languageclientsettings.cpp languageclientsettings.hpp
    languageclientsymbolsupport.cpp languageclientsymbolsupport.hpp
    languageclientutils.cpp languageclientutils.hpp
    languageclient_global.hpp
    locatorfilter.cpp locatorfilter.hpp
    lspinspector.cpp lspinspector.hpp
    progressmanager.cpp progressmanager.hpp
    semantichighlightsupport.cpp semantichighlightsupport.hpp
    snippet.cpp snippet.hpp
)
