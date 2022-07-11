cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ORCA_BUILD_WITH_PCH "Build Orca with precompiled headers" ON)
option(ORCA_BUILD_WITH_PLUGINS_TESTS "Build Orca with plugins tests enabled." OFF)
option(ORCA_BUILD_WITH_SEPARATED_DEBUG_INFO "Build Orca with debugging information separated from the binary files." OFF)
option(ORCA_BUILD_PLUGINS_BY_DEFAULT "Build Orca plugins by default. This can be used to build all plugins by default, or none." ON)
option(ORCA_BUILD_EXECUTABLES_BY_DEFAULT "Build Orca executables by default. This can be used to build all executables by default, or none." ON)
option(ORCA_BUILD_LIBRARIES_BY_DEFAULT "Build Orca libraries by default. This can be used to build all libraries by default, or none." ON)

include(FeatureSummary)
include(OrcaIDEBranding RESULT_VARIABLE IDE_BRANDING_FILE)
include(OrcaTranslations)
include(OrcaAPI)

project(Orca VERSION ${IDE_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6
  ${IDE_QT_VERSION_MIN}
  COMPONENTS Concurrent Core Gui Network PrintSupport Qml Sql Widgets Xml Core5Compat ${QT_TEST_COMPONENT}
  REQUIRED
)

add_feature_info("Build tests" ${ORCA_BUILD_WITH_PLUGINS_TESTS} "")
if (ORCA_BUILD_WITH_PLUGINS_TESTS)
  set(QT_TEST_COMPONENT Test)
  set(IMPLICIT_DEPENDS Qt6::Test)
endif()

# LIEF
option(LIEF_DOC "Build LIEF docs" OFF)
option(LIEF_PYTHON_API "Build LIEF Python API" OFF)
option(LIEF_EXAMPLES "Build LIEF examples" OFF)
option(LIEF_TESTS "Build LIEF tests" OFF)
if(MSVC)
  set(LIEF_USE_CRT_RELEASE "MT" CACHE STRING "LIEF CRT option")
  add_link_options(/NODEFAULTLIB:MSVCRT)
endif()
include(FetchContent)
set(LIEF_GIT_URL "https://github.com/lief-project/LIEF.git")
set(LIEF_VERSION 0.12.0)
FetchContent_Declare(LIEF
  GIT_REPOSITORY "${LIEF_GIT_URL}"
  GIT_TAG ${LIEF_VERSION}
)
FetchContent_MakeAvailable(LIEF)

# Crashpad
set(CRASHPAD_BACKEND_URL "" CACHE STRING "Crashpad backend URL")
set(BUILD_WITH_CRASHPAD OFF)
# Linux is not supported for now
# x86_64;arm64 is not supported for now
if(CRASHPAD_BACKEND_URL AND (WIN32 OR APPLE)) # Linux is not supported for now
  find_package(Crashpad QUIET)
  if(TARGET Crashpad::Crashpad)
    set(BUILD_WITH_CRASHPAD ON)
  endif()
endif()
add_feature_info("Build with Crashpad" ${BUILD_WITH_CRASHPAD} "")

if (APPLE)
  find_library(FWCoreFoundation CoreFoundation)
  find_library(FWCoreServices CoreServices)
  find_library(FWFoundation Foundation)
  find_library(FWAppKit AppKit)
  find_library(FWIOKit IOKit)
  find_library(FWSecurity Security)
  find_library(FWSystemConfiguration SystemConfiguration)
  find_library(FWWebKit WebKit)
endif()

if (ORCA_BUILD_WITH_PLUGINS_TESTS)
  enable_testing()
endif()

if (UNIX)
  add_subdirectory(bin)
endif()

add_subdirectory(src)
add_subdirectory(share)

feature_summary(INCLUDE_QUIET_PACKAGES WHAT
  PACKAGES_FOUND PACKAGES_NOT_FOUND
  ENABLED_FEATURES DISABLED_FEATURES
)
