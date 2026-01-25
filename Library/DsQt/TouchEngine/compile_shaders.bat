@echo off
REM Compile shaders to Qt Shader Baker (.qsb) format
REM Requires qsb tool from Qt installation

echo Compiling Qt RHI shaders...

set QT_DIR=C:\Qt\6.9.2\msvc2022_64
set QSB=%QT_DIR%\bin\qsb.exe

if not exist "%QSB%" (
    echo ERROR: qsb.exe not found at %QSB%
    echo Please update QT_DIR path in this script
    pause
    exit /b 1
)

set SHADER_DIR=%~dp0resources\shaders

if not exist "%SHADER_DIR%" mkdir "%SHADER_DIR%"

echo Compiling vertex shader...
"%QSB%" --glsl "100 es,120,150" --hlsl 50 --msl 12 ^
    -o "%SHADER_DIR%\texture.vert.qsb" ^
    "%SHADER_DIR%\texture.vert"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile vertex shader
    pause
    exit /b 1
)

echo Compiling fragment shader...
"%QSB%" --glsl "100 es,120,150" --hlsl 50 --msl 12 ^
    -o "%SHADER_DIR%\texture.frag.qsb" ^
    "%SHADER_DIR%\texture.frag"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile fragment shader
    pause
    exit /b 1
)

echo.
echo Shaders compiled successfully!
echo Output files:
echo   - texture.vert.qsb
echo   - texture.frag.qsb
echo.
pause