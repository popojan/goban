#===================================
# Build script for TheGoban ========
#===================================

project(goban C CXX)

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0074 NEW)

# paths
include(GNUInstallDirs)

set(GOBAN_VERSION_MAJOR 0)
set(GOBAN_VERSION_MINOR 1)
set(GOBAN_VERSION_PATCH 0)
set(GOBAN_VERSION_TWEAK 0)
set(PROJECT_VERSION ${GOBAN_VERSION_MAJOR}.${GOBAN_VERSION_MINOR}.${GOBAN_VERSION_PATCH}.${GOBAN_VERSION_TWEAK})

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
SET(libsndfile_inc_dir "${SOURCE_DIR}/src" "${BINARY_DIR}/src")
SET(libsndfile_lib_dir "${BINARY_DIR}")

ADD_LIBRARY(libsndfile STATIC IMPORTED)

#SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/libsndfile.a)


#SET(LIBSNDFILE_INCLUDE_PATH "${install_dir}/src/project_libsndfile-build/src/")

ExternalProject_Add(project_portaudio
    GIT_REPOSITORY      https://git.assembla.com/portaudio.git
    SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/portaudio
    BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/portaudio/build
    CMAKE_ARGS  -DPA_USE_JACK=0 -DPA_USE_ALSA=1 -DPA_USE_ASIO=0 -DPA_USE_WASAPI=1
    STEP_TARGETS build
    INSTALL_COMMAND     echo Skipping install step for portaudio
)

ExternalProject_Get_Property(project_portaudio SOURCE_DIR)
ExternalProject_Get_Property(project_portaudio BINARY_DIR)
SET(portaudio_lib_dir "${BINARY_DIR}")
SET(portaudio_inc_dir "${SOURCE_DIR}/include")


ADD_LIBRARY(portaudio STATIC IMPORTED)
if(WIN32)
    SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/Release/sndfile.lib)
    SET_PROPERTY(TARGET portaudio PROPERTY IMPORTED_LOCATION "${portaudio_lib_dir}/Release/portaudio_static_x64.lib")
else()
    SET_PROPERTY(TARGET libsndfile PROPERTY IMPORTED_LOCATION ${libsndfile_lib_dir}/libsndfile.a)
    SET_PROPERTY(TARGET portaudio PROPERTY IMPORTED_LOCATION "${portaudio_lib_dir}/libportaudio.a")
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
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -O0 -g")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -s")
endif()

if (WIN32)
  add_definitions(-DWIN32 -D_WINDOWS -D_WIN32_WINNT=0x0A00)
  option(CMAKE_USE_WIN32_THREADS_INIT "using WIN32 threads" ON)
  set(Boost_USE_STATIC_LIBS ON)
  set(GLEW_USE_STATIC_LIBS ON)
  add_definitions(-DROCKET_STATIC_LIB)
else()
  set(OpenGL_GL_PREFERENCE "GLVND")
  set(Boost_USE_STATIC_LIBS OFF)
endif()
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(LibRocket REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Boost 1.64.0 REQUIRED COMPONENTS thread system iostreams filesystem)
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

#===================================
# Setup paths ======================
#===================================

get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()


include_directories(
    ${PROJECT_SOURCE_DIR}/deps/glm
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/shell/include
    ${PROJECT_SOURCE_DIR}/src/glyphy
    ${PROJECT_SOURCE_DIR}/src/sgf/include
    ${PROJECT_SOURCE_DIR}/deps/boost
    ${PROJECT_SOURCE_DIR}/deps/spdlog/include
    ${PROJECT_SOURCE_DIR}/deps/clipp/include
    ${PROJECT_SOURCE_DIR}/src/sound/
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIR}
    ${CURSES_INCLUDE_DIR}
    ${LIBROCKET_INCLUDE_DIR}
    ${LIBGLYPHY_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${libsndfile_inc_dir}
    ${portaudio_inc_dir}
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
    ${PROJECT_SOURCE_DIR}/src/BoardObserver.h
    ${PROJECT_SOURCE_DIR}/src/GameThread.h
    ${PROJECT_SOURCE_DIR}/src/gtpclient.h
    ${PROJECT_SOURCE_DIR}/src/opengl.h
    ${PROJECT_SOURCE_DIR}/src/player.h
    ${PROJECT_SOURCE_DIR}/src/Quaternion.h
    ${PROJECT_SOURCE_DIR}/src/Vector.h
    ${PROJECT_SOURCE_DIR}/src/GobanView.h
    ${PROJECT_SOURCE_DIR}/src/GobanModel.h
    ${PROJECT_SOURCE_DIR}/src/GobanControl.h
    ${PROJECT_SOURCE_DIR}/src/GobanShader.h
    ${PROJECT_SOURCE_DIR}/src/Metrics.h
    ${PROJECT_SOURCE_DIR}/src/GobanOverlay.h
    ${PROJECT_SOURCE_DIR}/src/GameState.h src/Configuration.h)

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
    ${PROJECT_SOURCE_DIR}/src/sound/util.cpp src/Configuration.cpp)



if (WIN32)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /NODEFAULTLIB:libcmt.lib")
  add_executable(goban WIN32 ${GOBAN_SRC_FILES} ${GOBAN_HDR_FILES} ${SHELL_SRC_FILES} ${GLYPHY_SRC_FILES} ${PROJECT_SOURCE_DIR}/src/goban.rc)
  set_property(TARGET goban PROPERTY CXX_STANDARD 17)
  set_property(TARGET goban PROPERTY CXX_STANDARD_REQUIRED ON)
else()
  add_executable(goban ${GOBAN_SRC_FILES} ${GOBAN_HDR_FILES} ${SHELL_SRC_FILES} ${GLYPHY_SRC_FILES})
  set(EXTRA_LIBS rt ${ALSA_LIBRARIES} pthread)
endif()

add_dependencies(goban project_portaudio project_libsndfile)

ADD_DEFINITIONS(-DHAVE_CONFIG_H)

target_link_libraries(goban
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
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
    ${EXTRA_LIBS}
    nlohmann_json::nlohmann_json
)


set(RELEASE_DIR ${PROJECT_SOURCE_DIR}/Release)

file(COPY ${PROJECT_SOURCE_DIR}/data DESTINATION ${RELEASE_DIR})

install(DIRECTORY DESTINATION ${RELEASE_DIR})
install(TARGETS goban
    RUNTIME DESTINATION ${RELEASE_DIR}
    BUNDLE DESTINATION ${RELEASE_DIR})

