@echo off
setlocal enabledelayedexpansion

goto :script_start

:usage
echo[
echo Usage: build_and_install.bat [preset] [-test] [-tools] [-no-configure] [-no-install] [-clean]
echo                              [-hard-clean] [-rebuild] [-hard-rebuild] [-qt ^<ver^|path^>]
echo                              [-no-private-reinject]
echo[
echo   preset              : CMake configure preset name ^(default: ninja^)
echo   -test               : Enable and run unit tests after the Debug build
echo   -tools              : Build and install ProjectCloner + ClonerSource template
echo   -no-configure       : Skip the CMake configure step ^(for fast incremental builds^)
echo   -no-install         : Skip the install steps entirely
echo   -clean              : Run cmake --build clean targets and exit
echo   -hard-clean         : Delete the entire build folder and exit
echo   -rebuild            : Clean then build ^(cmake clean + full build^)
echo   -hard-rebuild       : Delete build folder then configure + build + install from scratch
echo   -qt ^<ver or path^>   : Qt version ^(e.g. 6.10.2^) or full path ^(e.g. C:\Qt\6.10.2\msvc2022_64^)
echo   -no-private-reinject: Disable private-header touch re-injection ^(falls back to
echo                         QCoreApplication::sendEvent; private reinject is ON by default^)
echo   -h / -?             : Print usage info and exit
if defined PRINT_HELP (
  exit /b 0
)

:script_start

REM Set up the MSVC developer environment if not already active
if not defined VSINSTALLDIR (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
    if %errorlevel% neq 0 (
        echo Failed to set up MSVC environment.
        exit /b %errorlevel%
    )
)

set PRESET=ninja
set RUN_TESTS=0
set SKIP_CONFIGURE=0
set SKIP_INSTALL=0
set DO_CLEAN=0
set DO_HARD_CLEAN=0
set DO_REBUILD=0
set DO_HARD_REBUILD=0
set BUILD_TOOLS=0
set PRINT_HELP=0
set QT_PATH=
set NO_PRIVATE_REINJECT=0

:: Parse arguments — accept preset and flags in any order
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="-test"          (set RUN_TESTS=1& shift & goto :parse_args)
if /i "%~1"=="/test"          (set RUN_TESTS=1& shift & goto :parse_args)
if /i "%~1"=="-tools"         (set BUILD_TOOLS=1& shift & goto :parse_args)
if /i "%~1"=="/tools"         (set BUILD_TOOLS=1& shift & goto :parse_args)
if /i "%~1"=="-no-configure"  (set SKIP_CONFIGURE=1& shift & goto :parse_args)
if /i "%~1"=="/no-configure"  (set SKIP_CONFIGURE=1& shift & goto :parse_args)
if /i "%~1"=="-no-install"    (set SKIP_INSTALL=1& shift & goto :parse_args)
if /i "%~1"=="/no-install"    (set SKIP_INSTALL=1& shift & goto :parse_args)
if /i "%~1"=="-clean"         (set DO_CLEAN=1& shift & goto :parse_args)
if /i "%~1"=="/clean"         (set DO_CLEAN=1& shift & goto :parse_args)
if /i "%~1"=="-hard-clean"    (set DO_HARD_CLEAN=1& shift & goto :parse_args)
if /i "%~1"=="/hard-clean"    (set DO_HARD_CLEAN=1& shift & goto :parse_args)
if /i "%~1"=="-rebuild"       (set DO_REBUILD=1& shift & goto :parse_args)
if /i "%~1"=="/rebuild"       (set DO_REBUILD=1& shift & goto :parse_args)
if /i "%~1"=="-hard-rebuild"  (set DO_HARD_REBUILD=1& shift & goto :parse_args)
if /i "%~1"=="/hard-rebuild"  (set DO_HARD_REBUILD=1& shift & goto :parse_args)
if /i "%~1"=="-qt"                  (set QT_PATH=%~2& shift & shift & goto :parse_args)
if /i "%~1"=="/qt"                  (set QT_PATH=%~2& shift & shift & goto :parse_args)
if /i "%~1"=="-no-private-reinject" (set NO_PRIVATE_REINJECT=1& shift & goto :parse_args)
if /i "%~1"=="/no-private-reinject" (set NO_PRIVATE_REINJECT=1& shift & goto :parse_args)
if /i "%~1"=="-h"            (set PRINT_HELP=1& shift & goto :parse_args)
if /i "%~1"=="/h"            (set PRINT_HELP=1& shift & goto :parse_args)
if /i "%~1"=="-?"            (set PRINT_HELP=1& shift & goto :parse_args)
if /i "%~1"=="/?"            (set PRINT_HELP=1& shift & goto :parse_args)
set PRESET=%~1
shift
goto :parse_args
:args_done

if !PRINT_HELP!==1 (
    goto :usage
)

:: --- Check if VCPKG is installed ---
if not defined VCPKG_ROOT (
    powershell -NoProfile -Command "Write-Host 'VCPKG_ROOT is not set. Please make sure VCPKG is properly installed.' -ForegroundColor Red"
    exit /b 1
)
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    powershell -NoProfile -Command "Write-Host 'vcpkg.exe not found in VCPKG_ROOT: %VCPKG_ROOT%' -ForegroundColor Red"
    exit /b 1
)

:: --- Pull the latest VCPKG ---
call :header "Updating VCPKG"
pushd "%VCPKG_ROOT%"
git fetch -q
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Write-Host 'WARNING: git fetch failed for vcpkg - build may fail if baseline is missing.' -ForegroundColor Yellow"
) else (
    git pull --ff-only -q
    if !errorlevel! neq 0 (
        powershell -NoProfile -Command "Write-Host 'WARNING: fast-forward-only git pull failed for vcpkg - build may fail if baseline is missing.' -ForegroundColor Yellow"
    ) else (
        powershell -NoProfile -Command "Write-Host 'vcpkg is up to date.' -ForegroundColor Green"
    )
)
popd

