#===================================
# Build script for TheGoban ========
#===================================

cmake_minimum_required(VERSION 3.1)

cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0074 NEW)

# MUST be done before call to 'project'
get_cmake_property(vars CACHE_VARIABLES)
foreach(var ${vars})
    get_property(currentHelpString CACHE "${var}" PROPERTY HELPSTRING)
        if("${currentHelpString}" MATCHES "No help, variable specified on the command line." OR "${currentHelpString}" STREQUAL "")
            # message("${var} = [${${var}}]  --  ${currentHelpString}") # uncomment to see the variables being processed
            list(APPEND CL_ARGS "-D${var}=${${var}}")
         endif()
endforeach()

project(goban C CXX)
project(goban VERSION 0.0.1.0)

# paths
include(GNUInstallDirs)

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR})

INCLUDE(ExternalProject)

ExternalProject_Add(project_libsndfile
    GIT_REPOSITORY     https://github.com/erikd/libsndfile.git
    SOURCE_DIR  ${PROJECT_SOURCE_DIR}/deps/libsndfile
    BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/libsndfile/build
    STEP_TARGETS build
    UPDATE_COMMAND ""
    INSTALL_COMMAND     echo Skipping install step for libsndfile
)

ExternalProject_Get_Property(project_libsndfile SOURCE_DIR)
ExternalProject_Get_Property(project_libsndfile BINARY_DIR)
SET(libsndfile_inc_dir "${SOURCE_DIR}/src" "${BINARY_DIR}/src" "${PROJECT_BINARY_DIR}/deps/")
SET(libsndfile_lib_dir "${BINARY_DIR}")

ADD_LIBRARY(libsndfile STATIC IMPORTED)

#SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/libsndfile.a)


#SET(LIBSNDFILE_INCLUDE_PATH "${install_dir}/src/project_libsndfile-build/src/")

ExternalProject_Add(project_portaudio
    GIT_REPOSITORY      https://git.assembla.com/portaudio.git
    SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/portaudio
    BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/portaudio/build
    CMAKE_ARGS  -DPA_USE_JACK=0 -DPA_USE_ALSA=1 -DPA_USE_ASIO=0 -DPA_USE_WASAPI=1
    GIT_TAG ticket_263_alsa_uninitialized_var_2
    STEP_TARGETS build
    INSTALL_COMMAND     echo Skipping install step for portaudio
)

ExternalProject_Get_Property(project_portaudio SOURCE_DIR)
ExternalProject_Get_Property(project_portaudio BINARY_DIR)
SET(portaudio_lib_dir "${BINARY_DIR}")
SET(portaudio_inc_dir "${SOURCE_DIR}/include")


ADD_LIBRARY(portaudio STATIC IMPORTED)

SET(SGFCPLUSPLUS_STATIC_DEFINE 1)
ExternalProject_Add(project_libsgfcplusplus 
    GIT_REPOSITORY  https://github.com/popojan/libsgfcplusplus.git
    GIT_TAG goban-build
    CMAKE_ARGS  ${CL_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/deps/libsgfcplusplus --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR	${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus
    BINARY_DIR	${CMAKE_BINARY_DIR}/deps/libsgfcplusplus
    STEP_TARGETS build
    UPDATE_COMMAND git restore --source=HEAD --staged --worktree -- sgfc
    COMMAND git fetch
    COMMAND git checkout sgfc
    INSTALL_COMMAND     echo Skipping install step for libsndfile
)

ExternalProject_Get_Property(project_libsgfcplusplus SOURCE_DIR)
ExternalProject_Get_Property(project_libsgfcplusplus BINARY_DIR)
SET(libsgfcplusplus_inc_dir "${SOURCE_DIR}/include" "${BINARY_DIR}/src")
SET(libsgfcplusplus_lib_dir "${BINARY_DIR}/src")

ADD_LIBRARY(libsgfcplusplus STATIC IMPORTED)


if(WIN32)
    IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
        SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION "${libsndfile_lib_dir}/Debug/sndfile.lib")
        SET_PROPERTY(TARGET portaudio PROPERTY IMPORTED_LOCATION "${portaudio_lib_dir}/Debug/portaudio_static_x64.lib")
        SET_PROPERTY(TARGET libsgfcplusplus PROPERTY IMPORTED_LOCATION "${libsgfcplusplus_lib_dir}/Debug/libsgfcplusplus_staticd.lib")
    ELSE()
        SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/Release/sndfile.lib)
        SET_PROPERTY(TARGET portaudio PROPERTY IMPORTED_LOCATION "${portaudio_lib_dir}/Release/portaudio_static_x64.lib")
        SET_PROPERTY(TARGET libsgfcplusplus PROPERTY IMPORTED_LOCATION "${libsgfcplusplus_lib_dir}/Release/libsgfcplusplus_static.lib")
  ENDIF()
else()
    SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/libsndfile.a)
    SET_PROPERTY(TARGET portaudio PROPERTY IMPORTED_LOCATION "${portaudio_lib_dir}/libportaudio.a")
    SET_PROPERTY(TARGET libsgfcplusplus PROPERTY IMPORTED_LOCATION "${libsgfcplusplus_lib_dir}/liblibsgfcplusplus_static.a")
