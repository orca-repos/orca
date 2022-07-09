add_library(app_version INTERFACE)
target_include_directories(app_version
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  INTERFACE $<INSTALL_INTERFACE:${IDE_HEADER_INSTALL_PATH}/src>)
install(TARGETS app_version EXPORT Orca)

add_subdirectory(app)
add_subdirectory(libs)
add_subdirectory(plugins)
add_subdirectory(shared)
add_subdirectory(tools)
