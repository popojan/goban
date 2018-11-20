# - Try to find GnuGO
# Once done this will define
#  GNUGO_FOUND - System has GnuGO
#  GNUGO_INCLUDE_DIRS - The GnuGO include directories
#  GNUGO_LIBRARIES - The libraries needed to use GnuGO
#  GNUGO_DEFINITIONS - Compiler switches required for using GnuGO

find_path(GNUGO_INCLUDE_DIR engine/gnugo.h
          PATH_SUFFIXES engine)

find_library(GNUGO_LIBRARY NAMES engine/engine)

set(GNUGO_LIBRARIES ${GNUGO_LIBRARY}engine/libengine.a ${GNUGO_LIBRARY}patterns/libpatterns.a ${GNUGO_LIBRARY}sgf/libsgf.a ${GNUGO_LIBRARY}utils/libutils.a)
set(GNUGO_INCLUDE_DIRS ${GNUGO_INCLUDE_DIR} ${GNUGO_INCLUDE_DIR}/engine ${GNUGO_INCLUDE_DIR}/sgf ${GNUGO_INCLUDE_DIR}/utils )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GnuGo DEFAULT_MSG
                                  GNUGO_LIBRARY GNUGO_INCLUDE_DIR)

mark_as_advanced(GNUGO_INCLUDE_DIR GNUGO_LIBRARY )

