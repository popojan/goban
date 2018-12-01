REM freetype 2.9.1
wget --progress=dot:mega "https://download.savannah.gnu.org/releases/freetype/ft291.zip"
7z x ft291.zip
cd freetype-2.9.1
mkdir build
cd build
cmake ..
MSBuild.exe freetype.sln /t:Build /p:Configuration=Release

cd ..\..

REM libRocket
git clone https://github.com/libRocket/libRocket.git
cd libRocket
mkdir BuildDir
cd BuildDir
cmake ../Build -DFREETYPE_INCLUDE_DIRS=../../freetype-2.9.1/include -DFREETYPE_LIBRARY=../../freetype-2.9.1/build/Release/freetype
MSBuild.exe libRocket.sln /t:Build /p:Configuration=Release

cd ..\..

REM glew
git clone https://github.com/tamaskenez/glew-with-extensions
cd glew-with-extensions
mkdir BuildDir
cd BuildDir
cmake ..\build\cmake -UGLEW_USE_STATIC_LIBS
MSBuild.exe glew_s.vcxproj /t:Build /p:Configuration=Release

cd ..\..

REM glm
git clone https://github.com/g-truc/glm.git

REM glyphy
git clone https://github.com/behdad/glyphy.git
copy _patches\goban_glyphy.vcxproj glyphy\win32
MSBuild.exe  glyphy\win32\goban_glyphy.vcxproj /t:Build /p:Configuration=Release /p:PlatformToolset=v141

::/p:WindowsTargetPlatformVersion=10.0.17763.0

REM boost
wget --progress=dot:giga https://dl.bintray.com/boostorg/release/1.65.0/binaries/boost_1_65_0-msvc-14.1-32.exe
innounp -b -x boost_1_65_0-msvc-14.1-32.exe | grep "#[0-9]\+00"
move {app} boost
