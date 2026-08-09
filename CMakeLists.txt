cmake_minimum_required(VERSION 3.10)

cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0167 OLD)

project(goban LANGUAGES C CXX VERSION 0.1.2)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Hard pin the language standard. target_compile_features(cxx_std_17) is a *minimum*,
# so a compiler defaulting to C++20 or later (GCC 16 does) satisfies it without emitting
# any -std flag — which breaks std::filesystem::path::u8string() call sites that expect
# the C++17 std::string return type.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# These were previously set via set_property(GLOBAL PROPERTY ...), which sets an unrelated
# global property rather than the variable — so they never reached the compiler.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    # Applied per-target below so that FetchContent dependencies stay quiet.
    set(GOBAN_WARNING_FLAGS -Wall -Wextra)
else()
    set(GOBAN_WARNING_FLAGS "")
endif()

INCLUDE(ExternalProject)
INCLUDE(FetchContent)

function(build_glyphy)
    if(WIN32)
        ExternalProject_Add(project_glyphy
            GIT_REPOSITORY  https://github.com/behdad/glyphy.git
            GIT_TAG 68a5f8d8ae64e0158d776b21581d0ecb70ec8304
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
            INSTALL_COMMAND ""
        )
    else()
        ExternalProject_Add(project_glyphy
            GIT_REPOSITORY  https://github.com/behdad/glyphy.git
            GIT_TAG 68a5f8d8ae64e0158d776b21581d0ecb70ec8304
            SOURCE_DIR    ${PROJECT_SOURCE_DIR}/deps/glyphy
            BINARY_DIR    ${PROJECT_BINARY_DIR}/deps/glyphy/build
            CONFIGURE_COMMAND
            COMMAND ${PROJECT_SOURCE_DIR}/deps/glyphy/autogen.sh
            COMMAND ${PROJECT_SOURCE_DIR}/deps/glyphy/configure --enable-static
            --prefix  ${PROJECT_BINARY_DIR}/deps/glyphy/build
            #BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_SOURCE_DIR}/deps/glyphy/
            UPDATE_COMMAND ""
            INSTALL_COMMAND ""
        )
    endif()
    ExternalProject_Get_Property(project_glyphy SOURCE_DIR)
    ExternalProject_Get_Property(project_glyphy BINARY_DIR)
    SET(glyphy_lib_dir "${BINARY_DIR}" PARENT_SCOPE)
    SET(glyphy_inc_dir "${SOURCE_DIR}/include" PARENT_SCOPE)
endfunction()

function(build_libsgfcplusplus)
    SET(SGFCPLUSPLUS_STATIC_DEFINE 1 PARENT_SCOPE)
    MESSAGE("build type = ${CMAKE_BUILD_TYPE}")
    ExternalProject_Add(project_libsgfcplusplus
            GIT_REPOSITORY  https://github.com/herzbube/libsgfcplusplus.git
            GIT_TAG 2.0.1
            CMAKE_ARGS  ${CL_ARGS}
            BUILD_COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus -DENABLE_TESTS=0
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/deps/libsgfcplusplus --config ${CMAKE_BUILD_TYPE} --parallel 1
            SOURCE_DIR	${CMAKE_SOURCE_DIR}/deps/libsgfcplusplus
            BINARY_DIR	${CMAKE_BINARY_DIR}/deps/libsgfcplusplus
            STEP_TARGETS build
            UPDATE_COMMAND ""
            PATCH_COMMAND git checkout -- .
            COMMAND git -C sgfc checkout -- .
            COMMAND git -c core.autocrlf=false apply --ignore-whitespace ${CMAKE_SOURCE_DIR}/deps/_patches/libsgfcplusplus-static-init-fix.patch
            COMMAND git -C sgfc -c core.autocrlf=false apply --ignore-whitespace ${CMAKE_SOURCE_DIR}/deps/_patches/sgfc-save-buffer-uninitialised-byte.patch
            INSTALL_COMMAND ""
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
endfunction()

function(fetch_glm)
    FetchContent_Declare(glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.1)

    FetchContent_GetProperties(glm)
    if(NOT glm_POPULATED)
        FetchContent_MakeAvailable(glm)
    endif()
endfunction()

function(fetch_spdlog)
    FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.12.0)

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
            GIT_TAG 6.2)

    FetchContent_MakeAvailable(rmlui)

    # Export source dir to parent scope
    set(RMLUI_BACKENDS_DIR "${rmlui_SOURCE_DIR}/Backends" PARENT_SCOPE)
