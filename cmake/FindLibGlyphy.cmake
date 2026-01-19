find_path(LIBGLYPHY_INCLUDE_DIR NAMES glyphy.h HINTS  ${PROJECT_SOURCE_DIR}/deps/glyphy/src)

find_library(LIBGLYPHY_LIBRARY NAMES goban_glyphy.lib libglyphy.a
    HINTS ${PROJECT_SOURCE_DIR}/deps/glyphy/win32/x64/${CMAKE_BUILD_TYPE}
    HINTS ${PROJECT_SOURCE_DIR}/deps/glyphy/win32/${CMAKE_BUILD_TYPE}
    HINTS ${PROJECT_BINARY_DIR}/deps/glyphy/build/src/.libs
)

add_library(libglyphy STATIC IMPORTED)

set_target_properties(libglyphy PROPERTIES
    IMPORTED_LOCATION ${LIBGLYPHY_LIBRARY}
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LibGlyphy DEFAULT_MSG
    LIBGLYPHY_LIBRARY LIBGLYPHY_INCLUDE_DIR)

mark_as_advanced(LIBGLYPHY_INCLUDE_DIR LIBGLYPHY_LIBRARY)

