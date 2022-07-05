add_qtc_library(ExtensionSystem
  DEPENDS Aggregation Utils
  PUBLIC_DEPENDS Qt6::Core Qt6::Widgets
  SOURCES
    extensionsystem_global.h
    invoker.cpp invoker.h
    iplugin.cpp iplugin.h iplugin_p.h
    optionsparser.cpp optionsparser.h
    plugindetailsview.cpp plugindetailsview.h plugindetailsview.ui
    pluginerroroverview.cpp pluginerroroverview.h pluginerroroverview.ui
    pluginerrorview.cpp pluginerrorview.h pluginerrorview.ui
    pluginmanager.cpp pluginmanager.h pluginmanager_p.h
    pluginspec.cpp pluginspec.h pluginspec_p.h
    pluginview.cpp pluginview.h
  EXPLICIT_MOC
    pluginmanager.h
    pluginmanager_p.h
  SKIP_AUTOMOC pluginmanager.cpp
)

find_package(Qt6 COMPONENTS Test QUIET)

extend_qtc_library(ExtensionSystem
  CONDITION TARGET Qt6::Test
  DEPENDS Qt6::Test
  DEFINES ORCA_BUILD_WITH_PLUGINS_TESTS
)