:: --- Resolve Qt path ---
if defined QT_PATH (
    REM If it looks like a version number (no backslash/slash), expand to C:\Qt\<ver>\msvc2022_64
    echo !QT_PATH! | findstr /r "[\\/]" >nul 2>&1
    if !errorlevel! neq 0 (
        set QT_PATH=C:\Qt\!QT_PATH!\msvc2022_64
    )
    if not exist "!QT_PATH!" (
        powershell -NoProfile -Command "Write-Host 'Qt path not found: !QT_PATH!' -ForegroundColor Red"
        exit /b 1
    )
    powershell -NoProfile -Command "Write-Host '    Qt: !QT_PATH!' -ForegroundColor Yellow"
)

:: --- Hard rebuild: delete build folder then continue with full build ---
if !DO_HARD_REBUILD!==1 (
    call :header "Hard Rebuild - Removing build\%PRESET%"
    if exist "build\%PRESET%" (
        rmdir /s /q "build\%PRESET%"
        powershell -NoProfile -Command "Write-Host 'build\%PRESET% removed.' -ForegroundColor Green"
    )
)

:: --- Rebuild: clean then continue with full build ---
if !DO_REBUILD!==1 (
    call :header "Rebuild - Cleaning %PRESET%"
    cmake --build --preset %PRESET%-debug --target clean 2>nul
    cmake --build --preset %PRESET%-release --target clean 2>nul
    powershell -NoProfile -Command "Write-Host 'Clean complete.' -ForegroundColor Green"
)

:: --- Hard clean: delete build folder and exit ---
if !DO_HARD_CLEAN!==1 (
    call :header "Hard Clean - Removing build\%PRESET%"
    if exist "build\%PRESET%" (
        rmdir /s /q "build\%PRESET%"
        powershell -NoProfile -Command "Write-Host 'build\%PRESET% removed.' -ForegroundColor Green"
    ) else (
        powershell -NoProfile -Command "Write-Host 'build\%PRESET% does not exist.' -ForegroundColor Yellow"
    )
    goto :eof
)

:: --- Clean: cmake clean targets and exit ---
if !DO_CLEAN!==1 (
    call :header "Clean - %PRESET%"
    cmake --build --preset %PRESET%-debug --target clean
    cmake --build --preset %PRESET%-release --target clean
    powershell -NoProfile -Command "Write-Host 'Clean complete.' -ForegroundColor Green"
    goto :eof
)

