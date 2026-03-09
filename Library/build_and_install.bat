@echo off
setlocal enabledelayedexpansion

REM Usage: build_and_install.bat [preset] [-test] [-no-configure] [-no-install]
REM   preset        : CMake configure preset name (default: ninja)
REM   -test         : Enable and run unit tests after the Debug build
REM   -no-configure : Skip the CMake configure step (for fast incremental builds)
REM   -no-install   : Skip the install steps entirely

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

:: Parse arguments — accept preset and flags in any order
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="-test"          (set RUN_TESTS=1& shift & goto :parse_args)
if /i "%~1"=="/test"          (set RUN_TESTS=1& shift & goto :parse_args)
if /i "%~1"=="-no-configure"  (set SKIP_CONFIGURE=1& shift & goto :parse_args)
if /i "%~1"=="/no-configure"  (set SKIP_CONFIGURE=1& shift & goto :parse_args)
if /i "%~1"=="-no-install"    (set SKIP_INSTALL=1& shift & goto :parse_args)
if /i "%~1"=="/no-install"    (set SKIP_INSTALL=1& shift & goto :parse_args)
set PRESET=%~1
shift
goto :parse_args
:args_done

:: Build the cmake configure command
set CMAKE_CONFIGURE=cmake --preset %PRESET%
if %RUN_TESTS%==1 (
    set CMAKE_CONFIGURE=%CMAKE_CONFIGURE% -DDSQT_BUILD_TESTS=ON
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
    if %RUN_TESTS%==1 powershell -NoProfile -Command "Write-Host '    Tests: ENABLED' -ForegroundColor Yellow"
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
set BUILD_DEBUG_OUTPUT=build\%PRESET%\build-debug-output.txt
call :gettime BUILD_DEBUG_START
cmake --build --preset %PRESET%-debug > "!BUILD_DEBUG_OUTPUT!" 2>&1
set BUILD_EXIT=!errorlevel!
call :gettime BUILD_DEBUG_END
type "!BUILD_DEBUG_OUTPUT!"
if not "!BUILD_EXIT!"=="0" (
    powershell -NoProfile -Command "Write-Host 'Debug build failed.' -ForegroundColor Red"
    exit /b !BUILD_EXIT!
)
:: If no Compiling/Linking lines, only AUTOMOC ran — treat as no-op
findstr /r /c:"Compiling" /c:"Linking" /c:"Generating" "!BUILD_DEBUG_OUTPUT!" >nul 2>&1
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
set BUILD_RELEASE_OUTPUT=build\%PRESET%\build-release-output.txt
call :gettime BUILD_RELEASE_START
cmake --build --preset %PRESET%-release > "!BUILD_RELEASE_OUTPUT!" 2>&1
set BUILD_EXIT=!errorlevel!
call :gettime BUILD_RELEASE_END
type "!BUILD_RELEASE_OUTPUT!"
if not "!BUILD_EXIT!"=="0" (
    powershell -NoProfile -Command "Write-Host 'Release build failed.' -ForegroundColor Red"
    exit /b !BUILD_EXIT!
)
:: If no Compiling/Linking lines, only AUTOMOC ran — treat as no-op
findstr /r /c:"Compiling" /c:"Linking" /c:"Generating" "!BUILD_RELEASE_OUTPUT!" >nul 2>&1
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
