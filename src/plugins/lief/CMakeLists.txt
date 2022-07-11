add_qtc_plugin(LIEF
  DEPENDS LIEF::LIEF
  PLUGIN_DEPENDS Core ProjectExplorer
  SOURCES
    plugin.cpp plugin.hpp
    new_file_wizard_factory.cpp
    new_project_wizard_factory.cpp
)
