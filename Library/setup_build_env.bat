@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo   Visual Studio Build Tools - Environment Setup
echo =======================================================
echo.

:: ─────────────────────────────────────────────────────────
:: STEP 1 – Search for vcvars64.bat across all known paths
:: ─────────────────────────────────────────────────────────
set "VCVARS="

for %%Y in (2022 2019 2017) do (
    for %%E in (BuildTools Enterprise Professional Community Preview) do (

        :: Program Files (x86)  — traditional install root
        set "_C=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!_C!" if not defined VCVARS set "VCVARS=!_C!"

        :: Program Files        — newer/ARM64 host installs
        set "_C=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!_C!" if not defined VCVARS set "VCVARS=!_C!"
    )
)

:: ─────────────────────────────────────────────────────────
:: STEP 2 – Install via winget if not found
:: ─────────────────────────────────────────────────────────
if not defined VCVARS (
    echo [INFO] Visual Studio Build Tools not detected on this machine.
    echo [INFO] Checking for winget...
    echo.

    where winget >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] winget is not available.
        echo         Install "App Installer" from the Microsoft Store and re-run this script.
        exit /b 1
    )

    :: Try newest edition first, fall back to 2019
    set "INSTALLED="
    for %%V in (2022 2019) do (
        if not defined INSTALLED (
            echo [INFO] Installing Microsoft.VisualStudio.%%V.BuildTools via winget...
            winget install ^
                --id Microsoft.VisualStudio.%%V.BuildTools ^
                --silent ^
                --accept-package-agreements ^
                --accept-source-agreements ^
                --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

            if not errorlevel 1 (
                set "INSTALLED=%%V"
                echo [OK] Visual Studio %%V Build Tools installed successfully.
            ) else (
                echo [WARN] Install of %%V Build Tools failed – trying older version...
            )
        )
    )

    if not defined INSTALLED (
        echo [ERROR] All winget install attempts failed.
        echo         Please install Visual Studio Build Tools manually from:
        echo         https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio
        exit /b 1
    )

    echo.
    echo [INFO] Searching for vcvars64.bat after install...
    echo.

    :: Search again — same logic as Step 1
    for %%Y in (2022 2019 2017) do (
        for %%E in (BuildTools Enterprise Professional Community Preview) do (
            set "_C=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
            if exist "!_C!" if not defined VCVARS set "VCVARS=!_C!"

            set "_C=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
            if exist "!_C!" if not defined VCVARS set "VCVARS=!_C!"
        )
    )
)

:: ─────────────────────────────────────────────────────────
:: STEP 3 – Bail out if vcvars64.bat still cannot be found
:: ─────────────────────────────────────────────────────────
if not defined VCVARS (
    echo [ERROR] vcvars64.bat was not found even after installation.
    echo         Make sure the "Desktop development with C++" workload is installed.
    exit /b 1
)

:: ─────────────────────────────────────────────────────────
:: STEP 4 – Initialise the 64-bit build environment
:: ─────────────────────────────────────────────────────────
echo [INFO] Initialising 64-bit MSVC environment from:
echo        %VCVARS%
echo.

call "%VCVARS%"
if errorlevel 1 (
    echo [ERROR] vcvars64.bat returned a non-zero exit code.
    exit /b 1
)

echo.
echo =======================================================
echo   Environment ready.  cl.exe, MSBuild, link, etc.
echo   are now available on PATH for this shell session.
echo =======================================================
echo.

:: ─────────────────────────────────────────────────────────
:: Add your build commands below this line, e.g.:
::
::   MSBuild MyProject.sln /t:Build /p:Configuration=Release /p:Platform=x64
::   cl.exe /EHsc /O2 main.cpp
::
:: ─────────────────────────────────────────────────────────
