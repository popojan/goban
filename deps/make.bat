set CLEAN=

REM freetype2
if not exist freetype2 git clone http://git.sv.nongnu.org/r/freetype/freetype2.git
cd freetype2
if defined CLEAN rmdir /s /q build
if not exist build mkdir build
cd build
cmake .. -G%STUDIO%
MSBuild.exe freetype.vcxproj /t:Build /p:Configuration=Release  /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%
cd ..\..

REM libRocket
if not exist libRocket git clone https://github.com/libRocket/libRocket.git
cd libRocket
if defined CLEAN rmdir /s /q BuildDir
if not exist BuildDir mkdir BuildDir
cd BuildDir
cmake -G%STUDIO% ../Build -DBUILD_SHARED_LIBS= -DFREETYPE_INCLUDE_DIRS=../../freetype2/include -DFREETYPE_LIBRARY=../../freetype2/build/Release/freetype
MSBuild.exe libRocket.sln /t:Build /p:Configuration=Release /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%

cd ..\..

REM glew
if not exist glew-with-extensions git clone https://github.com/tamaskenez/glew-with-extensions
cd glew-with-extensions
if defined CLEAN rmdir /s /q BuildDir
if not exist BuildDir mkdir BuildDir
cd BuildDir
cmake ..\build\cmake -G%STUDIO% -UGLEW_USE_STATIC_LIBS
MSBuild.exe glew_s.vcxproj /t:Build /p:Configuration=Release /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%

cd ..\..

REM glm
if not exist glm git clone https://github.com/g-truc/glm.git

REM glyphy
if not exist glyphy git clone https://github.com/behdad/glyphy.git
copy _patches\goban_glyphy.vcxproj glyphy\win32
set BUILD="Build"
if defined CLEAN set BUILD="Rebuild"
MSBuild.exe  glyphy\win32\goban_glyphy.vcxproj /t:%BUILD% /p:Configuration=Release /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET% /p:Platform=x64

REM boost
if not exist boost (
  set fboost="boost_1_68_0-msvc-14.1-64.exe"
  if not exist %fboost% wget --progress=dot:giga https://dl.bintray.com/boostorg/release/1.68.0/binaries/%fboost%
  innounp -b -q -x %fboost% {app}\boost\*
  innounp -b -x %fboost% {app}\lib64-msvc-14.1\libboost_*-vc141-mt-x64-1_68.lib 
  move {app} boost
)

REM spdlog
if not exist spdlog git clone --recursive https://github.com/gabime/spdlog.git
