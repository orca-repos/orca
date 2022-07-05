set(IDE_VERSION "1.0.0")                              # The IDE version.
set(IDE_VERSION_COMPAT "1.0.0")                       # The IDE Compatibility version.
set(IDE_VERSION_DISPLAY "1.0.0")                      # The IDE display version.
set(IDE_COPYRIGHT_YEAR "2022")                        # The IDE current copyright year.
set(IDE_SETTINGSVARIANT "Orca")                  # The IDE settings variation.
set(IDE_DISPLAY_NAME "Orca")                    # The IDE display name.
set(IDE_ID "orca")                               # The IDE id (no spaces, lowercase!)
set(IDE_CASED_ID "Orca")                         # The cased IDE id (no spaces!)
set(IDE_BUNDLE_IDENTIFIER "org.orca-project.${IDE_ID}") # The macOS application bundle identifier.
set(PROJECT_USER_FILE_EXTENSION .user)
set(IDE_DOC_FILE "orca/orca.qdocconf")
set(IDE_DOC_FILE_ONLINE "orca/orca-online.qdocconf")
set(IDE_REVISION FALSE CACHE BOOL "Marks the presence of IDE revision string.")
set(IDE_REVISION_STR "" CACHE STRING "The IDE revision string.")
set(IDE_REVISION_URL "" CACHE STRING "The IDE revision Url string.")

# Absolute, or relative to <orca>/src/app
# Should contain orca.ico, orca.xcassets
set(IDE_ICON_PATH "")
# Absolute, or relative to <orca>/src/plugins/coreplugin
# Should contain images/logo/(16|24|32|48|64|128|256|512)/Orca-orca.png
set(IDE_LOGO_PATH "")

# An advanced variable will not be displayed in any of the cmake GUIs unless 
# the show advanced option is on. In script mode, the advanced/non-advanced
# state has no effect.
mark_as_advanced(IDE_REVISION IDE_REVISION_STR IDE_REVISION_URL)