:: Build the cmake configure command
set CMAKE_CONFIGURE=cmake --preset %PRESET%
if %RUN_TESTS%==1 (
    set CMAKE_CONFIGURE=%CMAKE_CONFIGURE% -DDSQT_BUILD_TESTS=ON
)
if %NO_PRIVATE_REINJECT%==1 (
    set CMAKE_CONFIGURE=%CMAKE_CONFIGURE% -DDSQT_TOUCH_NO_PRIVATE_REINJECT=ON
)
if defined QT_PATH (
    set CMAKE_CONFIGURE=!CMAKE_CONFIGURE! -DCMAKE_PREFIX_PATH="!QT_PATH!" -DQt6_DIR="!QT_PATH!/lib/cmake/Qt6"
)

:: Record overall start time
call :gettime OVERALL_START

:: --- Configure ---
set CONFIGURE_START=0
set CONFIGURE_END=0
if %SKIP_CONFIGURE%==1 (
    call :header "Skipping Configure (-no-configure)"
) else (
    call :header "Configuring DsQt Library (%PRESET%)"
    if %RUN_TESTS%==1       powershell -NoProfile -Command "Write-Host '    Tests:            ENABLED' -ForegroundColor Yellow"
    if %NO_PRIVATE_REINJECT%==1 powershell -NoProfile -Command "Write-Host '    Private reinject: DISABLED' -ForegroundColor Yellow"
    call :gettime CONFIGURE_START
    %CMAKE_CONFIGURE%
    if %errorlevel% neq 0 (
        powershell -NoProfile -Command "Write-Host 'Configuration failed.' -ForegroundColor Red"
        exit /b %errorlevel%
    )
    call :gettime CONFIGURE_END
)

:: --- Build Debug ---
echo.
call :header "Building Debug"
set DEBUG_NOOP=0
set BUILD_MARKER=build\%PRESET%\.build_marker
copy /y nul "!BUILD_MARKER!" >nul 2>&1
call :gettime BUILD_DEBUG_START
cmake --build --preset %PRESET%-debug
set BUILD_EXIT=!errorlevel!
call :gettime BUILD_DEBUG_END
if not "!BUILD_EXIT!"=="0" (
    powershell -NoProfile -Command "Write-Host 'Debug build failed.' -ForegroundColor Red"
    exit /b !BUILD_EXIT!
)
:: Check if any .lib files were updated since the marker
powershell -NoProfile -Command "if (-not (Get-ChildItem 'build\%PRESET%' -Recurse -Filter '*.lib' | Where-Object { $_.LastWriteTime -gt (Get-Item '!BUILD_MARKER!').LastWriteTime })) { exit 1 }"
if !errorlevel! neq 0 set DEBUG_NOOP=1

:: Run tests after Debug build (tests are Debug-only)
set TEST_SECS=0
set TEST_OUTPUT=build\%PRESET%\test-output.txt
if %RUN_TESTS%==1 (
    echo.
    call :header "Running Tests"
    call :gettime TEST_START
    ctest --test-dir build/%PRESET% --build-config Debug -V --output-junit build/%PRESET%/test-results.xml > "!TEST_OUTPUT!" 2>&1
    set TEST_EXIT=!errorlevel!
    call :gettime TEST_END
    powershell -NoProfile -Command ^
        "Get-Content '!TEST_OUTPUT!' | ForEach-Object {" ^
        "  if ($_ -match 'PASS\s+:')        { Write-Host $_ -ForegroundColor Green }" ^
        "  elseif ($_ -match 'FAIL')         { Write-Host $_ -ForegroundColor Red }" ^
        "  elseif ($_ -match 'SKIP')         { Write-Host $_ -ForegroundColor Yellow }" ^
        "  elseif ($_ -match 'Totals:')      { Write-Host $_ -ForegroundColor Cyan }" ^
        "  elseif ($_ -match '^\d+/\d+')     { Write-Host $_ -ForegroundColor Cyan }" ^
        "  elseif ($_ -match '\*{3,}')       { Write-Host $_ -ForegroundColor DarkGray }" ^
        "  elseif ($_ -match 'Config:')      { Write-Host $_ -ForegroundColor DarkGray }" ^
        "  elseif ($_ -match 'Test command') {} " ^
        "  elseif ($_ -match 'Test timeout') {} " ^
        "  else { Write-Host $_ }" ^
        "}"
    if not "!TEST_EXIT!"=="0" (
        echo.
        powershell -NoProfile -Command "Write-Host 'WARNING: Some tests failed (exit code !TEST_EXIT!). Continuing with build...' -ForegroundColor Yellow"
        echo.
    )
)

