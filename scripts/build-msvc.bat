@echo off
setlocal

rem Run from an x64 MSVC Developer Command Prompt or WinDev MSVC environment.
rem Requirements: cl.exe, CMake, Ninja, .NET 10 SDK on PATH.

cmake -S . -B build_msvc -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DMS_DOTNET_CONFIGURATION=Release ^
  -DMS_DOTNET_RID=win-x64 ^
  -DMS_DOTNET_SELF_CONTAINED=ON

if errorlevel 1 exit /b 1

cmake --build build_msvc --target publish_dotnet
if errorlevel 1 exit /b 1

echo.
echo Build complete:
echo   build_msvc\dotnet_publish
endlocal
