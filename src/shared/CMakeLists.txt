add_subdirectory(proparser)
add_subdirectory(qtsingleapplication)
add_subdirectory(qtlockedfile)

# PCHs used by qmake based plugin builds
qtc_add_public_header(orca_pch.h)
qtc_add_public_header(orca_gui_pch.h)

