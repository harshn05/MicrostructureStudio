@echo off
setlocal

rem Run from a WinDev MinGW-w64 environment.
rem Requirements: MinGW-w64 GCC, CMake, Ninja, .NET 10 SDK on PATH.

cmake -S . -B build_mingw -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DMS_DOTNET_CONFIGURATION=Release ^
  -DMS_DOTNET_RID=win-x64 ^
  -DMS_DOTNET_SELF_CONTAINED=ON

if errorlevel 1 exit /b 1

cmake --build build_mingw --target publish_dotnet
if errorlevel 1 exit /b 1

echo.
echo Build complete:
echo   build_mingw\dotnet_publish
endlocal
