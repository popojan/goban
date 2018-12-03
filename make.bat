REM choco install git wget 7zip cmake
REM choco install visualstudio2017buildtools
REM choco install visualstudio2017-workload-vctools
REM #choco install visualstudio2017-workload-universalbuildtools
REM choco install windows-sdk-10
REM choco install innounp

set MSBUILD_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin"
set CMAKE_PATH="C:\Program Files\CMake\bin"
set PATH=%CMAKE_PATH%;%MSBUILD_PATH%;%PATH%
set TOOLSET=v141
set TARGET=10.0.17134.0
REM set TARGET=10.0.17763.0
set STUDIO="Visual Studio 15 2017 Win64"

set PROJECT_DIR=%~dp0
set PROJECT_DIR=%PROJECT_DIR:~0,-1%

REM build dependencies
cd deps
call make.bat

REM build goban
cd %PROJECT_DIR%
mkdir build
cd build
cmake .. -G%STUDIO% -DCMAKE_BUILD_TYPE=Release ^
	-DLIBROCKET_INCLUDE_DIR=../deps/libRocket/Include ^
	-DGLEW_INCLUDE_DIR="../deps/glew-with-extensions/include" ^
	-DGLEW_LIBRARY="../deps/glew-with-extensions/builddir/lib/Release/libglew32" ^
	-DLIBGLYPHY_INCLUDE_DIR=../deps/glyphy/src ^
	-DLIBGLYPHY_LIBRARY="../deps/glyphy/win32/x64/Release/goban_glyphy.lib" ^
	-DBoost_NO_SYSTEM_PATHS=TRUE ^
	-DBoost_NO_BOOST_CMAKE=TRUE ^
	-DBOOST_ROOT:PATHNAME="%PROJECT_DIR%\deps\boost" ^
	-DBOOST_INCLUDEDIR="%PROJECT_DIR%\deps\boost" ^
	-DBOOST_LIBRARYDIR="%PROJECT_DIR%\deps\boost\lib64-msvc-14.1" ^
	-DFREETYPE_INCLUDE_DIRS=../deps/freetype2/include ^
	-DFREETYPE_INCLUDE_DIR_freetype2=../deps/freetype2/include ^
	-DFREETYPE_INCLUDE_DIR_ft2build=../deps/freetype2/include ^
	-DFREETYPE_LIBRARY=../deps/freetype2/build/Release/freetype

MSBuild.exe goban.sln /t:Build /p:Configuration=Release /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%
