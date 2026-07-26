@echo off
setlocal

set "BUILD_TYPE=Release"
set "AUTO="

:parse_args
if "%~1"=="" goto :args_parsed

if /I "%~1"=="Debug" (
    set "BUILD_TYPE=Debug"
    shift /1
    goto :parse_args
)

if /I "%~1"=="Release" (
    set "BUILD_TYPE=Release"
    shift /1
    goto :parse_args
)

if /I "%~1"=="--auto" (
    if defined AUTO goto :usage
    set "AUTO=1"
    shift /1
    goto :parse_args
)

goto :usage

:args_parsed
set "RUN_DIR=%~dp0build\%BUILD_TYPE%"

if not exist "%RUN_DIR%\server.exe" (
    echo Missing executable: %RUN_DIR%\server.exe
    exit /b 1
)

if not exist "%RUN_DIR%\client.exe" (
    echo Missing executable: %RUN_DIR%\client.exe
    exit /b 1
)

start "mir2x server" /D "%RUN_DIR%" "%RUN_DIR%\server.exe" --auto-launch
timeout /t 1 /nobreak >nul

pushd "%RUN_DIR%"
if defined AUTO (
    client.exe --server-ip=localhost --auto-login=test:123456
) else (
    client.exe
)
set "EXIT_CODE=%ERRORLEVEL%"
popd

exit /b %EXIT_CODE%

:usage
echo Usage: %~nx0 [Debug^|Release] [--auto]
exit /b 2
