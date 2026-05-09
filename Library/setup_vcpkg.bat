@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo   vcpkg - Environment Setup
echo =======================================================
echo.

:: ─────────────────────────────────────────────────────────
:: STEP 1 – Accept an override install root as first argument
::           Usage: setup_vcpkg.bat [path]
::           Default: C:\vcpkg
:: ─────────────────────────────────────────────────────────
set "VCPKG_INSTALL_ROOT=C:\vcpkg"
if not "%~1"=="" set "VCPKG_INSTALL_ROOT=%~1"

:: ─────────────────────────────────────────────────────────
:: STEP 2 – Check if VCPKG_ROOT is already valid
:: ─────────────────────────────────────────────────────────
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo [OK] VCPKG_ROOT is already set and valid:
        echo      %VCPKG_ROOT%
        goto :done
    ) else (
        echo [WARN] VCPKG_ROOT is set but vcpkg.exe was not found there:
        echo        %VCPKG_ROOT%
        echo        Will search for a valid installation...
        echo.
    )
)

:: ─────────────────────────────────────────────────────────
:: STEP 3 – Search well-known locations
:: ─────────────────────────────────────────────────────────
set "FOUND_ROOT="

for %%P in (
    "C:\vcpkg"
    "%USERPROFILE%\vcpkg"
    "%LOCALAPPDATA%\vcpkg"
    "%PROGRAMFILES%\vcpkg"
    "%PROGRAMFILES(X86)%\vcpkg"
) do (
    if not defined FOUND_ROOT (
        if exist "%%~P\vcpkg.exe" (
            set "FOUND_ROOT=%%~P"
        )
    )
)

:: Also check if vcpkg.exe is already on PATH
if not defined FOUND_ROOT (
    for /f "delims=" %%X in ('where vcpkg.exe 2^>nul') do (
        if not defined FOUND_ROOT (
            set "_W=%%~dpX"
            :: Strip trailing backslash
            if "!_W:~-1!"=="\" set "_W=!_W:~0,-1!"
            set "FOUND_ROOT=!_W!"
        )
    )
)

if defined FOUND_ROOT (
    echo [INFO] Found existing vcpkg installation at:
    echo        !FOUND_ROOT!
    set "VCPKG_INSTALL_ROOT=!FOUND_ROOT!"
    goto :set_root
)

:: ─────────────────────────────────────────────────────────
:: STEP 4 – Ensure git is available (install via winget if not)
:: ─────────────────────────────────────────────────────────
echo [INFO] vcpkg not found. Will clone from GitHub to %VCPKG_INSTALL_ROOT%...
echo.

where git >nul 2>&1
if errorlevel 1 (
    echo [INFO] git not found. Installing Git via winget...

    where winget >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Neither git nor winget is available.
        echo         Install Git from https://git-scm.com/download/win and re-run this script.
        exit /b 1
    )

    winget install ^
        --id Git.Git ^
        --silent ^
        --accept-package-agreements ^
        --accept-source-agreements

    if errorlevel 1 (
        echo [ERROR] winget failed to install Git.
        echo         Install Git manually from https://git-scm.com/download/win
        exit /b 1
    )

    :: Refresh PATH so git.exe is visible in this session
    for /f "usebackq tokens=2*" %%A in (
        `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PATH 2^>nul`
    ) do set "PATH=%%B;%PATH%"
    for /f "usebackq tokens=2*" %%A in (
        `reg query "HKCU\Environment" /v PATH 2^>nul`
    ) do set "PATH=%%B;%PATH%"

    where git >nul 2>&1
    if errorlevel 1 (
        echo [WARN] git still not on PATH after install.
        echo        Open a new terminal and re-run this script, or add Git to PATH manually.
        exit /b 1
    )

    echo [OK] Git installed successfully.
    echo.
)

:: ─────────────────────────────────────────────────────────
:: STEP 5 – Clone vcpkg from GitHub
:: ─────────────────────────────────────────────────────────
echo [INFO] Cloning vcpkg into %VCPKG_INSTALL_ROOT%...
git clone https://github.com/microsoft/vcpkg.git "%VCPKG_INSTALL_ROOT%"
if errorlevel 1 (
    echo [ERROR] git clone failed.
    echo         Check your internet connection and that %VCPKG_INSTALL_ROOT% is writable.
    exit /b 1
)

:: ─────────────────────────────────────────────────────────
:: STEP 6 – Bootstrap to compile vcpkg.exe
:: ─────────────────────────────────────────────────────────
echo.
echo [INFO] Bootstrapping vcpkg (compiling vcpkg.exe)...
call "%VCPKG_INSTALL_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
if errorlevel 1 (
    echo [ERROR] bootstrap-vcpkg.bat failed.
    exit /b 1
)

:: ─────────────────────────────────────────────────────────
:: STEP 7 – Final verification
:: ─────────────────────────────────────────────────────────
if not exist "%VCPKG_INSTALL_ROOT%\vcpkg.exe" (
    echo [ERROR] vcpkg.exe still not found at %VCPKG_INSTALL_ROOT%.
    echo         Please check the install log above for errors.
    exit /b 1
)

echo [OK] vcpkg installed successfully at %VCPKG_INSTALL_ROOT%.

:set_root
:: ─────────────────────────────────────────────────────────
:: STEP 8 – Set VCPKG_ROOT for this session and persistently
:: ─────────────────────────────────────────────────────────
set "VCPKG_ROOT=%VCPKG_INSTALL_ROOT%"

echo.
echo [INFO] Setting VCPKG_ROOT=%VCPKG_INSTALL_ROOT%

:: Persist to the current user's environment (no admin required)
setx VCPKG_ROOT "%VCPKG_INSTALL_ROOT%" >nul
if errorlevel 1 (
    echo [WARN] setx failed — VCPKG_ROOT is set for this session only.
) else (
    echo [OK] VCPKG_ROOT persisted to user environment ^(new shells will inherit it^).
)

:done
echo.
echo =======================================================
echo   vcpkg is ready.
echo   VCPKG_ROOT = %VCPKG_ROOT%
echo =======================================================
echo.
