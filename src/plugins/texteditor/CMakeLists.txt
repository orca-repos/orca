if(TARGET KSyntaxHighlighting)
  set(KSYNTAXHIGHLIGHTING_TARGET KSyntaxHighlighting)
else()
  set(KSYNTAXHIGHLIGHTING_TARGET KF5::SyntaxHighlighting)
endif()

add_qtc_plugin(TextEditor
  DEPENDS Qt6::Concurrent Qt6::Network Qt6::PrintSupport Qt6::Xml
  PUBLIC_DEPENDS ${KSYNTAXHIGHLIGHTING_TARGET}
  PLUGIN_DEPENDS Core
  SOURCES
    autocompleter.cpp autocompleter.hpp
    basefilefind.cpp basefilefind.hpp
    basehoverhandler.cpp basehoverhandler.hpp
    behaviorsettings.cpp behaviorsettings.hpp
    behaviorsettingspage.cpp behaviorsettingspage.hpp behaviorsettingspage.ui
    behaviorsettingswidget.cpp behaviorsettingswidget.hpp behaviorsettingswidget.ui
    blockrange.hpp
    circularclipboard.cpp circularclipboard.hpp
    circularclipboardassist.cpp circularclipboardassist.hpp
    codeassist/assistenums.hpp
    codeassist/assistinterface.cpp codeassist/assistinterface.hpp
    codeassist/assistproposalitem.cpp codeassist/assistproposalitem.hpp
    codeassist/assistproposaliteminterface.hpp
    codeassist/codeassistant.cpp codeassist/codeassistant.hpp
    codeassist/completionassistprovider.cpp codeassist/completionassistprovider.hpp
    codeassist/documentcontentcompletion.cpp codeassist/documentcontentcompletion.hpp
    codeassist/functionhintproposal.cpp codeassist/functionhintproposal.hpp
    codeassist/functionhintproposalwidget.cpp codeassist/functionhintproposalwidget.hpp
    codeassist/genericproposal.cpp codeassist/genericproposal.hpp
    codeassist/genericproposalmodel.cpp codeassist/genericproposalmodel.hpp
    codeassist/genericproposalwidget.cpp codeassist/genericproposalwidget.hpp
    codeassist/iassistprocessor.cpp codeassist/iassistprocessor.hpp
    codeassist/iassistproposal.cpp codeassist/iassistproposal.hpp
    codeassist/iassistproposalmodel.cpp codeassist/iassistproposalmodel.hpp
    codeassist/iassistproposalwidget.cpp codeassist/iassistproposalwidget.hpp
    codeassist/iassistprovider.cpp codeassist/iassistprovider.hpp
    codeassist/ifunctionhintproposalmodel.cpp codeassist/ifunctionhintproposalmodel.hpp
    codeassist/keywordscompletionassist.cpp codeassist/keywordscompletionassist.hpp
    codeassist/runner.cpp codeassist/runner.hpp
    codeassist/textdocumentmanipulator.cpp codeassist/textdocumentmanipulator.hpp
    codeassist/textdocumentmanipulatorinterface.hpp
    codestyleeditor.cpp codestyleeditor.hpp
    codestylepool.cpp codestylepool.hpp
    codestyleselectorwidget.cpp codestyleselectorwidget.hpp codestyleselectorwidget.ui
    colorpreviewhoverhandler.cpp colorpreviewhoverhandler.hpp
    colorscheme.cpp colorscheme.hpp
    colorschemeedit.cpp colorschemeedit.hpp colorschemeedit.ui
    command.cpp command.hpp
    commentssettings.cpp commentssettings.hpp
    completionsettings.cpp completionsettings.hpp
    completionsettingspage.cpp completionsettingspage.hpp completionsettingspage.ui
    displaysettings.cpp displaysettings.hpp
    displaysettingspage.cpp displaysettingspage.hpp displaysettingspage.ui
    extraencodingsettings.cpp extraencodingsettings.hpp
    findincurrentfile.cpp findincurrentfile.hpp
    findinfiles.cpp findinfiles.hpp
    findinopenfiles.cpp findinopenfiles.hpp
    fontsettings.cpp fontsettings.hpp
    fontsettingspage.cpp fontsettingspage.hpp fontsettingspage.ui
    formatter.hpp
    formattexteditor.cpp formattexteditor.hpp
    highlighter.cpp highlighter.hpp
    highlightersettings.cpp highlightersettings.hpp
    highlightersettingspage.cpp highlightersettingspage.hpp highlightersettingspage.ui
    icodestylepreferences.cpp icodestylepreferences.hpp
    icodestylepreferencesfactory.cpp icodestylepreferencesfactory.hpp
    indenter.hpp
    ioutlinewidget.hpp
    linenumberfilter.cpp linenumberfilter.hpp
    marginsettings.cpp marginsettings.hpp
    outlinefactory.cpp outlinefactory.hpp
    plaintexteditorfactory.cpp plaintexteditorfactory.hpp
    quickfix.cpp quickfix.hpp
    refactoringchanges.cpp refactoringchanges.hpp
    refactoroverlay.cpp refactoroverlay.hpp
    semantichighlighter.cpp semantichighlighter.hpp
    simplecodestylepreferences.cpp simplecodestylepreferences.hpp
    simplecodestylepreferenceswidget.cpp simplecodestylepreferenceswidget.hpp
    snippets/reuse.hpp
    snippets/snippet.cpp snippets/snippet.hpp
    snippets/snippetassistcollector.cpp snippets/snippetassistcollector.hpp
    snippets/snippeteditor.cpp snippets/snippeteditor.hpp
    snippets/snippetoverlay.cpp snippets/snippetoverlay.hpp
    snippets/snippetparser.cpp snippets/snippetparser.hpp
    snippets/snippetprovider.cpp snippets/snippetprovider.hpp
    snippets/snippetscollection.cpp snippets/snippetscollection.hpp
    snippets/snippetssettings.cpp snippets/snippetssettings.hpp
    snippets/snippetssettingspage.cpp snippets/snippetssettingspage.hpp snippets/snippetssettingspage.ui
    storagesettings.cpp storagesettings.hpp
    syntaxhighlighter.cpp syntaxhighlighter.hpp
    tabsettings.cpp tabsettings.hpp
    tabsettingswidget.cpp tabsettingswidget.hpp tabsettingswidget.ui
    textdocument.cpp textdocument.hpp
    textdocumentlayout.cpp textdocumentlayout.hpp
    texteditor.cpp texteditor.hpp
    texteditor.qrc
    texteditor_global.hpp
    texteditor_p.hpp
    texteditoractionhandler.cpp texteditoractionhandler.hpp
    texteditorconstants.cpp texteditorconstants.hpp
    texteditoroverlay.cpp texteditoroverlay.hpp
    texteditorplugin.cpp texteditorplugin.hpp
    texteditorsettings.cpp texteditorsettings.hpp
    textindenter.cpp textindenter.hpp
    textmark.cpp textmark.hpp
    textstyles.hpp
    typingsettings.cpp typingsettings.hpp
)

extend_qtc_plugin(TextEditor
  CONDITION WITH_TESTS
  SOURCES texteditor_test.cpp
)
