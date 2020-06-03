set CLEAN=

REM freetype2
if not exist freetype2 git clone http://git.sv.nongnu.org/r/freetype/freetype2.git
cd freetype2
if defined CLEAN rmdir /s /q build
if not exist build mkdir build
cd build
cmake .. -G%STUDIO%
MSBuild.exe freetype.vcxproj /t:Build /p:Configuration=%BUILD_TYPE%  /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%
cd ..\..

REM libRocket
if not exist libRocket git clone https://github.com/popojan/libRocket.git
cd libRocket
git checkout rem-fix
if defined CLEAN rmdir /s /q BuildDir
if not exist BuildDir mkdir BuildDir
cd BuildDir
cmake -G%STUDIO% ../Build -DBUILD_SHARED_LIBS= -DFREETYPE_INCLUDE_DIRS=../../freetype2/include -DFREETYPE_LIBRARY=../../freetype2/build/Release/freetype
MSBuild.exe libRocket.sln /t:Build /p:Configuration=%BUILD_TYPE% /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET%

cd ..\..

REM glm
if not exist glm git clone https://github.com/g-truc/glm.git

REM glyphy
if not exist glyphy git clone https://github.com/behdad/glyphy.git
copy _patches\goban_glyphy.vcxproj glyphy\win32
set BUILD="Build"
if defined CLEAN set BUILD="Rebuild"
MSBuild.exe  glyphy\win32\goban_glyphy.vcxproj /t:%BUILD% /p:Configuration=%BUILD_TYPE% /p:PlatformToolset=%TOOLSET% /p:TargetPlatformVersion=%TARGET% /p:Platform=x64

REM boost
set FBOOST="boost_1_68_0-msvc-14.1-64.exe"
if not exist boost (
  if not exist %FBOOST% wget --progress=dot:giga https://dl.bintray.com/boostorg/release/1.68.0/binaries/%FBOOST%
  innounp -b -q -x %FBOOST% {app}\boost\*
  innounp -b -x %FBOOST% {app}\lib64-msvc-14.1\libboost_*-vc141-mt-x64-1_68.lib
  innounp -b -x %FBOOST% {app}\lib64-msvc-14.1\libboost_*-vc141-mt-gd-x64-1_68.lib
  move {app} boost
)
