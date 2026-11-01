@echo off
setlocal

REM Run from the project root in a WinDev MinGW environment.
REM Ensure cmake, ninja (if selected), dotnet and MinGW are on PATH.

cmake -S . -B build_mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build_mingw --target publish_dotnet
if errorlevel 1 exit /b 1

echo.
echo Build complete.
echo Publish directory: build_mingw\dotnet_publish
endlocal