endfunction()

function(fetch_doctest)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)

    FetchContent_Declare(doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.12)

    FetchContent_MakeAvailable(doctest)

    # Needed by the caller to include doctest's CTest integration script.
    set(doctest_SOURCE_DIR "${doctest_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(download_fonts)
    set(FONT_DIR "${PROJECT_SOURCE_DIR}/config/fonts")
    set(FONT_FILE "${FONT_DIR}/NotoSansCJKsc-Regular.otf")
    set(FONT_URL "https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf")

    if(NOT EXISTS "${FONT_FILE}")
        message(STATUS "Downloading Noto Sans CJK SC font...")
        file(DOWNLOAD
            "${FONT_URL}"
            "${FONT_FILE}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(WARNING "Failed to download font: ${DOWNLOAD_STATUS}")
        else()
            message(STATUS "Font downloaded successfully: ${FONT_FILE}")
        endif()
    else()
        message(STATUS "Font already exists: ${FONT_FILE}")
    endif()
endfunction()

function(generate_glad_headers)
    #glad --out-path src/glad --profile="compatibility" --api="gl=3.1" --generator="c" --spec="gl" --extensions="" --reproducible
    #glad --out-path src/glad --api="wgl=1.0" --generator="c" --spec="wgl" --extensions="WGL_ARB_extensions_string,WGL_EXT_extensions_string" --reproducible
    #glad --out-path src/glad --api="glx=1.4" --generator="c" --spec="glx" --extensions="GLX_ARB_create_context,GLX_EXT_visual_info" --reproducible
endfunction()

cmake_language(CALL build_glyphy)
cmake_language(CALL fetch_rmlui)
cmake_language(CALL fetch_glfw)
cmake_language(CALL build_libsgfcplusplus)
cmake_language(CALL fetch_json)
cmake_language(CALL fetch_clipp)
cmake_language(CALL fetch_glm)
cmake_language(CALL fetch_spdlog)
cmake_language(CALL download_fonts)
find_package(Threads)
find_package(OpenGL REQUIRED)

if(WIN32)
    # Windows: use vcpkg for dependencies (CONFIG mode for proper transitive deps)
    find_package(Freetype REQUIRED)
    find_package(Portaudio REQUIRED)
    find_package(SndFile CONFIG REQUIRED)
    set(SNDFILE_TARGET SndFile::sndfile)
elseif(APPLE)
    # macOS: GLFW handles Cocoa, PortAudio handles CoreAudio
    find_package(Freetype REQUIRED)
    find_package(Portaudio REQUIRED)
    find_package(LibSndFile REQUIRED)
    set(SNDFILE_TARGET ${LIBSNDFILE_LIBRARY})
    # macOS frameworks needed for GLFW/PortAudio
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
else()
    # Linux
    find_package(X11 REQUIRED)
    find_package(ALSA REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(Portaudio REQUIRED)
    find_package(LibSndFile REQUIRED)
    set(SNDFILE_TARGET ${LIBSNDFILE_LIBRARY})
endif()

option(BuildPrerequisites "BuildPrerequisites" OFF)
option(GOBAN_BUILD_TESTS "Build goban_core unit tests" ON)

find_package(LibGlyphy)
if(BuildPrerequisites OR NOT LibGlyphy_FOUND)
    message("Build Prerequisites")
    add_custom_target(Rescan ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR} DEPENDS project_glyphy)
else()
    message("Build Goban")
    add_custom_target(Rescan)

    # Generate version header from template
    configure_file(
        ${PROJECT_SOURCE_DIR}/src/version.h.in
        ${PROJECT_BINARY_DIR}/generated/version.h
        @ONLY
    )
    include_directories(${PROJECT_BINARY_DIR}/generated)

    cmake_language(CALL find_libsgfcplusplus)

    if(WIN32)
        # WIN32_LEAN_AND_MEAN prevents windows.h from including winsock.h
        add_definitions(-DWIN32 -D_WINDOWS -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN)
    endif()

    # RmlUi backend files (Platform + Renderer)
    set(RMLUI_BACKEND_SRC_FILES
        ${RMLUI_BACKENDS_DIR}/RmlUi_Platform_GLFW.cpp
        ${RMLUI_BACKENDS_DIR}/RmlUi_Renderer_GL2.cpp
    )

    if(WIN32)
        # Windows: vcpkg dependencies
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    elseif(APPLE)
        # iconv is needed by libsgfcplusplus for SGF encoding conversion
        find_library(ICONV_LIBRARY iconv REQUIRED)
        set(EXTRA_LIBS
                ${COCOA_LIBRARY}
                ${IOKIT_LIBRARY}
                ${COREVIDEO_LIBRARY}
                ${COREAUDIO_LIBRARY}
                ${AUDIOTOOLBOX_LIBRARY}
                ${ICONV_LIBRARY}
                pthread
                ${CMAKE_THREAD_LIBS_INIT}
        )
    else()
        # Linux
        set(EXTRA_LIBS
                rt
                ${ALSA_LIBRARIES}
                pthread
                ${CMAKE_THREAD_LIBS_INIT}
        )
    endif()

    set(GLYPHY_SRC_FILES
        src/glyphy/GlyphyAtlas.cc
        src/glyphy/GlyphyBuffer.cc
        src/glyphy/GlyphyFont.cc
        src/glyphy/GlyphyState.cc
        src/glyphy/GlyphyShader.cc
    )

    # ------------------------------------------------------------------
    # goban_core — game logic with no OpenGL, GLFW or RmlUi rendering
    # dependency. Linkable into unit tests without a graphics context.
    # The only RmlUi contact is Rml::Input::KeyIdentifier, used as the
    # keybinding key type in Configuration.h (a header-only enum).
    # ------------------------------------------------------------------
    set(GOBAN_CORE_HDR_FILES
        src/Board.h
        src/Camera.h
        src/Configuration.h
        src/GameNavigator.h
        src/GameObserver.h
        src/GameRecord.h
        src/GameState.h
        src/GameThread.h
        src/GobanModel.h
        src/Metrics.h
        src/PlayerManager.h
        src/player.h
        src/Quaternion.h
        src/ScenarioRecorder.h
        src/SGF.h
        src/UserSettings.h
        src/Vector.h
        src/UiActions.h
        src/gtpclient.h
    )

    set(GOBAN_CORE_SRC_FILES
        src/Board.cpp
        src/Camera.cpp
        src/Configuration.cpp
        src/GameNavigator.cpp
        src/GameRecord.cpp
        src/GameThread.cpp
        src/GobanModel.cpp
        src/Metrics.cpp
        src/PlayerManager.cpp
        src/player.cpp
        src/Quaternion.cpp
        src/ScenarioRecorder.cpp
        src/UiActions.cpp
        src/UserSettings.cpp
        src/Vector.cpp
        src/gtpclient.cpp
    )

    add_library(goban_core STATIC
        ${GOBAN_CORE_SRC_FILES}
        ${GOBAN_CORE_HDR_FILES}
    )

    target_include_directories(goban_core PUBLIC
            src
            ${glm_SOURCE_DIR}
            ${libsgfcplusplus_inc_dir}
            ${PROJECT_BINARY_DIR}/generated
    )

    add_dependencies(goban_core project_libsgfcplusplus)

    target_link_libraries(goban_core PUBLIC
        libsgfcplusplus
        spdlog
        rmlui
        nlohmann_json::nlohmann_json
        ${CMAKE_THREAD_LIBS_INIT}
    )

    target_compile_options(goban_core PRIVATE ${GOBAN_WARNING_FLAGS})

    # ------------------------------------------------------------------
    # goban — the application: rendering, UI, audio, platform glue
    # ------------------------------------------------------------------
    set(GOBAN_APP_HDR_FILES
        src/AppState.h
        src/ElementGame.h
        src/Event.h
        src/EventHandler.h
        src/EventHandlerFileChooser.h
        src/EventHandlerNewGame.h
        src/EventInstancer.h
        src/EventManager.h
        src/FileChooserDataSource.h
        src/GobanControl.h
        src/GobanOverlay.h
        src/GobanShader.h
        src/GobanView.h
        src/OpenGL.h
        src/ScenarioRunner.h
        src/Shadinclude.hpp
    )

    set(GOBAN_APP_SRC_FILES
        src/AppState.cpp
        src/ElementGame.cpp
        src/Event.cpp
        src/EventHandler.cpp
        src/EventHandlerFileChooser.cpp
        src/EventHandlerNewGame.cpp
        src/EventInstancer.cpp
        src/EventManager.cpp
        src/FileChooserDataSource.cpp
        src/GobanControl.cpp
        src/GobanOverlay.cpp
        src/GobanShader.cpp
        src/GobanView.cpp
        src/ScenarioRunner.cpp
        src/main.cpp
        src/sound/AudioFile.cpp
        src/sound/AudioPlayer.cpp
        src/sound/FileHandler.cpp
        src/sound/StreamHandler.cpp
        src/glad/src/glad.c
    )

    add_executable(${PROJECT_NAME}
        ${GOBAN_APP_SRC_FILES}
        ${GOBAN_APP_HDR_FILES}
        ${GLYPHY_SRC_FILES}
        ${RMLUI_BACKEND_SRC_FILES}
    )

    if(WIN32)
        target_sources(${PROJECT_NAME} PRIVATE src/goban.rc)
        set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
            src/glyphy
            src/sound/
            src/glad/include
            ${RMLUI_BACKENDS_DIR}
            ${OPENGL_INCLUDE_DIRS}
            ${LIBGLYPHY_INCLUDE_DIR}
            ${FREETYPE_INCLUDE_DIRS}
            ${LIBSNDFILE_INCLUDE_DIR}
            ${PORTAUDIO_INCLUDE_DIRS}
    )

    add_dependencies(${PROJECT_NAME}
            project_glyphy
            project_libsgfcplusplus
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H=1)

    target_compile_options(${PROJECT_NAME} PRIVATE ${GOBAN_WARNING_FLAGS})

    target_link_libraries(${PROJECT_NAME}
        goban_core
        ${CMAKE_DL_LIBS}
        ${OPENGL_LIBRARIES}
        libglyphy
        ${FREETYPE_LIBRARIES}
        rmlui
        glfw
        ${PORTAUDIO_LIBRARIES}
        ${SNDFILE_TARGET}
        libsgfcplusplus
        spdlog
        clipp
        nlohmann_json::nlohmann_json
        ${EXTRA_LIBS}
    )

    # ------------------------------------------------------------------
    # goban_tests — unit tests over goban_core (no graphics context)
    # ------------------------------------------------------------------
    if(GOBAN_BUILD_TESTS)
        cmake_language(CALL fetch_doctest)
        enable_testing()

        # Deterministic GTP engine used as a test double. Intentionally does not
        # link goban_core: a mock that shared Board's rules implementation could
        # not catch a bug in it.
        add_executable(mock_gtp_engine tests/mock_gtp_engine.cpp)
        target_compile_options(mock_gtp_engine PRIVATE ${GOBAN_WARNING_FLAGS})

        # CONFIGURE_DEPENDS so newly added test files are picked up without
        # having to edit this file for every new suite.
        file(GLOB GOBAN_TEST_SRC_FILES CONFIGURE_DEPENDS
            ${PROJECT_SOURCE_DIR}/tests/*.cpp
        )
        # The mock engine is a separate executable with its own main().
        list(REMOVE_ITEM GOBAN_TEST_SRC_FILES
            ${PROJECT_SOURCE_DIR}/tests/mock_gtp_engine.cpp
        )

        if(GOBAN_TEST_SRC_FILES)
            add_executable(goban_tests ${GOBAN_TEST_SRC_FILES})
            target_link_libraries(goban_tests PRIVATE goban_core doctest::doctest)
            target_include_directories(goban_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
            target_compile_options(goban_tests PRIVATE ${GOBAN_WARNING_FLAGS})

            add_dependencies(goban_tests mock_gtp_engine)

            target_compile_definitions(goban_tests PRIVATE
                GOBAN_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
                GOBAN_TEST_BIN_DIR="${PROJECT_BINARY_DIR}"
                GOBAN_MOCK_ENGINE="$<TARGET_FILE:mock_gtp_engine>"
            )

            include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
            doctest_discover_tests(goban_tests)
        else()
            message(STATUS "GOBAN_BUILD_TESTS is ON but tests/*.cpp is empty")
        endif()
    endif()

endif()
