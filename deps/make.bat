REM boost
wget https://dl.bintray.com/boostorg/release/1.65.0/binaries/boost_1_65_0-msvc-14.1-32.exe
innounp -x boost_1_65_0-msvc-14.1-32.exe

REM freetype 2.9.1
wget "https://download.savannah.gnu.org/releases/freetype/ft291.zip"
7z x ft291.zip
MSBuild.exe freetype-2.9.1\builds\windows\vc2010\freetype.vcxproj /t:Build /p:Configuration=Release /p:PlatformToolset=v141

REM libRocket
git clone https://github.com/libRocket/libRocket.git
cd libRocket
mkdir BuildDir
cd BuildDir
cmake ../Build -DFREETYPE_INCLUDE_DIRS=../../freetype-2.9.1/include -DFREETYPE_LIBRARY=../../freetype-2.9.1/objs/Win32/Release/freetype
MSBuild.exe libRocket.sln /t:Build /p:Configuration=Release

cd %PROJECT_DIR%

REM glew
git clone https://github.com/tamaskenez/glew-with-extensions
MSBuild.exe glew-with-extensions\build\vc12\glew_static.vcxproj /t:Build /p:Configuration=Release /p:PlatformToolset=v141

REM glm
git clone https://github.com/g-truc/glm.git
cd glm
mkdir build
cd build
cmake ..
MSBuild.exe glm.sln /t:Build /p:Configuration=Release

cd %PROJECT_DIR%

REM glyphy
git clone https://github.com/behdad/glyphy.git
copy _patches\goban_glyphy.vcxproj glyphy\win32
MSBuild.exe  glyphy\win32\goban_glyphy.vcxproj /t:Build /p:Configuration=Release /p:PlatformToolset=v141
