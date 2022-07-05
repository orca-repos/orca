if (LINUX) # Debug build only!
  add_qtc_executable(orcacrashhandler
    DEPENDS Utils Qt6::Widgets
    SOURCES
      backtracecollector.cpp backtracecollector.h
      crashhandler.cpp crashhandler.h
      crashhandlerdialog.cpp crashhandlerdialog.h crashhandlerdialog.ui
      main.cpp
      utils.cpp utils.h
  )
endif()
