add_subdirectory(orca)
add_subdirectory(3rdparty)

if (NOT APPLE AND NOT WIN32)
  set(DATE_ATTRIBUTE)
  configure_file(metainfo/org.qt-project.orca.appdata.xml.cmakein metainfo/org.qt-project.orca.appdata.xml)

  install(
    DIRECTORY
      applications
    DESTINATION
      ${CMAKE_INSTALL_DATAROOTDIR}
  )
  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/metainfo/org.qt-project.orca.appdata.xml
    DESTINATION
      ${CMAKE_INSTALL_DATAROOTDIR}/metainfo/
  )
endif()