:: --- Build Release ---
echo.
call :header "Building Release"
set RELEASE_NOOP=0
copy /y nul "!BUILD_MARKER!" >nul 2>&1
call :gettime BUILD_RELEASE_START
cmake --build --preset %PRESET%-release
set BUILD_EXIT=!errorlevel!
call :gettime BUILD_RELEASE_END
if not "!BUILD_EXIT!"=="0" (
    powershell -NoProfile -Command "Write-Host 'Release build failed.' -ForegroundColor Red"
    exit /b !BUILD_EXIT!
)
:: Check if any .lib files were updated since the marker
powershell -NoProfile -Command "if (-not (Get-ChildItem 'build\%PRESET%' -Recurse -Filter '*.lib' | Where-Object { $_.LastWriteTime -gt (Get-Item '!BUILD_MARKER!').LastWriteTime })) { exit 1 }"
if !errorlevel! neq 0 set RELEASE_NOOP=1

:: --- Install (skip if -no-install or both builds were no-ops) ---
set INSTALL_DEBUG_START=0
set INSTALL_DEBUG_END=0
set INSTALL_RELEASE_START=0
set INSTALL_RELEASE_END=0

if !SKIP_INSTALL!==1 (
    echo.
    call :header "Skipping Install (-no-install)"
    goto :timing
)

if !DEBUG_NOOP!==1 if !RELEASE_NOOP!==1 (
    echo.
    powershell -NoProfile -Command "Write-Host 'No changes detected - skipping install.' -ForegroundColor Yellow"
    goto :timing
)

echo.
call :header "Installing Debug"
call :gettime INSTALL_DEBUG_START
cmake --install build/%PRESET% --config Debug
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Write-Host 'Debug install failed.' -ForegroundColor Red"
    exit /b %errorlevel%
)
call :gettime INSTALL_DEBUG_END

echo.
call :header "Installing Release"
call :gettime INSTALL_RELEASE_START
cmake --install build/%PRESET% --config Release
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Write-Host 'Release install failed.' -ForegroundColor Red"
    exit /b %errorlevel%
)
call :gettime INSTALL_RELEASE_END

:: --- Build and Install Tools (ProjectCloner + ClonerSource) ---
if !BUILD_TOOLS!==1 (
    set TOOLS_DIR=%~dp0..\Tools\ProjectCloner
    set CLONER_SRC=%~dp0..\Examples\ClonerSource
    set TOOLS_BUILD=!TOOLS_DIR!\build\%PRESET%
    set INSTALL_PREFIX=%USERPROFILE%\Documents\DsQt

    echo.
    call :header "Configuring ProjectCloner"
    set TOOLS_CMAKE=cmake -S "!TOOLS_DIR!" -B "!TOOLS_BUILD!" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="!INSTALL_PREFIX!\Tools\ProjectCloner"
    if defined QT_PATH (
        set TOOLS_CMAKE=!TOOLS_CMAKE! -DCMAKE_PREFIX_PATH="!QT_PATH!"
    )
    !TOOLS_CMAKE!
    if !errorlevel! neq 0 (
        powershell -NoProfile -Command "Write-Host 'ProjectCloner configure failed.' -ForegroundColor Red"
        exit /b !errorlevel!
    )

    echo.
    call :header "Building ProjectCloner"
    cmake --build "!TOOLS_BUILD!" --config Release
    if !errorlevel! neq 0 (
        powershell -NoProfile -Command "Write-Host 'ProjectCloner build failed.' -ForegroundColor Red"
        exit /b !errorlevel!
    )

    echo.
    call :header "Installing ProjectCloner"
    cmake --install "!TOOLS_BUILD!" --config Release
    if !errorlevel! neq 0 (
        powershell -NoProfile -Command "Write-Host 'ProjectCloner install failed.' -ForegroundColor Red"
        exit /b !errorlevel!
    )

    echo.
    call :header "Copying ClonerSource template"
    if exist "!INSTALL_PREFIX!\Tools\ClonerSource" rmdir /s /q "!INSTALL_PREFIX!\Tools\ClonerSource"
    xcopy "!CLONER_SRC!" "!INSTALL_PREFIX!\Tools\ClonerSource\" /E /I /Q /Y >nul
    if !errorlevel! neq 0 (
        powershell -NoProfile -Command "Write-Host 'ClonerSource copy failed.' -ForegroundColor Red"
        exit /b !errorlevel!
    )
    powershell -NoProfile -Command "Write-Host 'Tools installed to: !INSTALL_PREFIX!\Tools' -ForegroundColor Green"
)

