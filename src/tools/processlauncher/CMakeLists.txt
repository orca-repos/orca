set(UTILSDIR "${PROJECT_SOURCE_DIR}/src/libs/utils")

add_qtc_executable(orca_processlauncher
  INCLUDES "${UTILSDIR}"
  DEPENDS Qt6::Core Qt6::Network
  DEFINES ORCA_UTILS_STATIC_LIB
  SOURCES
    launcherlogging.cpp
    launcherlogging.hpp
    launchersockethandler.cpp
    launchersockethandler.hpp
    processlauncher-main.cpp
    ${UTILSDIR}/launcherpackets.cpp
    ${UTILSDIR}/launcherpackets.hpp
    ${UTILSDIR}/processreaper.cpp
    ${UTILSDIR}/processreaper.hpp
    ${UTILSDIR}/processutils.cpp
    ${UTILSDIR}/processutils.hpp
    ${UTILSDIR}/qtcassert.cpp
    ${UTILSDIR}/qtcassert.hpp
    ${UTILSDIR}/singleton.cpp
    ${UTILSDIR}/singleton.hpp
    )
