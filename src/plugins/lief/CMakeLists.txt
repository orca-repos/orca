add_qtc_plugin(LIEF
  DEPENDS LIEF::LIEF
  PLUGIN_DEPENDS Core
  SOURCES
    plugin.cpp
    plugin.hpp
    system/windows/windows.cpp
    system/windows/windows.hpp
)
