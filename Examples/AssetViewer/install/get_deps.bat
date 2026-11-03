@echo off
setlocal

cd /d "%~dp0"

echo === Installing Python dependencies ===
pip install -r requirements.txt
if errorlevel 1 (
    echo Failed to install Python dependencies.
    exit /b 1
)

echo.
echo === Downloading DSAppHost ===
python download_apphost.py %*
if errorlevel 1 (
    echo Failed to download DSAppHost.
    exit /b 1
)

echo.
echo === Downloading BridgeSync ===
python download_bridgesync.py %*
if errorlevel 1 (
    echo Failed to download BridgeSync.
    exit /b 1
)

echo.
echo === All dependencies downloaded ===
