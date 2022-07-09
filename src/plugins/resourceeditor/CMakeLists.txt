add_qtc_plugin(ResourceEditor
  DEPENDS Qt6::Xml
  DEFINES RESOURCE_LIBRARY
  PLUGIN_DEPENDS Core ProjectExplorer
  SOURCES
    qrceditor/qrceditor.cpp qrceditor/qrceditor.hpp qrceditor/qrceditor.ui
    qrceditor/resourcefile.cpp qrceditor/resourcefile_p.hpp
    qrceditor/resourceview.cpp qrceditor/resourceview.hpp
    qrceditor/undocommands.cpp qrceditor/undocommands_p.hpp
    resource_global.hpp
    resourceeditorconstants.hpp
    resourceeditorfactory.cpp resourceeditorfactory.hpp
    resourceeditorplugin.cpp resourceeditorplugin.hpp
    resourceeditorw.cpp resourceeditorw.hpp
    resourcenode.cpp resourcenode.hpp
)
