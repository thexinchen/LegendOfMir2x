@echo off
setlocal

pushd "%~dp0"

set "BUILD_TYPE=Release"
set "BUILD_PRESET=conan-release"
set "BUILD_TARGET="
set "MIR2X_RES_OPTION="
set "USE_GIT_PROXY="

:parse_args
if "%~1"=="" goto :args_parsed

if /I "%~1"=="-h" goto :help
if /I "%~1"=="--help" goto :help

if /I "%~1"=="--proxy" (
    set "USE_GIT_PROXY=1"
    shift /1
    goto :parse_args
)

if /I "%~1"=="Debug" (
    set "BUILD_TYPE=Debug"
    set "BUILD_PRESET=conan-debug"
    shift /1
    goto :parse_args
) else if /I "%~1"=="Release" (
    set "BUILD_TYPE=Release"
    set "BUILD_PRESET=conan-release"
    shift /1
    goto :parse_args
)

if /I "%~1"=="--target" (
    if "%~2"=="" goto :usage
    if defined BUILD_TARGET goto :usage
    set "BUILD_TARGET=%~2"
    shift /1
    shift /1
    goto :parse_args
)

if /I "%~1"=="--mir2x-res" (
    if "%~2"=="" goto :usage
    if defined MIR2X_RES_OPTION goto :usage
    set "MIR2X_RES_OPTION=%~2"
    shift /1
    shift /1
    goto :parse_args
)

goto :usage

:args_parsed
if defined USE_GIT_PROXY call :enable_git_proxy

if defined MIR2X_RES_OPTION set "MIR2X_RES_REPO_PATH=%MIR2X_RES_OPTION%"

if not defined MIR2X_RES_REPO_PATH (
    if exist "%~dp0..\mir2x_res\" set "MIR2X_RES_REPO_PATH=%~dp0..\mir2x_res"
)

if /I "%BUILD_TARGET%"=="zsdbdeploy" if not exist "%MIR2X_RES_REPO_PATH%\" (
    echo Resource directory not found. Set MIR2X_RES_REPO_PATH or place mir2x_res next to this repository.
    popd
    exit /b 2
)

echo [1/3] Installing Conan dependencies for %BUILD_TYPE%...
conan install . ^
    -s:h build_type=%BUILD_TYPE% ^
    -s:h compiler.cppstd=23 ^
    -s:h compiler.runtime_type=%BUILD_TYPE% ^
    -c "tools.cmake:configure_args=['-DCMAKE_POLICY_VERSION_MINIMUM=3.5']" ^
    --build=missing
if errorlevel 1 goto :error
   
echo [2/3] Configuring CMake...
if defined MIR2X_RES_REPO_PATH (
    cmake --fresh --preset conan-default "-DMIR2X_RES_REPO_PATH=%MIR2X_RES_REPO_PATH%"
) else (
    cmake --fresh --preset conan-default
)
if errorlevel 1 goto :error

echo [3/3] Building %BUILD_TYPE%...
if defined BUILD_TARGET (
    cmake --build --preset %BUILD_PRESET% --target "%BUILD_TARGET%" --parallel 10
) else (
    cmake --build --preset %BUILD_PRESET% --parallel 10
)
if errorlevel 1 goto :error

echo Build completed successfully.
popd
exit /b 0

:error
echo Build failed.
popd
exit /b 1

:usage
echo Usage: %~nx0 [Debug^|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]
popd
exit /b 2

:help
echo Usage: %~nx0 [Debug^|Release] [--proxy] [--target TARGET] [--mir2x-res PATH]
echo.
echo Arguments:
echo   Debug^|Release       Build configuration. Default: Release.
echo.
echo Options:
echo   --proxy            Use the current Git proxy configuration.
echo   --target TARGET     Build only the specified CMake target.
echo   --mir2x-res PATH    Set MIR2X_RES_REPO_PATH for CMake configure.
echo   -h, --help          Show this help message.
popd
exit /b 0

:enable_git_proxy
set "GIT_HTTP_PROXY="
set "GIT_HTTPS_PROXY="
for /f "delims=" %%P in ('git config --get http.proxy 2^>nul') do set "GIT_HTTP_PROXY=%%P"
for /f "delims=" %%P in ('git config --get https.proxy 2^>nul') do set "GIT_HTTPS_PROXY=%%P"
set "GIT_CONFIG_COUNT=2"
set "GIT_CONFIG_KEY_0=http.proxy"
set "GIT_CONFIG_VALUE_0=%GIT_HTTP_PROXY%"
set "GIT_CONFIG_KEY_1=https.proxy"
set "GIT_CONFIG_VALUE_1=%GIT_HTTPS_PROXY%"
exit /b 0
