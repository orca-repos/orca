add_qtc_plugin(LIEF
  DEPENDS LIEF::LIEF
  PLUGIN_DEPENDS Core
  SOURCES
    lief-plugin.cpp
    lief-plugin.hpp
    lief-system-windows.cpp
    lief-system-windows.hpp
)
