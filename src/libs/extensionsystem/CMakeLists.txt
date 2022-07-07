add_qtc_library(ExtensionSystem
  DEPENDS Aggregation Utils
  PUBLIC_DEPENDS Qt6::Core Qt6::Widgets
  SOURCES
    extensionsystem_global.hpp
    invoker.cpp invoker.hpp
    iplugin.cpp iplugin.hpp iplugin_p.hpp
    optionsparser.cpp optionsparser.hpp
    plugindetailsview.cpp plugindetailsview.hpp plugindetailsview.ui
    pluginerroroverview.cpp pluginerroroverview.hpp pluginerroroverview.ui
    pluginerrorview.cpp pluginerrorview.hpp pluginerrorview.ui
    pluginmanager.cpp pluginmanager.hpp pluginmanager_p.hpp
    pluginspec.cpp pluginspec.hpp pluginspec_p.hpp
    pluginview.cpp pluginview.hpp
  EXPLICIT_MOC
    pluginmanager.hpp
    pluginmanager_p.hpp
  SKIP_AUTOMOC pluginmanager.cpp
)

find_package(Qt6 COMPONENTS Test QUIET)

extend_qtc_library(ExtensionSystem
  CONDITION TARGET Qt6::Test
  DEPENDS Qt6::Test
  DEFINES ORCA_BUILD_WITH_PLUGINS_TESTS
)
