REM choco install git wget 7zip cmake
REM choco install visualstudio2017buildtools
REM choco install visualstudio2017-workload-vctools
REM #choco install visualstudio2017-workload-universalbuildtools
REM choco install windows-sdk-10
REM choco install innoextract

set MSBUILD_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin"
set CMAKE_PATH="C:\Program Files\CMake\bin"
set PATH=%CMAKE_PATH%;%MSBUILD_PATH%;%PATH%
set STUDIO="Visual Studio 15 2017 Win64"

set PROJECT_DIR=%~dp0
set PROJECT_DIR=%PROJECT_DIR:~0,-1%

if "%DEBUG%" == "1" (
    set BUILD_TYPE=Debug
)
else (
    set BUILD_TYPE=Release
)

cd %PROJECT_DIR%
mkdir %BUILD_DIR%
cd %BUILD_DIR%

cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -G%STUDIO% --config=%BUILD_TYPE% %PROJECT_DIR%

cmake -v --build .

if %ERRORLEVEL% EQU 0 (
  cmake --install .
  cd %PROJECT_DIR%
  echo Success
) else (
  cd %PROJECT_DIR%
  echo Failure %ERRORLEVEL%
  exit /b %ERRORLEVEL%
)
