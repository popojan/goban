# - Try to find libRocket
# Once done this will define
#  LIBROCKET_FOUND - System has libRocket
#  LIBROCKET_INCLUDE_DIRS - The LibRocket include directories
#  LIBROCKET_LIBRARIES - The libraries needed to use LibRocket
#  LIBROCKET_DEFINITIONS - Compiler switches required for using LibRocket

find_path(LIBROCKET_INCLUDE_DIR Rocket/Core.h
        PATH_SUFFIXES Rocket HINTS deps/libRocket/Include)

find_library(LIBROCKET_CORE_LIBRARY NAMES RocketCore
	HINTS deps/libRocket/build/)
find_library(LIBROCKET_CONTROLS_LIBRARY NAMES RocketControls
	HINTS deps/libRocket/build/)
find_library(LIBROCKET_DEBUGGER_LIBRARY NAMES RocketDebugger
	HINTS deps/libRocket/build/)

set(LIBROCKET_LIBRARIES
    ${LIBROCKET_CORE_LIBRARY}
    ${LIBROCKET_CONTROLS_LIBRARY}
    ${LIBROCKET_DEBUGGER_LIBRARY}
)
set(LIBROCKET_INCLUDE_DIRS ${LIBROCKET_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBROCKET_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibRocket DEFAULT_MSG
                                  LIBROCKET_LIBRARIES LIBROCKET_INCLUDE_DIR)

mark_as_advanced(LIBROCKET_INCLUDE_DIR LIBROCKET_LIBRARIES)

