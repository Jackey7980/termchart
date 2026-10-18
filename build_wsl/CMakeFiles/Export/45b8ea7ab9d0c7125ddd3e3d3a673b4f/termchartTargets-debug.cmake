#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "termchart::termchart" for configuration "Debug"
set_property(TARGET termchart::termchart APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(termchart::termchart PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libtermchart.a"
  )

list(APPEND _cmake_import_check_targets termchart::termchart )
list(APPEND _cmake_import_check_files_for_termchart::termchart "${_IMPORT_PREFIX}/lib/libtermchart.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
