set(resource_directories
  themes
)

if (APPLE)
  set(resource_directories ${resource_directories} scripts)
endif()

# copy resource directories during build
qtc_copy_to_builddir(copy_share_to_builddir
  DIRECTORIES ${resource_directories}
  DESTINATION "${IDE_DATA_PATH}"
  CREATE_SUBDIRS
)

# create install rule for resource directories
install(
  DIRECTORY ${resource_directories}
  DESTINATION "${IDE_DATA_PATH}"
  USE_SOURCE_PERMISSIONS
)

# create install rule for resource files
foreach(file ${resource_files})
  get_filename_component(path "${file}" DIRECTORY)
  install(
    FILES "${file}"
    DESTINATION "${IDE_DATA_PATH}/${path}"
  )
endforeach()

add_subdirectory(translations)
