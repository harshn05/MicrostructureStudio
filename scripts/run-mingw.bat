@echo off
setlocal
set "APP_DIR=%~dp0..\build_mingw\dotnet_publish"
if not exist "%APP_DIR%\MicrostructureStudio.exe" (
    echo Application not found. Run scripts\build-mingw.bat first.
    exit /b 1
)
start "" "%APP_DIR%\MicrostructureStudio.exe"
endlocal
