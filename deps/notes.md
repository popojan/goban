# Tools

PowerShell
Make http://gnuwin32.sourceforge.net/packages/make.htm
CMake https://cmake.org/download/

# PowerScript automation notes

* run powerscript
* `powershell -executionpolicy bypass ./build_dependencies.ps1`
* download a file
* `(new-object System.Net.WebClient).DownloadFile('<url>','<target>')`
* get `msbuild.exe` path
* `(Get-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\MSBuild\ToolsVersions\12.0 -Name MSBuildToolsPath).MSBuildToolsPath`
* `powershell.exe -NoP -NonI -Command "(Get-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\MSBuild\ToolsVersions\12.0 -Name MSBuildToolsPath).MSBuildToolsPath"`
* `powershell.exe -NoP -NonI -Command "(new-object System.Net.WebClient).DownloadFile('https://download.savannah.gnu.org/releases/freetype/ft291.zip', 'ft291.zip')"

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
cd glew\build\vc12\
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" glew_static.vcxproj /t:Build /p:Configuration=Release
```

## GLM

TODO

## Glyphy

TODO

# Build
```
cd build
cmake .. -DLIBROCKET_INCLUDE_DIR=../deps/libRocket/Include ^
	-DGLEW_INCLUDE_DIR=../deps/glew-with-extensions/include ^
	-DGLEW_LIBRARY=../deps/glew-with-extensions/lib/Release/Win32/glew32s ^
	-DLIBGLYPHY_INCLUDE_DIR=../deps/glyphy/src ^
	-DLIBGLYPHY_LIBRARY=../deps/glyphy/win32/Release/glyphy.lib ^
	-DBOOST_ROOT="C:/Program Files/boost_1_55_0" ^
	-DBOOST_INCLUDE_DIRS="C:/Program Files/boost_1_55_0/" ^
	-DFREETYPE_INCLUDE_DIR_freetype2=../deps/freetype-2.9.1/include ^
	-DFREETYPE_LIBRARY=../deps/freetype-2.9.1/objs/Win32/Release/freetype

"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" goban.sln /t:Build /p:Configuration=Release

cd ..
copy deps\libRocket\buildDir\Release\Rocket*.dll .
copy deps\freetype-2.9\objs\Win32\Release\freetype.dll .
copy build\Release\goban.exe .
```
