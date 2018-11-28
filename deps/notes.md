# Tools

PowerShell
CMake https://cmake.org/download/

# PowerScript automation notes

* run powerscript
* `powershell -executionpolicy bypass ./build_dependencies.ps1`
* download a file
* `(new-object System.Net.WebClient).DownloadFile('<url>','<target>')`
* get `msbuild.exe` path
* `(Get-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\MSBuild\ToolsVersions\12.0 -Name MSBuildToolsPath).MSBuildToolsPath`
* `powershell.exe -NoP -NonI -Command "(Get-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\MSBuild\ToolsVersions\12.0 -Name MSBuildToolsPath).MSBuildToolsPath"`
* `powershell.exe -NoP -NonI -Command "(new-object System.Net.WebClient).DownloadFile('https://download.savannah.gnu.org/releases/freetype/ft291.zip', 'ft291.zip')"`
* replace in file `powershell -Command "(gc myFile.txt) -replace 'foo', 'bar' | Out-File myFile.txt"`

# Dependencies
 
## Freetype 2.9.1

https://download.savannah.gnu.org/releases/freetype/ft291.zip
TODO from git: https://github.com/aseprite/freetype2.git

```
cd deps
powershell.exe -NoP -NonI -Command "(new-object System.Net.WebClient).DownloadFile('https://download.savannah.gnu.org/releases/freetype/ft291.zip', 'ft291.zip')"
powershell.exe -NoP -NonI -Command "Expand-Archive '.\ft291.zip' '.\'"
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" ^
	freetype-2.9.1\builds\windows\vc2010\freetype.vcxproj ^
	/t:Build /p:Configuration=Release
```

## libRocket

```
git clone https://github.com/libRocket/libRocket.git
cd libRocket
mkdir BuildDir
cd BuildDir
cmake ../Build  ^
	-DFREETYPE_INCLUDE_DIR_freetype2=../../freetype-2.9.1/include ^
	-DFREETYPE_LIBRARY=../../freetype-2.9.1/objs/Win32/Release/freetype
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" libRocket.sln /t:Build /p:Configuration=Release
```

## glew

```
git clone https://github.com/tamaskenez/glew-with-extensions
copy _patches\glew_static.vcxproj glew-with-extensions\build\vc12\
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" glew-with-extensions\build\vc12\glew_static.vcxproj /t:Build /p:Configuration=Release
```

## GLM

```
git clone https://github.com/g-truc/glm.git 
cd glm
mkdir build
cd build
cmake ..
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" glm.sln /t:Build /p:Configuration=Release
```

## Glyphy

```
git clone https://github.com/behdad/glyphy.git 
copy _patches\goban_glyphy.vcxproj glyphy\win32
copy _patches\glyphy_config.h glyphy\win32\config.h
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" glyphy\win32\goban_glyphy.vcxproj /t:Build /p:Configuration=Release
```

## Boost 1.62

```
git clone --recurse-submodules https://github.com/boostorg/boost.git
cd boost
git checkout boost-1.62.0
git submodule foreach --recursive "git checkout boost-1.62.0 || echo 1"
bootstrap.bat
b2 headers
b2 --build-type=complete --toolset=msvc-12.0 --with-system --with-iostreams --with-thread --with-filesystem --variant=release --link=static --threading=multi --runtime-link=shared stage
```

## Boost Process 0.5

```
git clone https://github.com/BorisSchaeling/boost-process.git
```

# Build
```
cd build
C:/Program Files/boost_1_55_0
cmake .. -DCMAKE_BUILD_TYPE=Release ^
	-DLIBROCKET_INCLUDE_DIR=../deps/libRocket/Include ^
	-DGLEW_INCLUDE_DIR=../deps/glew-with-extensions/include ^
	-DGLEW_LIBRARY=../deps/glew-with-extensions/lib/Release/Win32/glew32s ^
	-DLIBGLYPHY_INCLUDE_DIR=../deps/glyphy/src ^
	-DLIBGLYPHY_LIBRARY=../deps/glyphy/win32/Release/goban_glyphy.lib ^
	-DBoost_NO_SYSTEM_PATHS=TRUE ^
	-DBoost_NO_BOOST_CMAKE=TRUE ^
	-DBOOST_ROOT="V:/Projekty/github/Goban/deps/boost" ^
	-DBOOST_INCLUDE_DIRS="V:/Projekty/github/Goban/deps/boost" ^
	-DFREETYPE_INCLUDE_DIR_freetype2=../deps/freetype-2.9.1/include ^
	-DFREETYPE_LIBRARY=../deps/freetype-2.9.1/objs/Win32/Release/freetype

"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" goban.sln /t:ReBuild /p:Configuration=Release

cd ..
copy deps\libRocket\buildDir\Release\Rocket*.dll .
copy deps\freetype-2.9\objs\Win32\Release\freetype.dll .
copy build\Release\goban.exe .

```
