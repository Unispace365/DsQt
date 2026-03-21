@echo off
setlocal

cd /d "%~dp0"

set "ISS_FILE=ClonerSource_dev.iss"

if /i "%~1"=="--production" set "ISS_FILE=ClonerSource.iss"
if /i "%~1"=="-p" set "ISS_FILE=ClonerSource.iss"

REM --- Find ISCC.exe ---
set "ISCC="
where iscc >nul 2>&1 && set "ISCC=iscc"
if not defined ISCC (
    for %%V in (6 5) do (
        if exist "%ProgramFiles(x86)%\Inno Setup %%V\ISCC.exe" (
            set "ISCC=%ProgramFiles(x86)%\Inno Setup %%V\ISCC.exe"
        )
        if exist "%ProgramFiles%\Inno Setup %%V\ISCC.exe" (
            set "ISCC=%ProgramFiles%\Inno Setup %%V\ISCC.exe"
        )
    )
)
if not defined ISCC (
    echo Error: ISCC.exe not found. Please install Inno Setup or add it to your PATH.
    exit /b 1
)

echo === Downloading dependencies ===
call get_deps.bat
if errorlevel 1 (
    echo Failed to get dependencies.
    exit /b 1
)

echo.
echo === Compiling installer: %ISS_FILE% ===
"%ISCC%" "%ISS_FILE%"
if errorlevel 1 (
    echo Installer compilation failed.
    exit /b 1
)

echo.
echo === Installer built successfully ===
echo Output: build\
