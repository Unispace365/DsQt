@echo off
setlocal

REM Set up the MSVC developer environment if not already active
if not defined VSINSTALLDIR (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
    if %errorlevel% neq 0 (
        echo Failed to set up MSVC environment.
        exit /b %errorlevel%
    )
)

set PRESET=ninja

:: Record overall start time
call :gettime OVERALL_START

echo === Configuring DsQt Library (%PRESET%) ===
call :gettime CONFIGURE_START
cmake --preset %PRESET%
if %errorlevel% neq 0 (
    echo Configuration failed.
    exit /b %errorlevel%
)
call :gettime CONFIGURE_END

echo.
echo === Building Debug ===
call :gettime BUILD_DEBUG_START
cmake --build --preset %PRESET%-debug
if %errorlevel% neq 0 (
    echo Debug build failed.
    exit /b %errorlevel%
)
call :gettime BUILD_DEBUG_END

echo.
echo === Building Release ===
call :gettime BUILD_RELEASE_START
cmake --build --preset %PRESET%-release
if %errorlevel% neq 0 (
    echo Release build failed.
    exit /b %errorlevel%
)
call :gettime BUILD_RELEASE_END

echo.
echo === Installing Debug ===
call :gettime INSTALL_DEBUG_START
cmake --install build/%PRESET% --config Debug
if %errorlevel% neq 0 (
    echo Debug install failed.
    exit /b %errorlevel%
)
call :gettime INSTALL_DEBUG_END

echo.
echo === Installing Release ===
call :gettime INSTALL_RELEASE_START
cmake --install build/%PRESET% --config Release
if %errorlevel% neq 0 (
    echo Release install failed.
    exit /b %errorlevel%
)
call :gettime INSTALL_RELEASE_END

call :gettime OVERALL_END

:: --- Timing summary ---
call :elapsed %CONFIGURE_START%       %CONFIGURE_END%       CONFIGURE_SECS
call :elapsed %BUILD_DEBUG_START%     %BUILD_DEBUG_END%     BUILD_DEBUG_SECS
call :elapsed %BUILD_RELEASE_START%   %BUILD_RELEASE_END%   BUILD_RELEASE_SECS
call :elapsed %INSTALL_DEBUG_START%   %INSTALL_DEBUG_END%   INSTALL_DEBUG_SECS
call :elapsed %INSTALL_RELEASE_START% %INSTALL_RELEASE_END% INSTALL_RELEASE_SECS
call :elapsed %OVERALL_START%         %OVERALL_END%         OVERALL_SECS

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
echo ==========================================
echo  Timing Summary
echo ==========================================
echo  Configure time      : %CONFIGURE_FMT%
echo  Debug build time    : %BUILD_DEBUG_FMT%
echo  Release build time  : %BUILD_RELEASE_FMT%
echo  Total build time    : %TOTAL_BUILD_FMT%
echo  Debug install time  : %INSTALL_DEBUG_FMT%
echo  Release install time: %INSTALL_RELEASE_FMT%
echo  Total install time  : %TOTAL_INSTALL_FMT%
echo  Overall time        : %OVERALL_FMT%
echo ==========================================
echo.
echo Done. Installed to %USERPROFILE%\Documents\DsQt
goto :eof

:: Get current time as integer seconds since midnight -> result var
:gettime
for /f %%T in ('powershell -NoProfile -Command "[int](Get-Date).TimeOfDay.TotalSeconds"') do set %1=%%T
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