endif()
# Search in the 'cmake' directory for additional CMake modules.
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#===================================
# Build options ====================
#===================================

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

if(NOT WIN32)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-builtin")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0 -g")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -s")
endif()

if (WIN32)
  add_definitions(-DWIN32 -D_WINDOWS -D_WIN32_WINNT=0x0A00)
  option(CMAKE_USE_WIN32_THREADS_INIT "using WIN32 threads" ON)
  set(Boost_USE_STATIC_LIBS ON)
  add_definitions(-DROCKET_STATIC_LIB)
else()
  set(OpenGL_GL_PREFERENCE "GLVND")
  set(Boost_USE_STATIC_LIBS OFF)
endif()
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(LibRocket REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Boost 1.64.0 REQUIRED COMPONENTS system iostreams filesystem)
if (NOT WIN32)
	find_package(Curses REQUIRED)
	find_package(X11 REQUIRED)
	find_package(ALSA REQUIRED)
endif (NOT WIN32)
find_package(LibGlyphy REQUIRED)
find_package(Freetype REQUIRED)


include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY git://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent.git
  GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()



FetchContent_Declare(spdlog
  GIT_REPOSITORY git://github.com/gabime/spdlog.git
  GIT_TAG v1.6.0)

FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  FetchContent_Populate(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(clipp
  GIT_REPOSITORY git://github.com/muellan/clipp.git
  GIT_TAG 2c32b2f1f7cc530b1ec1f62c92f698643bb368db)

FetchContent_GetProperties(clipp)
if(NOT clipp_POPULATED)
  FetchContent_Populate(clipp)
  add_subdirectory(${clipp_SOURCE_DIR} ${clipp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#===================================
# Setup paths ======================
#===================================

get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/deps/glm
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/shell/include
    ${PROJECT_SOURCE_DIR}/src/glyphy
    ${PROJECT_SOURCE_DIR}/src/sgf/include
    ${PROJECT_SOURCE_DIR}/deps/boost
    ${PROJECT_SOURCE_DIR}/src/sound/
    ${PROJECT_SOURCE_DIR}/src/glad/include
    ${OPENGL_INCLUDE_DIRS}
    ${CURSES_INCLUDE_DIR}
    ${LIBROCKET_INCLUDE_DIR}
    ${LIBGLYPHY_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${libsndfile_inc_dir}
    ${portaudio_inc_dir}
    ${libsgfcplusplus_inc_dir}
)

# Include list of source files

if (WIN32)
set(SHELL_SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/shell/src/Input.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/Shell.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/ShellFileInterface.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/ShellRenderInterfaceOpenGL.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/ShellSystemInterface.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/Win32
    ${PROJECT_SOURCE_DIR}/src/shell/src/Win32/ShellRenderInterfaceExtensionsOpenGL_Win32.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/Win32/InputWin32.cpp
    ${PROJECT_SOURCE_DIR}/src/shell/src/Win32/ShellWin32.cpp
)
else()
    set(SHELL_SRC_FILES
        ${PROJECT_SOURCE_DIR}/src/shell/src/Input.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/Shell.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/ShellFileInterface.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/ShellRenderInterfaceOpenGL.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/ShellSystemInterface.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/x11
        ${PROJECT_SOURCE_DIR}/src/shell/src/x11/ShellRenderInterfaceExtensionsOpenGL_X11.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/x11/InputX11.cpp
        ${PROJECT_SOURCE_DIR}/src/shell/src/x11/ShellX11.cpp
    )
endif()

set(GLYPHY_SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/glyphy/GlyphyAtlas.cc
    ${PROJECT_SOURCE_DIR}/src/glyphy/GlyphyBuffer.cc
    ${PROJECT_SOURCE_DIR}/src/glyphy/GlyphyFont.cc
    ${PROJECT_SOURCE_DIR}/src/glyphy/GlyphyState.cc
    ${PROJECT_SOURCE_DIR}/src/glyphy/GlyphyShader.cc
)

set(GOBAN_HDR_FILES
    ${PROJECT_SOURCE_DIR}/src/Board.h
    ${PROJECT_SOURCE_DIR}/src/Camera.h
    ${PROJECT_SOURCE_DIR}/src/Complex.h
    ${PROJECT_SOURCE_DIR}/src/ElementGame.h
    ${PROJECT_SOURCE_DIR}/src/Event.h
    ${PROJECT_SOURCE_DIR}/src/EventHandler.h
    ${PROJECT_SOURCE_DIR}/src/EventHandlerNewGame.h
    ${PROJECT_SOURCE_DIR}/src/EventInstancer.h
    ${PROJECT_SOURCE_DIR}/src/EventManager.h
    ${PROJECT_SOURCE_DIR}/src/GameThread.h
    ${PROJECT_SOURCE_DIR}/src/GameObserver.h
    ${PROJECT_SOURCE_DIR}/src/GameThread.h
    ${PROJECT_SOURCE_DIR}/src/gtpclient.h
    ${PROJECT_SOURCE_DIR}/src/player.h
    ${PROJECT_SOURCE_DIR}/src/Quaternion.h
    ${PROJECT_SOURCE_DIR}/src/Vector.h
    ${PROJECT_SOURCE_DIR}/src/GobanView.h
    ${PROJECT_SOURCE_DIR}/src/GobanModel.h
    ${PROJECT_SOURCE_DIR}/src/GobanControl.h
    ${PROJECT_SOURCE_DIR}/src/Shadinclude.hpp
    ${PROJECT_SOURCE_DIR}/src/GobanShader.h
    ${PROJECT_SOURCE_DIR}/src/Metrics.h
    ${PROJECT_SOURCE_DIR}/src/GobanOverlay.h
    ${PROJECT_SOURCE_DIR}/src/GameState.h
    src/Configuration.h
    src/InputThread.h
    src/GameRecord.h
    ${}
)

set(GOBAN_SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/Board.cpp
    ${PROJECT_SOURCE_DIR}/src/Camera.cpp
    ${PROJECT_SOURCE_DIR}/src/Complex.cpp
    ${PROJECT_SOURCE_DIR}/src/ElementGame.cpp
    ${PROJECT_SOURCE_DIR}/src/Event.cpp
    ${PROJECT_SOURCE_DIR}/src/EventHandler.cpp
    ${PROJECT_SOURCE_DIR}/src/EventHandlerNewGame.cpp
    ${PROJECT_SOURCE_DIR}/src/EventInstancer.cpp
    ${PROJECT_SOURCE_DIR}/src/EventManager.cpp
    ${PROJECT_SOURCE_DIR}/src/GameThread.cpp
    ${PROJECT_SOURCE_DIR}/src/gtpclient.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
    ${PROJECT_SOURCE_DIR}/src/Quaternion.cpp
    ${PROJECT_SOURCE_DIR}/src/Vector.cpp
    ${PROJECT_SOURCE_DIR}/src/GobanView.cpp
    ${PROJECT_SOURCE_DIR}/src/GobanModel.cpp
    ${PROJECT_SOURCE_DIR}/src/GobanControl.cpp
    ${PROJECT_SOURCE_DIR}/src/GobanShader.cpp
    ${PROJECT_SOURCE_DIR}/src/Metrics.cpp
    ${PROJECT_SOURCE_DIR}/src/GameState.cpp
    ${PROJECT_SOURCE_DIR}/src/GobanOverlay.cpp
    ${PROJECT_SOURCE_DIR}/src/sound/AudioFile.cpp
    ${PROJECT_SOURCE_DIR}/src/sound/AudioPlayer.cpp
    ${PROJECT_SOURCE_DIR}/src/sound/FileHandler.cpp
    ${PROJECT_SOURCE_DIR}/src/sound/StreamHandler.cpp
    ${PROJECT_SOURCE_DIR}/src/sound/util.cpp src/Configuration.cpp
    ${PROJECT_SOURCE_DIR}/src/glad/src/glad.c
    src/InputThread.cpp
    src/GameRecord.cpp
)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()


if (MSVC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib")
  add_executable(goban WIN32
    ${GOBAN_SRC_FILES}
    ${GOBAN_HDR_FILES}
    ${SHELL_SRC_FILES}
    ${GLYPHY_SRC_FILES}
    ${PROJECT_SOURCE_DIR}/src/goban.rc
    ${PROJECT_SOURCE_DIR}/src/glad/src/glad_wgl.c
  )
  set_property(TARGET goban PROPERTY CXX_STANDARD 17)
  set_property(TARGET goban PROPERTY CXX_STANDARD_REQUIRED ON)
else()
  add_executable(goban
    ${GOBAN_SRC_FILES}
    ${GOBAN_HDR_FILES}
    ${SHELL_SRC_FILES}
    ${GLYPHY_SRC_FILES}
    ${PROJECT_SOURCE_DIR}/src/glad/src/glad_glx.c
  )
  set(EXTRA_LIBS rt ${ALSA_LIBRARIES} pthread)
endif()

add_dependencies(goban project_portaudio project_libsndfile project_libsgfcplusplus)

ADD_DEFINITIONS(-DHAVE_CONFIG_H)

target_link_libraries(goban
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_IOSTREAMS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${CURSES_LIBRARY}
    ${X11_LIBRARIES}
    ${LIBGLYPHY_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${LIBROCKET_LIBRARIES}
    portaudio
    libsndfile
    libsgfcplusplus
    ${EXTRA_LIBS}
    spdlog
    clipp
    nlohmann_json::nlohmann_json
    ${OPENGL_LIBRARIES}
)

set(RELEASE_DIR ${PROJECT_SOURCE_DIR}/publish/Goban)

file(COPY ${PROJECT_SOURCE_DIR}/data DESTINATION ${RELEASE_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/engine DESTINATION ${RELEASE_DIR})

install(DIRECTORY DESTINATION ${RELEASE_DIR})
install(TARGETS goban
    RUNTIME DESTINATION ${RELEASE_DIR}
    BUNDLE DESTINATION ${RELEASE_DIR})
