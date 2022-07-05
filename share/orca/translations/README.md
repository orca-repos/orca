How To add translations to Orca
=====================================

- Read the instructions at http://wiki.qt.io/Qt_Localization

- Add your language to the `set(languages ...` line in `CMakeLists.txt`. Don't
  qualify it with a country unless it is reasonable to expect country-specific
  variants. Skip this step if updating an existing translation.

- Configure Orca build directory with CMake.

- Run `cmake --build . --target ts_<lang>`.

- Start Qt Linguist and translate the strings.

- Create a commit:
  - Discard the modifications to any `.ts` files except yours
  - Create a commit with your file
  - If needed, amend the commit with the modified `CMakeLists.txt` file

- .qm files are generated as part of the regular build.

