@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo   Qt 6 - Environment Setup
echo =======================================================
echo.

:: ─────────────────────────────────────────────────────────
:: Configuration
::   Override any of these by setting the variable before
::   calling this script, e.g.:
::     set QT_VERSION=6.9.0
::     call setup_qt6.bat
:: ─────────────────────────────────────────────────────────
if not defined QT_ROOT     set "QT_ROOT=C:\Qt"
if not defined QT_VERSION  set "QT_VERSION=6.10.2"
if not defined QT_COMPILER set "QT_COMPILER=win64_msvc2022_64"
if not defined QT_MODULES  set "QT_MODULES=qthttpserver qtimageformats qtmultimedia qtnetworkauth qtshadertools qtvirtualkeyboard qtwebsockets"

set "QT_INSTALL_DIR=%QT_ROOT%\%QT_VERSION%\%QT_COMPILER%"

:: ─────────────────────────────────────────────────────────
:: STEP 1 – Check if Qt 6 is already installed
::
:: First try the well-known install path directly, then fall
:: back to asking CMake (requires cmake on PATH).
:: ─────────────────────────────────────────────────────────
if exist "%QT_INSTALL_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [OK] Qt %QT_VERSION% already installed at:
    echo      %QT_INSTALL_DIR%
    goto :set_env
)

:: Probe via CMake if the direct path check didn't find it
where cmake.exe >nul 2>&1
if not errorlevel 1 (
    cmake -P "%~dp0application\cmake\qt_probe.cmake" >nul 2>&1
    if not errorlevel 1 (
        echo [OK] Qt 6 detected via CMake probe.
        goto :done
    )
)

echo [INFO] Qt %QT_VERSION% was not found on this machine.
echo.

:: ─────────────────────────────────────────────────────────
:: STEP 2 – Ask how to install
:: ─────────────────────────────────────────────────────────
echo How would you like to proceed?
echo   [A] Install automatically via aqtinstall (lightweight, no Qt account needed)
echo   [Q] Open Qt's official installer (full IDE and tools, requires a Qt account)
echo   [C] Cancel
echo.
choice /C AQC /N /M "Your choice: "
if errorlevel 3 (
    echo [INFO] Cancelled.
    exit /b 1
)
if errorlevel 2 goto :install_qt_official
if errorlevel 1 goto :install_qt_aqt

:: ─────────────────────────────────────────────────────────
:: Option Q – open Qt's official download page
:: ─────────────────────────────────────────────────────────
:install_qt_official
echo.
echo [INFO] Opening the Qt Online Installer download page...
start https://www.qt.io/download-qt-installer
echo.
echo Please install Qt %QT_VERSION% with the "%QT_COMPILER%" kit,
echo then rerun this script.
exit /b 1

:: ─────────────────────────────────────────────────────────
:: Option A – install automatically via aqtinstall
:: ─────────────────────────────────────────────────────────
:install_qt_aqt

:: Ensure Python is available
echo [INFO] Checking for Python...
python --version >nul 2>&1
if errorlevel 1 (
    echo [INFO] Python not found. Installing via winget...

    where winget >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] winget is not available.
        echo         Install Python from https://www.python.org/downloads/ and re-run.
        exit /b 1
    )

    winget install -e --id Python.Python.3.12 ^
        --accept-package-agreements ^
        --accept-source-agreements
    if errorlevel 1 (
        echo [ERROR] Failed to install Python.
        exit /b 1
    )

    :: Refresh PATH from registry so python.exe is visible in this session
    for /f "usebackq tokens=2*" %%A in (
        `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PATH 2^>nul`
    ) do set "SYS_PATH=%%B"
    for /f "usebackq tokens=2*" %%A in (
        `reg query "HKCU\Environment" /v PATH 2^>nul`
    ) do set "USR_PATH=%%B"
    set "PATH=!SYS_PATH!;!USR_PATH!"

    python --version >nul 2>&1
    if errorlevel 1 (
        echo [WARN] Python still not on PATH after install.
        echo        Open a new terminal and re-run this script.
        exit /b 1
    )
)

for /f "delims=" %%V in ('python --version 2^>^&1') do echo [OK] %%V

:: Install aqtinstall
echo.
echo [INFO] Installing aqtinstall...
pip install aqtinstall --quiet --upgrade
if errorlevel 1 (
    echo [ERROR] Failed to install aqtinstall.
    exit /b 1
)

:: Install Qt base + requested modules
echo.
echo [INFO] Installing Qt %QT_VERSION% [%QT_COMPILER%]...
echo        Output directory : %QT_ROOT%
echo        Modules          : %QT_MODULES%
echo.
aqt install-qt windows desktop %QT_VERSION% %QT_COMPILER% --outputdir "%QT_ROOT%" -m %QT_MODULES%
if errorlevel 1 (
    echo [ERROR] Qt installation via aqtinstall failed.
    echo         Check the output above for details, or try the official installer:
    echo         https://www.qt.io/download-qt-installer
    exit /b 1
)

echo.
echo [OK] Qt %QT_VERSION% installed successfully.

:set_env
:: ─────────────────────────────────────────────────────────
:: STEP 3 – Expose Qt to CMake via CMAKE_PREFIX_PATH
::
:: Setting CMAKE_PREFIX_PATH lets find_package(Qt6) work
:: without any extra hints in CMakeLists.txt.
:: ─────────────────────────────────────────────────────────
if not exist "%QT_INSTALL_DIR%" (
    echo [WARN] Expected Qt install dir not found: %QT_INSTALL_DIR%
    echo        CMAKE_PREFIX_PATH will not be set automatically.
    goto :done
)

:: Extend CMAKE_PREFIX_PATH for the current session
if defined CMAKE_PREFIX_PATH (
    set "CMAKE_PREFIX_PATH=%QT_INSTALL_DIR%;!CMAKE_PREFIX_PATH!"
) else (
    set "CMAKE_PREFIX_PATH=%QT_INSTALL_DIR%"
)

echo [OK] CMAKE_PREFIX_PATH set to: !CMAKE_PREFIX_PATH!

:done
echo.
echo =======================================================
echo   Qt %QT_VERSION% is ready.
echo   Install dir : %QT_INSTALL_DIR%
echo =======================================================
echo.