:timing
call :gettime OVERALL_END

:: --- Timing summary ---
call :elapsed !CONFIGURE_START!       !CONFIGURE_END!       CONFIGURE_SECS
call :elapsed !BUILD_DEBUG_START!     !BUILD_DEBUG_END!     BUILD_DEBUG_SECS
call :elapsed !BUILD_RELEASE_START!   !BUILD_RELEASE_END!   BUILD_RELEASE_SECS
call :elapsed !INSTALL_DEBUG_START!   !INSTALL_DEBUG_END!   INSTALL_DEBUG_SECS
call :elapsed !INSTALL_RELEASE_START! !INSTALL_RELEASE_END! INSTALL_RELEASE_SECS
call :elapsed !OVERALL_START!         !OVERALL_END!         OVERALL_SECS

set /a TOTAL_BUILD_SECS   = BUILD_DEBUG_SECS   + BUILD_RELEASE_SECS
set /a TOTAL_INSTALL_SECS = INSTALL_DEBUG_SECS + INSTALL_RELEASE_SECS

call :fmt %CONFIGURE_SECS%       CONFIGURE_FMT
call :fmt %BUILD_DEBUG_SECS%     BUILD_DEBUG_FMT
call :fmt %BUILD_RELEASE_SECS%   BUILD_RELEASE_FMT
call :fmt %INSTALL_DEBUG_SECS%   INSTALL_DEBUG_FMT
call :fmt %INSTALL_RELEASE_SECS% INSTALL_RELEASE_FMT
call :fmt %TOTAL_BUILD_SECS%     TOTAL_BUILD_FMT
call :fmt %TOTAL_INSTALL_SECS%   TOTAL_INSTALL_FMT
call :fmt %OVERALL_SECS%         OVERALL_FMT

echo.
call :header "Timing Summary"
if !SKIP_CONFIGURE!==0 echo  Configure time      : %CONFIGURE_FMT%
echo  Debug build time    : %BUILD_DEBUG_FMT%
echo  Release build time  : %BUILD_RELEASE_FMT%
echo  Total build time    : %TOTAL_BUILD_FMT%
if !RUN_TESTS!==1 (
    call :elapsed !TEST_START! !TEST_END! TEST_SECS
    call :fmt !TEST_SECS! TEST_FMT
    echo  Test time           : !TEST_FMT!
)
if !SKIP_INSTALL!==0 if not "!INSTALL_DEBUG_END!"=="0" (
    echo  Debug install time  : %INSTALL_DEBUG_FMT%
    echo  Release install time: %INSTALL_RELEASE_FMT%
    echo  Total install time  : %TOTAL_INSTALL_FMT%
)
echo  Overall time        : %OVERALL_FMT%
echo.
powershell -NoProfile -Command "Write-Host 'Done.' -ForegroundColor Green"
goto :eof

:: Print a colored section header
:header
powershell -NoProfile -Command "Write-Host ('=== ' + '%~1' + ' ===') -ForegroundColor Cyan"
goto :eof

:: Get current time as integer seconds since midnight -> result var
:: The 1%%x-100 trick forces decimal parsing (avoids octal on 08, 09)
:gettime
for /f "tokens=1-3 delims=:." %%a in ("%TIME: =0%") do set /a %1=(1%%a-100)*3600 + (1%%b-100)*60 + (1%%c-100)
goto :eof

:: Compute elapsed seconds between two integer timestamps -> result var
:elapsed
setlocal
set /a DIFF=%~2 - %~1
if %DIFF% lss 0 set /a DIFF+=86400
endlocal & set %~3=%DIFF%
goto :eof

:: Format seconds as Xm Ys -> result var
:fmt
setlocal
set /a M = %~1 / 60
set /a S = %~1 %% 60
set RESULT=%M%m %S%s
endlocal & set %~2=%RESULT%
goto :eof
