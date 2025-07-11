# - Try to find libRocket
# Once done this will define
#  LIBROCKET_FOUND - System has libRocket
#  LIBROCKET_INCLUDE_DIRS - The LibRocket include directories
#  LIBROCKET_LIBRARIES - The libraries needed to use LibRocket
#  LIBROCKET_DEFINITIONS - Compiler switches required for using LibRocket

find_path(LIBROCKET_INCLUDE_DIR Rocket/Core.h
        PATH_SUFFIXES Rocket HINTS ${PROJECT_SOURCE_DIR}/deps/libRocket/Include)

message(${PROJECT_BINARY_DIR}/deps/libRocket/build/${CMAKE_BUILD_TYPE})
find_library(LIBROCKET_CORE_LIBRARY NAMES RocketCore
	HINTS ${PROJECT_BINARY_DIR}/deps/libRocket/build ${PROJECT_BINARY_DIR}/deps/libRocket/build/${CMAKE_BUILD_TYPE})
find_library(LIBROCKET_CONTROLS_LIBRARY NAMES RocketControls
	HINTS ${PROJECT_BINARY_DIR}/deps/libRocket/build ${PROJECT_BINARY_DIR}/deps/libRocket/build/${CMAKE_BUILD_TYPE})
find_library(LIBROCKET_DEBUGGER_LIBRARY NAMES RocketDebugger
	HINTS ${PROJECT_BINARY_DIR}/deps/libRocket/build ${PROJECT_BINARY_DIR}/deps/libRocket/build/${CMAKE_BUILD_TYPE})

add_library(LibRocket_Core STATIC IMPORTED)
add_library(LibRocket_Controls STATIC IMPORTED)
add_library(LibRocket_Debugger STATIC IMPORTED)

set_target_properties(LibRocket_Core PROPERTIES IMPORTED_LOCATION ${LIBROCKET_CORE_LIBRARY})
set_target_properties(LibRocket_Controls PROPERTIES IMPORTED_LOCATION ${LIBROCKET_CONTROLS_LIBRARY})
set_target_properties(LibRocket_Debugger PROPERTIES IMPORTED_LOCATION ${LIBROCKET_DEBUGGER_LIBRARY})

set(LIBROCKET_LIBRARIES
    ${LIBROCKET_CORE_LIBRARY}
    ${LIBROCKET_CONTROLS_LIBRARY}
    ${LIBROCKET_DEBUGGER_LIBRARY}
)
set(LIBROCKET_INCLUDE_DIRS ${LIBROCKET_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LibRocket DEFAULT_MSG
                                  LIBROCKET_LIBRARIES LIBROCKET_INCLUDE_DIR)

mark_as_advanced(LIBROCKET_INCLUDE_DIR LIBROCKET_LIBRARIES)
