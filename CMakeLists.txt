cmake_minimum_required(VERSION 3.5)

cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0167 OLD)

project(goban LANGUAGES C CXX VERSION  0.0.3.0)

include(GNUInstallDirs)

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR})

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set_property(GLOBAL PROPERTY CMAKE_CXX_FLAGS_RELEASE "-O3 -s")
set_property(GLOBAL PROPERTY CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra")

INCLUDE(ExternalProject)
INCLUDE(FetchContent)

function(build_portaudio)
    ExternalProject_Add(project_portaudio
            GIT_REPOSITORY      https://github.com/PortAudio/portaudio
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/portaudio
            BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/portaudio/build
            CMAKE_ARGS  -DPA_USE_JACK=0 -DPA_USE_ALSA=1 -DPA_USE_ASIO=0 -DPA_USE_WASAPI=1
            GIT_TAG v19.7.0
            STEP_TARGETS build
            INSTALL_COMMAND     echo Skipping install step for portaudio
            )

    ExternalProject_Get_Property(project_portaudio SOURCE_DIR)
    ExternalProject_Get_Property(project_portaudio BINARY_DIR)

    SET(portaudio_lib_dir "${BINARY_DIR}" PARENT_SCOPE)
    SET(portaudio_inc_dir "${SOURCE_DIR}/include" PARENT_SCOPE)

    ADD_LIBRARY(portaudio STATIC IMPORTED)
endfunction()

function(build_libsndfile)
    ExternalProject_Add(project_libsndfile
        GIT_REPOSITORY     https://github.com/erikd/libsndfile.git
        GIT_TAG 1.0.31
        SOURCE_DIR  ${PROJECT_SOURCE_DIR}/deps/libsndfile
        BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/libsndfile/build
        STEP_TARGETS build
        UPDATE_COMMAND ""
        INSTALL_COMMAND  echo Skipping install step for libsndfile
    )

    ExternalProject_Get_Property(project_libsndfile SOURCE_DIR)
    ExternalProject_Get_Property(project_libsndfile BINARY_DIR)
    SET(libsndfile_inc_dir "${SOURCE_DIR}/src" "${SOURCE_DIR}/include" "${BINARY_DIR}/include" PARENT_SCOPE)
    SET(libsndfile_lib_dir "${BINARY_DIR}" PARENT_SCOPE)

    ADD_LIBRARY(sndfile STATIC IMPORTED)
endfunction()

function(build_freetype2)
    ExternalProject_Add(project_freetype2
            GIT_REPOSITORY  https://git.savannah.gnu.org/git/freetype/freetype2.git
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/freetype2
            BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/freetype2/build
            INSTALL_COMMAND  echo Skipping install step for freetype2
    )

    ExternalProject_Get_Property(project_freetype2 SOURCE_DIR)
    ExternalProject_Get_Property(project_freetype2 BINARY_DIR)
    SET(freetype2_lib_dir "${BINARY_DIR}/${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    SET(freetype2_inc_dir "${SOURCE_DIR}/include" PARENT_SCOPE)

    ADD_LIBRARY(freetype STATIC IMPORTED)
endfunction()

function(build_glyphy)
    if(WIN32)
        ExternalProject_Add(project_glyphy
            GIT_REPOSITORY  https://github.com/behdad/glyphy.git
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/glyphy
            BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/glyphy/build
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy
                ${PROJECT_SOURCE_DIR}/deps/_patches/goban_glyphy.vcxproj
                ${PROJECT_SOURCE_DIR}/deps/glyphy/win32/
            BUILD_COMMAND ${CMAKE_VS_MSBUILD_COMMAND} ${PROJECT_SOURCE_DIR}/deps/glyphy/win32/goban_glyphy.vcxproj
                /t:Build /p:Configuration=${CMAKE_BUILD_TYPE}
                /p:PlatformToolset=${CMAKE_VS_PLATFORM_TOOLSET}
                /p:TargetPlatformVersion=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
                /p:Platform=${CMAKE_VS_PLATFORM_NAME}
                /p:AssemblySearchPaths=${PROJECT_SOURCE_DIR}/deps/glyphy/include;
            UPDATE_COMMAND ""
            INSTALL_COMMAND  echo Skipping install step for glyphy
        )
    else()
        ExternalProject_Add(project_glyphy
            GIT_REPOSITORY  https://github.com/behdad/glyphy.git
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/glyphy
            BINARY_DIR    ${PROJECT_BINARY_DIR}/deps/glyphy/build
            CONFIGURE_COMMAND
            COMMAND ${PROJECT_SOURCE_DIR}/deps/glyphy/autogen.sh
            COMMAND ${PROJECT_SOURCE_DIR}/deps/glyphy/configure --enable-static
            --prefix  ${PROJECT_BINARY_DIR}/deps/glyphy/build
            #BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_SOURCE_DIR}/deps/glyphy/
            UPDATE_COMMAND ""
            INSTALL_COMMAND  echo Skipping install step for glyphy
        )
    endif()
    ExternalProject_Get_Property(project_glyphy SOURCE_DIR)
    ExternalProject_Get_Property(project_glyphy BINARY_DIR)
    SET(glyphy_lib_dir "${BINARY_DIR}" PARENT_SCOPE)
    SET(glyphy_inc_dir "${SOURCE_DIR}/include" PARENT_SCOPE)
endfunction()

function(download_boost)
    SET(boost_archive boost_1_68_0-msvc-14.1-64.exe)
    ExternalProject_Add( project_boost
        PREFIX "boost"
        DOWNLOAD_COMMAND
            if not exist ${PROJECT_BINARY_DIR}/boost/src/${boost_archive}
                wget --progress=dot:giga https://dl.bintray.com/boostorg/release/1.68.0/binaries/${boost_archive}
        SOURCE_DIR ${PROJECT_BINARY_DIR}/boost/src/
        BINARY_DIR ${PROJECT_BINARY_DIR}/boost/src/
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND
            COMMAND if not exist ${PROJECT_BINARY_DIR}/boost/src/boost
            innoextract ${PROJECT_BINARY_DIR}/boost/src/${boost_archive} #-I app\\boost
                -I app\\lib64-msvc-14.1\\libboost_system-vc141-mt-x64-1_68.lib
                -I app\\lib64-msvc-14.1\\libboost_system-vc141-mt-gd-x64-1_68.lib
                -I app\\lib64-msvc-14.1\\libboost_filesystem-vc141-mt-x64-1_68.lib
                -I app\\lib64-msvc-14.1\\libboost_filesystem-vc141-mt-gd-x64-1_68.lib
                -I app\\lib64-msvc-14.1\\libboost_iostreams-vc141-mt-x64-1_68.lib
                -I app\\lib64-msvc-14.1\\libboost_iostreams-vc141-mt-gd-x64-1_68.lib
            COMMAND if not exist ${PROJECT_BINARY_DIR}/boost/src/boost ${CMAKE_COMMAND} -E rename ${PROJECT_BINARY_DIR}/boost/src/app ${PROJECT_BINARY_DIR}/boost/src/boost
    )
    SET(boost_lib_dir  ${PROJECT_BINARY_DIR}/boost/src/boost/lib64-msvc-14.1 PARENT_SCOPE)
    ADD_LIBRARY(boost_system STATIC IMPORTED)
    ADD_LIBRARY(boost_filesystem STATIC IMPORTED)
    ADD_LIBRARY(boost_iostreams STATIC IMPORTED)
endfunction()

function(build_librocket)
    ExternalProject_Add(project_librocket
            GIT_REPOSITORY      https://github.com/popojan/libRocket.git
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/libRocket
            BINARY_DIR  ${PROJECT_BINARY_DIR}/deps/libRocket/build
            SOURCE_SUBDIR Build
            CMAKE_ARGS
            -DBUILD_SHARED_LIBS=
            -DFREETYPE_INCLUDE_DIRS=${PROJECT_SOURCE_DIR}/deps/freetype2/include
            -DFREETYPE_LIBRARY=${freetype2_lib_dir}/freetype
            INSTALL_COMMAND     echo Skipping install step for libRocket
            )
    ExternalProject_Get_Property(project_librocket SOURCE_DIR)
    ExternalProject_Get_Property(project_librocket BINARY_DIR)
    SET(librocket_lib_dir "${BINARY_DIR}" PARENT_SCOPE)
    SET(librocket_inc_dir "${SOURCE_DIR}/include" PARENT_SCOPE)

    #ADD_LIBRARY(librocket STATIC IMPORTED)
endfunction()

function(build_libsgfcplusplus)
    SET(SGFCPLUSPLUS_STATIC_DEFINE 1 PARENT_SCOPE)
    MESSAGE("build type = ${CMAKE_BUILD_TYPE}")
    ExternalProject_Add(project_libsgfcplusplus
            GIT_REPOSITORY  https://github.com/herzbube/libsgfcplusplus.git
            GIT_TAG master
            CMAKE_ARGS  ${CL_ARGS}
            BUILD_COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus -DENABLE_TESTS=0
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/deps/libsgfcplusplus --config ${CMAKE_BUILD_TYPE}
            SOURCE_DIR	${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus
            BINARY_DIR	${CMAKE_BINARY_DIR}/deps/libsgfcplusplus
            STEP_TARGETS build
            UPDATE_COMMAND ""
            PATCH_COMMAND git apply ${CMAKE_SOURCE_DIR}/deps/_patches/libsgfcplusplus-static-init-fix.patch || true
            INSTALL_COMMAND     echo Skipping install step for libsgfcplusplus
            )

    ExternalProject_Get_Property(project_libsgfcplusplus SOURCE_DIR)
    ExternalProject_Get_Property(project_libsgfcplusplus BINARY_DIR)
    set(libsgfcplusplus_inc_dir "${SOURCE_DIR}/include" "${BINARY_DIR}/src" PARENT_SCOPE)
    if (WIN32)
        set(libsgfcplusplus_lib_dir "${BINARY_DIR}/src/${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    else()
        set(libsgfcplusplus_lib_dir "${BINARY_DIR}/src" PARENT_SCOPE)
    endif()
endfunction()

function(find_libsgfcplusplus)
    find_library(libsgfcplusplus_DBG
            NAMES libsgfcplusplus_staticd
            HINTS ${libsgfcplusplus_lib_dir})
    find_library(libsgfcplusplus_REL
            NAMES libsgfcplusplus_static
            HINTS ${libsgfcplusplus_lib_dir})
    add_library(libsgfcplusplus STATIC IMPORTED)

    set_target_properties(libsgfcplusplus PROPERTIES
            IMPORTED_LOCATION_DEBUG ${libsgfcplusplus_DBG}
            IMPORTED_LOCATION ${libsgfcplusplus_REL})
endfunction()

function(fetch_json)
    include(FetchContent)

    FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(json)
    FetchContent_GetProperties(json)
    if(NOT json_POPULATED)
        FetchContent_MakeAvailable(json)
    endif()
endfunction()

function(fetch_glm)
    FetchContent_Declare(glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 0.9.9.8)

    FetchContent_GetProperties(glm)
    if(NOT glm_POPULATED)
        FetchContent_MakeAvailable(glm)
    endif()
endfunction()

function(fetch_spdlog)
    FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.6.0)

    FetchContent_GetProperties(spdlog)
    if(NOT spdlog_POPULATED)
        FetchContent_MakeAvailable(spdlog)
    endif()
endfunction()

function(fetch_clipp)
    FetchContent_Declare(clipp
            GIT_REPOSITORY https://github.com/muellan/clipp.git
            GIT_TAG 2c32b2f1f7cc530b1ec1f62c92f698643bb368db)

    FetchContent_GetProperties(clipp)
    if(NOT clipp_POPULATED)
        FetchContent_MakeAvailable(clipp)
    endif()
endfunction()

function(fetch_glfw)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4)

    FetchContent_GetProperties(glfw)
    if(NOT glfw_POPULATED)
        FetchContent_MakeAvailable(glfw)
    endif()
endfunction()

function(fetch_rmlui)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(rmlui
            GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
            GIT_TAG master)  # Using master for GCC 15 fix (PR #766)

    FetchContent_MakeAvailable(rmlui)
endfunction()

function(find_libsndfile)
    find_library(libsndfile_DBG
        NAMES sndfile.lib
        HINTS "${libsndfile_lib_dir}"
        PATH_SUFFIXES Debug
    )
    find_library(libsndfile_REL
        NAMES sndfile.lib
        HINTS "${libsndfile_lib_dir}"
        PATH_SUFFIXES Release
    )
    find_path(SNDFILE_INCLUDE_DIR1 NAMES sndfile.h
        HINTS ${PROJECT_BINARY_DIR}/deps/libsndfile/build/include
    )
    find_path(SNDFILE_INCLUDE_DIR2 NAMES sndfile.hh
        HINTS
        ${PROJECT_SOURCE_DIR}/deps/libsndfile/include
    )
    set(SNDFILE_INCLUDE_DIRS ${SNDFILE_INCLUDE_DIR1} ${SNDFILE_INCLUDE_DIR2} PARENT_SCOPE)
    set_target_properties(sndfile PROPERTIES
        IMPORTED_LOCATION_DEBUG ${libsndfile_DBG}
        IMPORTED_LOCATION ${libsndfile_REL}
    )
endfunction()
function(find_portaudio)
    find_library(portaudio_DBG
        NAMES portaudio_static_x64
        HINTS "${portaudio_lib_dir}/${CMAKE_BUILD_TYPE}"
        PATH_SUFFIXES Debug
    )
    find_library(portaudio_REL
        NAMES portaudio_static_x64
        HINTS "${portaudio_lib_dir}/${CMAKE_BUILD_TYPE}"
        PATH_SUFFIXES Release
    )
    find_path(PORTAUDIO_INCLUDE_DIRS NAMES portaudio.h
        HINTS ${PROJECT_SOURCE_DIR}/deps/portaudio/include
    )
    set_target_properties(portaudio PROPERTIES
        IMPORTED_LOCATION_DEBUG "${portaudio_DBG}"
        IMPORTED_LOCATION "${portaudio_REL}"
    )
endfunction()

function(find_freetype2)
    find_library(freetype_DBG
        NAMES freetyped
        HINTS ${freetype2_lib_dir}
    )
    find_library(freetype_REL
        NAMES freetype
        HINTS ${freetype2_lib_dir}
    )
    find_path(FREETYPE_INCLUDE_DIRS NAMES ft2build.h
        HINTS ${PROJECT_SOURCE_DIR}/deps/freetype2/include
    )
    set_target_properties(freetype PROPERTIES
        IMPORTED_LOCATION_DEBUG ${freetype_DBG}
        IMPORTED_LOCATION ${freetype_REL}
    )
endfunction()

function(find_boost)
    SET(boost_lib_dir  ${PROJECT_BINARY_DIR}/boost/src/boost/lib64-msvc-14.1)
    find_library(boost_system_DBG NAMES libboost_system-vc141-mt-gd-x64-1_68 HINTS ${boost_lib_dir})
    find_library(boost_system_REL NAMES libboost_system-vc141-mt-x64-1_68    HINTS ${boost_lib_dir})
    find_library(boost_filesystem_DBG NAMES libboost_filesystem-vc141-mt-gd-x64-1_68 HINTS ${boost_lib_dir})
    find_library(boost_filesystem_REL NAMES libboost_filesystem-vc141-mt-x64-1_68    HINTS ${boost_lib_dir})
    find_library(boost_iostreams_DBG NAMES libboost_iostreams-vc141-mt-gd-x64-1_68 HINTS ${boost_lib_dir})
    find_library(boost_iostreams_REL NAMES libboost_iostreams-vc141-mt-x64-1_68    HINTS ${boost_lib_dir})
    set_target_properties(boost_system PROPERTIES
            IMPORTED_LOCATION_DEBUG ${boost_system_DBG}
            IMPORTED_LOCATION ${boost_system_REL})
    set_target_properties(boost_filesystem PROPERTIES
            IMPORTED_LOCATION_DEBUG ${boost_filesystem_DBG}
            IMPORTED_LOCATION ${boost_filesystem_REL})
    set_target_properties(boost_iostreams PROPERTIES
            IMPORTED_LOCATION_DEBUG ${boost_iostreams_DBG}
            IMPORTED_LOCATION ${boost_iostreams_REL})
endfunction()

function(generate_glad_headers)
    #glad --out-path src/glad --profile="compatibility" --api="gl=3.1" --generator="c" --spec="gl" --extensions="" --reproducible
    #glad --out-path src/glad --api="wgl=1.0" --generator="c" --spec="wgl" --extensions="WGL_ARB_extensions_string,WGL_EXT_extensions_string" --reproducible
    #glad --out-path src/glad --api="glx=1.4" --generator="c" --spec="glx" --extensions="GLX_ARB_create_context,GLX_EXT_visual_info" --reproducible
endfunction()

cmake_language(CALL build_glyphy)
# cmake_language(CALL build_librocket)  # Replaced by RmlUi
cmake_language(CALL fetch_rmlui)
cmake_language(CALL fetch_glfw)
cmake_language(CALL build_libsgfcplusplus)
cmake_language(CALL fetch_json)
cmake_language(CALL fetch_clipp)
cmake_language(CALL fetch_glm)
cmake_language(CALL fetch_spdlog)
find_package(Threads)
find_package(OpenGL REQUIRED)
if(WIN32)
    cmake_language(CALL build_libsndfile)
    cmake_language(CALL build_freetype2)
    cmake_language(CALL build_portaudio)
    cmake_language(CALL download_boost)
else()
    set(Boost_USE_STATIC_LIBS OFF)
    find_package(Boost 1.64.0 REQUIRED COMPONENTS system iostreams filesystem)
    find_package(Curses REQUIRED)
    find_package(X11 REQUIRED)
    find_package(ALSA REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(Portaudio REQUIRED)
    find_package(LibSndFile REQUIRED)
endif()

option(BuildPrerequisites "BuildPrerequisites" OFF)

find_package(LibGlyphy)
if(BuildPrerequisites OR NOT LibGlyphy_FOUND)
    message("Build Prerequisites")
    add_custom_target(Rescan ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR} DEPENDS project_glyphy)
else()
    message("Build Goban")
    add_custom_target(Rescan)

    # find_package(LibRocket REQUIRED)  # Replaced by RmlUi
    cmake_language(CALL find_libsgfcplusplus)

    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)

    add_executable(${PROJECT_NAME}
            src/FileChooserDataSource.cpp)

    if(WIN32)
        cmake_language(CALL find_libsndfile)
        cmake_language(CALL find_portaudio)
        cmake_language(CALL find_freetype2)
        cmake_language(CALL find_boost)

        add_definitions(-DWIN32 -D_WINDOWS -D_WIN32_WINNT=0x0A00)
        #option(CMAKE_USE_WIN32_THREADS_INIT "using WIN32 threads" ON)
        set(Boost_USE_STATIC_LIBS ON)
        # add_definitions(-DROCKET_STATIC_LIB)  # Not needed for RmlUi
    endif()

    set(SHELL_SRC_FILES_COMMON
        src/shell/src/Input.cpp
        src/shell/src/Shell.cpp
        src/shell/src/ShellFileInterface.cpp
        src/shell/src/ShellRenderInterfaceOpenGL.cpp
        src/shell/src/ShellSystemInterface.cpp
    )
    if (WIN32)
        set(PLATFORM_DEPENDENT_SRC_FILES
            src/shell/src/Win32
            src/shell/src/Win32/ShellRenderInterfaceExtensionsOpenGL_Win32.cpp
            src/shell/src/Win32/InputWin32.cpp
            src/shell/src/Win32/ShellWin32.cpp
            src/goban.rc
            src/glad/src/glad_wgl.c
        )
        add_dependencies(${PROJECT_NAME}
                project_boost
                project_freetype2
                project_portaudio
                project_libsndfile
                )
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:libcmtd.lib")
        set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    else()
        set(PLATFORM_DEPENDENT_SRC_FILES
            src/shell/src/x11
            src/shell/src/x11/ShellRenderInterfaceExtensionsOpenGL_X11.cpp
            src/shell/src/x11/InputX11.cpp
            src/shell/src/x11/ShellX11.cpp
            src/glad/src/glad_glx.c
        )
        set(EXTRA_LIBS
                rt
                ${ALSA_LIBRARIES}
                pthread
                ${CMAKE_THREAD_LIBS_INIT}
                ${X11_LIBRARIES}
        )
    endif()

    set(GLYPHY_SRC_FILES
        src/glyphy/GlyphyAtlas.cc
        src/glyphy/GlyphyBuffer.cc
        src/glyphy/GlyphyFont.cc
        src/glyphy/GlyphyState.cc
        src/glyphy/GlyphyShader.cc
    )

    set(GOBAN_HDR_FILES
        src/Board.h
        src/Camera.h
        src/ElementGame.h
        src/Event.h
        src/EventHandler.h
        src/EventHandlerNewGame.h
        src/EventHandlerFileChooser.h
        src/EventInstancer.h
        src/EventManager.h
        src/GameThread.h
        src/GameObserver.h
        src/GameThread.h
        src/gtpclient.h
        src/player.h
        src/Quaternion.h
        src/Vector.h
        src/GobanView.h
        src/GobanModel.h
        src/GobanControl.h
        src/Shadinclude.hpp
        src/GobanShader.h
        src/Metrics.h
        src/GobanOverlay.h
        src/GameState.h
        src/Configuration.h
        src/InputThread.h
        src/GameRecord.h
        src/FileChooserDataSource.h
    )

    set(GOBAN_SRC_FILES
        src/Board.cpp
        src/Camera.cpp
        src/ElementGame.cpp
        src/Event.cpp
        src/EventHandler.cpp
        src/EventHandlerNewGame.cpp
        src/EventHandlerFileChooser.cpp
        src/EventInstancer.cpp
        src/EventManager.cpp
        src/GameThread.cpp
        src/gtpclient.cpp
        src/main.cpp
        src/player.cpp
        src/Quaternion.cpp
        src/Vector.cpp
        src/GobanView.cpp
        src/GobanModel.cpp
        src/FileChooserDataSource.cpp
        src/GobanControl.cpp
        src/GobanShader.cpp
        src/Metrics.cpp
        src/GobanOverlay.cpp
        src/sound/AudioFile.cpp
        src/sound/AudioPlayer.cpp
        src/sound/FileHandler.cpp
        src/sound/StreamHandler.cpp
        src/sound/util.cpp src/Configuration.cpp
        src/glad/src/glad.c
        src/GameRecord.cpp
    )
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

    target_sources(${PROJECT_NAME} PRIVATE
        ${GOBAN_SRC_FILES}
        ${GOBAN_HDR_FILES}
        ${GLYPHY_SRC_FILES}
        ${SHELL_SRC_FILES_COMMON}
        ${PLATFORM_DEPENDENT_SRC_FILES}
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
            deps/glm
            deps/boost
            src
            src/shell/include
            src/glyphy
            src/sound/
            src/glad/include
            ${OPENGL_INCLUDE_DIRS}
            ${CURSES_INCLUDE_DIR}
            # ${LIBROCKET_INCLUDE_DIR}  # Replaced by RmlUi
            ${LIBGLYPHY_INCLUDE_DIR}
            ${FREETYPE_INCLUDE_DIRS}
            ${SNDFILE_INCLUDE_DIRS}
            ${PORTAUDIO_INCLUDE_DIRS}
            ${libsgfcplusplus_inc_dir}
    )

    add_dependencies(${PROJECT_NAME}
            # project_librocket  # Replaced by RmlUi
            project_glyphy
            project_libsgfcplusplus
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H=1)

    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_DL_LIBS}
        ${OPENGL_LIBRARIES}
        boost_system
        boost_iostreams
        boost_filesystem
        libglyphy
        freetype
        # LibRocket_Core      # Replaced by RmlUi
        # LibRocket_Controls
        # LibRocket_Debugger
        rmlui
        glfw
        portaudio
        sndfile
        libsgfcplusplus
        spdlog
        clipp
        nlohmann_json::nlohmann_json
        ${EXTRA_LIBS}
    )
    set(RELEASE_DIR ${PROJECT_SOURCE_DIR}/publish/Goban)

    file(COPY ${PROJECT_SOURCE_DIR}/data DESTINATION ${RELEASE_DIR})
    file(COPY ${PROJECT_SOURCE_DIR}/engine DESTINATION ${RELEASE_DIR})

    install(DIRECTORY DESTINATION ${RELEASE_DIR})
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${RELEASE_DIR}
        BUNDLE DESTINATION ${RELEASE_DIR})

endif()
