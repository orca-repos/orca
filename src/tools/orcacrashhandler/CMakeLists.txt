if (LINUX) # Debug build only!
  add_qtc_executable(orcacrashhandler
    DEPENDS Utils Qt6::Widgets
    SOURCES
      backtracecollector.cpp backtracecollector.hpp
      crashhandler.cpp crashhandler.hpp
      crashhandlerdialog.cpp crashhandlerdialog.hpp crashhandlerdialog.ui
      main.cpp
      utils.cpp utils.hpp
  )
endif()
