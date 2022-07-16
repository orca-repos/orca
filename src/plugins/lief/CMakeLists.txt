add_qtc_plugin(LIEF
  DEPENDS LIEF::LIEF
  PLUGIN_DEPENDS Core
  SOURCES
    lief-logo.qrc
    lief-plugin.cpp
    lief-plugin.hpp
    lief-wizard-dialog.cpp
    lief-wizard-dialog.hpp
    lief-wizard-windows.cpp
    lief-wizard-windows.hpp
    lief-wizard.cpp
    lief-wizard.hpp
    lief.cpp
    lief.hpp
    lief.ui
)
