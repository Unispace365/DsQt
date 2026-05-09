@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo   CMake - Environment Setup
echo =======================================================
echo.

:: ─────────────────────────────────────────────────────────
:: STEP 1 – Check if cmake is already on PATH
:: ─────────────────────────────────────────────────────────
set "CMAKE_BIN="

where cmake.exe >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%X in ('where cmake.exe') do (
        if not defined CMAKE_BIN set "CMAKE_BIN=%%~dpX"
    )
    :: Strip trailing backslash
    if "!CMAKE_BIN:~-1!"=="\" set "CMAKE_BIN=!CMAKE_BIN:~0,-1!"

    for /f "delims=" %%V in ('cmake --version 2^>nul ^| findstr /i "cmake version"') do (
        echo [OK] cmake is already on PATH ^(%%V^)
    )
    echo      Location: !CMAKE_BIN!
    goto :done
)

:: ─────────────────────────────────────────────────────────
:: STEP 2 – Search well-known install locations
:: ─────────────────────────────────────────────────────────
echo [INFO] cmake not found on PATH. Searching known locations...

for %%P in (
    "%PROGRAMFILES%\CMake\bin"
    "%PROGRAMFILES(X86)%\CMake\bin"
    "%LOCALAPPDATA%\Programs\CMake\bin"
) do (
    if not defined CMAKE_BIN (
        if exist "%%~P\cmake.exe" set "CMAKE_BIN=%%~P"
    )
)

if defined CMAKE_BIN (
    echo [INFO] Found cmake at: !CMAKE_BIN!
    echo        Adding to PATH for this session...
    set "PATH=!CMAKE_BIN!;%PATH%"
    goto :persist_path
)

:: ─────────────────────────────────────────────────────────
:: STEP 3 – Install via winget
:: ─────────────────────────────────────────────────────────
echo [INFO] cmake not found. Installing via winget...
echo.

where winget >nul 2>&1
if errorlevel 1 (
    echo [ERROR] winget is not available.
    echo         Install "App Installer" from the Microsoft Store, or download CMake from:
    echo         https://cmake.org/download/
    exit /b 1
)

winget install ^
    --id Kitware.CMake ^
    --silent ^
    --accept-package-agreements ^
    --accept-source-agreements

if errorlevel 1 (
    echo [ERROR] winget failed to install CMake.
    echo         Download CMake manually from https://cmake.org/download/
    exit /b 1
)

echo.
echo [OK] CMake installed. Locating cmake.exe...

:: ─────────────────────────────────────────────────────────
:: STEP 4 – Find cmake.exe after install
::          (winget silent install may not update PATH in
::           the current session, so we search manually)
:: ─────────────────────────────────────────────────────────

:: Refresh PATH from registry first (picks up winget changes)
for /f "usebackq tokens=2*" %%A in (
    `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PATH 2^>nul`
) do set "SYS_PATH=%%B"
for /f "usebackq tokens=2*" %%A in (
    `reg query "HKCU\Environment" /v PATH 2^>nul`
) do set "USR_PATH=%%B"
set "PATH=!SYS_PATH!;!USR_PATH!"

:: Check PATH first after refresh
where cmake.exe >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%X in ('where cmake.exe') do (
        if not defined CMAKE_BIN set "CMAKE_BIN=%%~dpX"
    )
    if "!CMAKE_BIN:~-1!"=="\" set "CMAKE_BIN=!CMAKE_BIN:~0,-1!"
)

:: Fall back to known locations
if not defined CMAKE_BIN (
    for %%P in (
        "%PROGRAMFILES%\CMake\bin"
        "%PROGRAMFILES(X86)%\CMake\bin"
        "%LOCALAPPDATA%\Programs\CMake\bin"
    ) do (
        if not defined CMAKE_BIN (
            if exist "%%~P\cmake.exe" set "CMAKE_BIN=%%~P"
        )
    )
)

if not defined CMAKE_BIN (
    echo [ERROR] cmake.exe not found after installation.
    echo         Try opening a new terminal — the installer may have updated PATH.
    exit /b 1
)

echo [OK] cmake.exe found at: !CMAKE_BIN!
set "PATH=!CMAKE_BIN!;%PATH%"

:persist_path
:: ─────────────────────────────────────────────────────────
:: STEP 5 – Persist cmake\bin to the user PATH (if not there)
:: ─────────────────────────────────────────────────────────
echo.

:: Check whether the path is already in the persistent user PATH
for /f "usebackq tokens=2*" %%A in (
    `reg query "HKCU\Environment" /v PATH 2^>nul`
) do set "_UPATH=%%B"

echo !_UPATH! | findstr /i /c:"!CMAKE_BIN!" >nul 2>&1
if not errorlevel 1 (
    echo [OK] cmake\bin is already in the persistent user PATH.
    goto :done
)

:: Append to user PATH via setx
setx PATH "!CMAKE_BIN!;!_UPATH!" >nul
if errorlevel 1 (
    echo [WARN] setx failed — cmake is on PATH for this session only.
) else (
    echo [OK] cmake\bin persisted to user PATH ^(new shells will inherit it^).
)

:done
echo.
for /f "delims=" %%V in ('cmake --version 2^>nul ^| findstr /i "cmake version"') do echo [INFO] %%V
echo =======================================================
echo   CMake is ready.
echo =======================================================
echo.
