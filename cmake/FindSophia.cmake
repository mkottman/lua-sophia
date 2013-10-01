find_path(SOPHIA_INCLUDE_DIR
  NAMES sophia.h
  PATH_PREFIXES sophia
  PATHS
  /usr/include
  /usr/local/include
  $ENV{LIB_DIR}/include
)

find_library(SOPHIA_LIBRARY
  NAMES sophia
  PATHS
  /usr/lib
  /usr/local/lib
  $ENV{LIB_DIR}/lib
)

set(SOPHIA_LIBRARIES ${SOPHIA_LIBRARY})

# Handle the QUIETLY and REQUIRED arguments and set SOPHIA_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sophia
  DEFAULT_MSG
  SOPHIA_LIBRARY
  SOPHIA_INCLUDE_DIR)

mark_as_advanced(SOPHIA_LIBRARY SOPHIA_INCLUDE_DIR)
