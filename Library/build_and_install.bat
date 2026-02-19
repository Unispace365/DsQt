@echo off
setlocal

set PRESET=vs2022-6.10.1

:: Record overall start time
set OVERALL_START=%time%

echo === Configuring DsQt Library (%PRESET%) ===
set CONFIGURE_START=%time%
cmake --preset %PRESET%
if %errorlevel% neq 0 (
    echo Configuration failed.
    exit /b %errorlevel%
)
set CONFIGURE_END=%time%

echo.
echo === Building Debug ===
set BUILD_DEBUG_START=%time%
cmake --build --preset %PRESET%-debug
if %errorlevel% neq 0 (
    echo Debug build failed.
    exit /b %errorlevel%
)
set BUILD_DEBUG_END=%time%

echo.
echo === Building Release ===
set BUILD_RELEASE_START=%time%
cmake --build --preset %PRESET%-release
if %errorlevel% neq 0 (
    echo Release build failed.
    exit /b %errorlevel%
)
set BUILD_RELEASE_END=%time%

echo.
echo === Installing Debug ===
set INSTALL_DEBUG_START=%time%
cmake --install build/%PRESET% --config Debug
if %errorlevel% neq 0 (
    echo Debug install failed.
    exit /b %errorlevel%
)
set INSTALL_DEBUG_END=%time%

echo.
echo === Installing Release ===
set INSTALL_RELEASE_START=%time%
cmake --install build/%PRESET% --config Release
if %errorlevel% neq 0 (
    echo Release install failed.
    exit /b %errorlevel%
)
set INSTALL_RELEASE_END=%time%

set OVERALL_END=%time%

:: --- Timing summary ---
call :elapsed %CONFIGURE_START%     %CONFIGURE_END%     CONFIGURE_SECS
call :elapsed %BUILD_DEBUG_START%   %BUILD_DEBUG_END%   BUILD_DEBUG_SECS
call :elapsed %BUILD_RELEASE_START% %BUILD_RELEASE_END% BUILD_RELEASE_SECS
call :elapsed %INSTALL_DEBUG_START% %INSTALL_DEBUG_END% INSTALL_DEBUG_SECS
call :elapsed %INSTALL_RELEASE_START% %INSTALL_RELEASE_END% INSTALL_RELEASE_SECS
call :elapsed %OVERALL_START% %OVERALL_END% OVERALL_SECS

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

:: Compute elapsed seconds between two %time% values -> result var
:elapsed
setlocal
set T1=%~1
set T2=%~2
:: Parse HH:MM:SS.cs
for /f "tokens=1-4 delims=:,. " %%a in ("%T1%") do (
    set /a S1 = (%%a*3600) + (%%b*60) + %%c
)
for /f "tokens=1-4 delims=:,. " %%a in ("%T2%") do (
    set /a S2 = (%%a*3600) + (%%b*60) + %%c
)
set /a DIFF = S2 - S1
if %DIFF% lss 0 set /a DIFF += 86400
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
