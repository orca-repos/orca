add_qtc_plugin(ClassView
  PLUGIN_DEPENDS Core CppEditor ProjectExplorer TextEditor
  SOURCES
    classviewconstants.hpp
    classviewmanager.cpp classviewmanager.hpp
    classviewnavigationwidget.cpp classviewnavigationwidget.hpp
    classviewnavigationwidgetfactory.cpp classviewnavigationwidgetfactory.hpp
    classviewparser.cpp classviewparser.hpp
    classviewparsertreeitem.cpp classviewparsertreeitem.hpp
    classviewplugin.cpp classviewplugin.hpp
    classviewsymbolinformation.cpp classviewsymbolinformation.hpp
    classviewsymbollocation.cpp classviewsymbollocation.hpp
    classviewtreeitemmodel.cpp classviewtreeitemmodel.hpp
    classviewutils.cpp classviewutils.hpp
)
