@echo off
setlocal

set PRESET=vs2022-6.10.1

echo === Configuring DsQt Library (%PRESET%) ===
cmake --preset %PRESET%
if %errorlevel% neq 0 (
    echo Configuration failed.
    exit /b %errorlevel%
)

echo.
echo === Building Debug ===
cmake --build --preset %PRESET%-debug
if %errorlevel% neq 0 (
    echo Debug build failed.
    exit /b %errorlevel%
)

echo.
echo === Building Release ===
cmake --build --preset %PRESET%-release
if %errorlevel% neq 0 (
    echo Release build failed.
    exit /b %errorlevel%
)

echo.
echo === Installing Debug ===
cmake --install build/%PRESET% --config Debug
if %errorlevel% neq 0 (
    echo Debug install failed.
    exit /b %errorlevel%
)

echo.
echo === Installing Release ===
cmake --install build/%PRESET% --config Release
if %errorlevel% neq 0 (
    echo Release install failed.
    exit /b %errorlevel%
)

echo.
echo === Done. Installed to %USERPROFILE%\Documents\DsQt ===
