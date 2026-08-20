@echo off
setlocal EnableExtensions

set "PROJECT_DIR=%~dp0"
if not defined GODOT_BIN set "GODOT_BIN=godot"

if /I "%GODOT_BIN%"=="godot" (
    where godot >nul 2>&1
    if errorlevel 1 (
        echo Godot executable not found. Set GODOT_BIN to godot.exe or its full path.
        exit /b 1
    )
)

if not exist "%PROJECT_DIR%build\world_res" (
    echo World resources not found: %PROJECT_DIR%build\world_res
    echo Run client_godot\build_remote.sh or generate the Godot resources first.
    exit /b 1
)
if not exist "%PROJECT_DIR%build\audio" (
    echo Audio resources not found: %PROJECT_DIR%build\audio
    echo Run client_godot\build_remote.sh or generate the Godot resources first.
    exit /b 1
)

set "MIR2X_WORLD_RES=%PROJECT_DIR%build\world_res"
set "MIR2X_AUDIO_RES=%PROJECT_DIR%build\audio"

"%GODOT_BIN%" --path "%PROJECT_DIR%" %*
exit /b %ERRORLEVEL%
